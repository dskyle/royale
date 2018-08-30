#include <royale/Runner.hpp>

#include <boost/process.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>

namespace royale {

Experiment &Runner::add_experiment(Experiment e)
{
  auto log = spdlog::get("log");

  std::string name = e.name();

  log->info("Runner::add_experiment: adding \"{}\"", name);
  SPDLOG_TRACE(log, "   Experiment \"{}\": {}", name, xtd::lazy_json_dump(e));

  if (name == "") {
    throw std::runtime_error("Can't add experiment without name");
  }

  auto ret = experiments_.emplace(std::piecewise_construct,
      std::forward_as_tuple(std::move(name)),
      std::forward_as_tuple(std::make_unique<Experiment>(std::move(e))));

  if (!ret.second) {
    throw std::runtime_error("Experiment already added");
  }

  return *ret.first->second;
}

void Runner::send_message(stream_type &stream, Message::Enum message,
    io::yield_context yield)
{
  auto log = spdlog::get("log");

  std::string buf = json(message).dump();
  SPDLOG_DEBUG(log, "Sending message {}", xtd::lazy_json_dump(message));
  stream.async_write(io::buffer(buf), yield);
  SPDLOG_TRACE(log, "Message sent");
}

Message::Enum Runner::get_message(stream_type &stream, io::yield_context yield)
{
  auto log = spdlog::get("log");

  beast::multi_buffer buffer;

  SPDLOG_TRACE(log, "Waiting for message");
  stream.async_read(buffer, yield);

  Message::Enum ret = json::parse(beast::buffers_to_string(buffer.data()));
  SPDLOG_DEBUG(log, "Got message {}", xtd::lazy_json_dump(ret));

  return ret;
}

Trial Runner::exec_remote_experiment(stream_type &stream, const Experiment &,
    Trial trial, io::yield_context yield)
{
  auto log = spdlog::get("log");

  log->info("Runner::exec_remote_experiment: preparing to send run of {} "
      "to {} with inputs {}", trial.input().experiment_name(),
      stream.next_layer().remote_endpoint(),
      xtd::lazy_json_dump(trial.input().sample()));

  auto cmd = Message::RunTrial::mk(std::move(trial));
  send_message(stream, std::move(cmd), yield);
  auto resp = get_message(stream, yield);
  resp.visit(xtd::overload(
    [&trial](Message::TrialDone &done) {
      trial = std::move(done.trial());
    },
    [](Message &msg) {
      throw std::runtime_error(std::string("Unexpected message type: ") +
          msg.virt_type_name());
    }));

  return trial;
}

Trial Runner::run_trial(const std::string &name,
    io::yield_context yield, stream_type *stream)
{
  auto log = spdlog::get("log");

  log->info("Runner::run_trial: running \"{}\"", name);

  Trial trial;

  trial.input().experiment_name(name);

  const auto &e = *experiments().at(name);
  SPDLOG_DEBUG(log, "   Experiment \"{}\": {}", name, xtd::lazy_json_dump(e));

  const auto &inputs = e.inputs();
  auto sample = inputs.sample();
  SPDLOG_DEBUG(log, "   Experiment \"{}\" inputs: {}",
      name, xtd::lazy_json_dump(sample));

  trial.input().sample(std::move(sample));

  if (stream) {
    return exec_remote_experiment(*stream, e, std::move(trial), yield);
  } else if (remote()) {
    return exec_remote_experiment(*remote(), e, std::move(trial), yield);
  } else {
    SPDLOG_TRACE(log, "Runner::run_trial: queueing experiment");
    auto ret = exec_experiment(e, std::move(trial), yield);
    SPDLOG_TRACE(log, "Runner::run_trial: enqueued experiment");
    return ret;
  }
}

void Runner::exec_experiment_impl(const Experiment &exp, Trial trial,
      std::function<void(Trial)> handler)
{
  auto log = spdlog::get("log");

  const auto &cmd = exp.cmd();

  log->info("Running command {}", xtd::lazy_json_dump(cmd));

  auto path = boost::this_process::path();
  auto cwd = boost::filesystem::current_path();
  cwd /= exp.cd();
  path.push_back(cwd);

  SPDLOG_DEBUG(log, "Search path: {}", xtd::lazy_json_dump(path));


  std::string in = json(trial.input()).dump();
  auto pin = xtd::into_shared(std::move(in));

  auto pout = std::make_shared<io::streambuf>();
  auto perr = std::make_shared<io::streambuf>();

  auto cmdpath = bp::search_path(cmd.at(0), std::move(path));
  SPDLOG_DEBUG(log, "Search result: {}", xtd::lazy_json_dump(cmdpath));

  std::error_code ec;

  auto trial_ = xtd::into_shared(std::move(trial));

  auto child_ = std::make_shared<std::unique_ptr<bp::child>>();

  auto on_exit =
    [=] (int result, const std::error_code &ec) mutable {
      SPDLOG_TRACE(log, "Runner::exec_experiment::on_exit: entered");
      (void)child_; // Capture child_ to extend lifetime
      std::string sout((std::istreambuf_iterator<char>(pout.get())),
                        std::istreambuf_iterator<char>());
      std::string serr((std::istreambuf_iterator<char>(perr.get())),
                        std::istreambuf_iterator<char>());

      log->info("Command exited with code {}", result);
      log->info("  ec: {}", ec.message());
      log->info("  stdin: {}", *pin);
      log->info("  stdout: {}", xtd::lazy_json_dump(sout));
      log->info("  stderr: {}", xtd::lazy_json_dump(serr));

      if (ec) {
        SPDLOG_TRACE(log, "Runner::exec_experiment::on_exit: error_code");
        trial_->status(TrialStatus::Error::mk(ErrorKind::ErrorCode::mk(
                ec, std::move(sout), std::move(serr))));
        handler(std::move(*trial_));
        return;
      }

      if (result != 0) {
        SPDLOG_TRACE(log, "Runner::exec_experiment::on_exit: exit status");
        trial_->status(TrialStatus::Error::mk(ErrorKind::ExitStatus::mk(
                result, std::move(sout), std::move(serr))));
        handler(std::move(*trial_));
        return;
      }

      try {
        SPDLOG_TRACE(log, "Runner::exec_experiment::on_exit: parsing stdout");
        TrialOutput out = json::parse(sout);
        SPDLOG_TRACE(log, "Runner::exec_experiment::on_exit: parsed stdout");

        trial_->status(TrialStatus::Complete::mk(std::move(out), std::move(serr)));
      } catch (const std::exception &e) {
        SPDLOG_TRACE(log, "Runner::exec_experiment::on_exit: bad stdout");
        trial_->status(TrialStatus::Error::mk(ErrorKind::BadOutput::mk(
                std::move(sout), std::move(serr))));
      }

      SPDLOG_TRACE(log, "Runner::exec_experiment::on_exit: calling handler");
      handler(std::move(*trial_));
      SPDLOG_TRACE(log, "Runner::exec_experiment::on_exit: called handler");

      SPDLOG_TRACE(log, "Runner::exec_experiment::on_exit: leaving");
    };

  SPDLOG_TRACE(log, "Runner::exec_experiment: creating child");
  *child_ = std::make_unique<bp::child>(
      cmdpath.string(),
      bp::args(cmd),
      bp::std_in < io::buffer(*pin),
      bp::std_out > *pout,
      bp::std_err > *perr,
      bp::start_dir(exp.cd()),
      bp::on_exit(on_exit),
      ioc_);
  SPDLOG_TRACE(log, "Runner::exec_experiment: created child");
}

void Runner::connect_to(std::string host, std::string port,
      std::function<void(stream_type)> callback)
{
  auto log = spdlog::get("log");

  auto do_connected =
    [log, host, port, callback, &runner = *this](io::yield_context yield) mutable {
      boost::system::error_code ec;

      try {
        tcp::resolver resolver{runner.ioc()};
        websocket::stream<tcp::socket> ws{runner.ioc()};

        auto const results = resolver.async_resolve(host, port, yield);

        boost::asio::async_connect(ws.next_layer(), results.begin(), results.end(), yield);

        ws.async_handshake(host, "/", yield);

        if (callback) {
          callback(std::move(ws));
        } else {
          ws.async_close(websocket::close_code::normal, yield);
        }
      } catch (...) {
        auto e = std::current_exception();
        xtd::log_exception(log, "Runner::connect_to", e);
        std::rethrow_exception(e);
      }
    };
  spawn(std::move(do_connected));
}

std::vector<Trial> Runner::run_batch(const std::string &name,
    io::yield_context yield)
{
  auto log = spdlog::get("log");

  if (remote_) {
    auto req = Message::RunBatch::mk(std::move(name));
    send_message(*remote_, std::move(req), yield);
    auto resp = get_message(*remote_, yield);
    std::vector<Trial> ret;
    resp.visit(xtd::overload(
      [&](Message::BatchDone &resp) {
        ret = std::move(resp.trials());
      },
      [](Message &msg) {
        throw std::runtime_error(
            std::string("Runner::run_batch Unexpected message type: ") +
            msg.virt_type_name());
      }));
    return ret;
  } else {
    auto remotes = registry_.lookup(name);

    size_t count = boost::size(remotes);
    std::vector<Trial> ret;
    ret.reserve(count);

    std::vector<Registry::remotes_iterator_type> dead_remotes;

    xtd::CoroutineWaiter waiter(ioc());
    for (auto &remote : remotes)
    {
      waiter.spawn(
        [this, &remote, &stream = remote.second->stream(), &ret,
          &name, &dead_remotes, &log]
        (io::yield_context yield) mutable
        {
          try {
            SPDLOG_TRACE(log, "RunBatch: starting experiment \"{}\"", name);
            Trial trial = run_trial(name, yield, &stream);
            SPDLOG_TRACE(log, "RunBatch: experiment \"{}\" completed", name);
            ret.emplace_back(std::move(trial));
          } catch (...) {
            xtd::log_exception(spdlog::get("log"),
                "RunBatch", std::current_exception());
            SPDLOG_TRACE(log, "RunBatch: while requesting trial from {}, "
                "marking as dead",
                stream.next_layer().remote_endpoint());
            dead_remotes.emplace_back(remote.second);
          }
        });
    }
    SPDLOG_TRACE(log, "RunBatch: waiting for {} completions", count);
    waiter.async_wait(count, yield);

    SPDLOG_TRACE(log, "RunBatch: removing {} dead remotes",
        dead_remotes.size());
    registry_.remove(dead_remotes);

    return ret;
  }
}

bool Runner::handle_request(Runner::stream_type &stream,
    Message::Enum req, io::yield_context yield)
{
  auto log = spdlog::get("log");

  bool ret = true;
  req.visit(xtd::overload(
    [&](Message::RunTrial &run) {
      SPDLOG_TRACE(log, "Runner::handle_request Handle RunTrial {}",
          xtd::lazy_json_dump(run));

      Trial trial = std::move(run.trial());
      const std::string &name = trial.input().experiment_name();
      try {
        auto e = experiments_.find(name);
        if (e != experiments_.end()) {
          trial = exec_experiment(*e->second, std::move(trial), yield);
        } else {
          trial.status(TrialStatus::Error::mk(
                ErrorKind::UnknownExperiment::mk(name)));
        }
      } catch (const std::exception &e) {
        trial.exception(e);
        log->error("Runner::handle_request RunTrial: trial caused exception: \"{}\"",
            json(trial).dump());
      } catch (...) {
        trial.exception(std::runtime_error("Unknown exception; not std::exception!"));
        log->critical("Runner::handle_request RunTrial: trial caused exception not "
            "inherited from std::exception!");
      }
      auto resp = Message::TrialDone::mk(
          std::move(trial));
      send_message(stream, std::move(resp), yield);
      SPDLOG_TRACE(log, "Runner::handle_request Ran trial");
    },
    [&](Message::Register &reg) {
      SPDLOG_TRACE(spdlog::get("log"),
          "Runner::handle_request Handle Register msg {}",
          xtd::lazy_json_dump(req));

      registry_.register_remote(
          std::move(stream), std::move(reg.experiments()));
      SPDLOG_TRACE(spdlog::get("log"),
          "Runner::handle_request Registered remote");
      ret = false;
    },
    [&](Message::RunBatch &run) {
      SPDLOG_TRACE(spdlog::get("log"),
          "Runner::handle_request Handle RunBatch {}",
          xtd::lazy_json_dump(run));

      std::string name = std::move(run.experiment_name());
      auto results = run_batch(name, yield);
      auto resp = Message::BatchDone::mk(std::move(name), std::move(results));
      send_message(stream, std::move(resp), yield);
      SPDLOG_TRACE(spdlog::get("log"),
          "Runner::handle_request Ran batch");
    },
    [](Message &msg) {
      throw std::runtime_error(
          std::string("Unexpected message type: ") +
          msg.virt_type_name());
    }));
  return ret;
}

void Runner::launch_listener(std::string host, std::string port)
{
  auto log = spdlog::get("log");

  auto do_accept =
    [log, host, port, &runner = *this](io::yield_context yield) mutable {
      namespace sys = boost::system;

      sys::error_code ec;

      tcp::acceptor acceptor(runner.ioc());

      tcp::resolver resolver{runner.ioc()};
      websocket::stream<tcp::socket> ws{runner.ioc()};

      auto const results = resolver.async_resolve(host, port, yield);

      if (results.size() == 0) {
        log->error("Runner::launch_listener: host/port not found: {}:{}",
            host, port);
      }

      auto endpoint = results.begin()->endpoint();

      acceptor.open(endpoint.protocol());
      acceptor.bind(endpoint);

      acceptor.listen(io::socket_base::max_listen_connections);

      for(;;) {
        try {
          tcp::socket socket(runner.ioc());
          acceptor.async_accept(socket, yield);
          runner.spawn(std::move(
            [log, socket = std::move(socket), &runner]
            (io::yield_context yield) mutable {
              try {
                tcp::endpoint remote_endpoint = socket.remote_endpoint();
                Registry::Remote::stream_type ws{std::move(socket)};

                log->info("TCP connection from {} accepted", remote_endpoint);
                ws.async_accept(yield);
                log->info("Websocket connection from {} accepted",
                    remote_endpoint);

                for(;;) {
                  auto req = runner.get_message(ws, yield);
                  if (!runner.handle_request(ws, std::move(req), yield)) {
                    break;
                  }
                }
              } catch (...) {
                xtd::log_exception(log, "Runner::launch_listener acceptor",
                    std::current_exception());
              }
            }));
        } catch (...) {
          xtd::log_exception(log, "Runner::launch_listener listener",
              std::current_exception());
        }
      }
    };
  spawn(std::move(do_accept));
}

void Runner::register_with(std::string host, std::string port)
{
  connect_to(host, port,
    [this](Runner::stream_type stream) mutable {
      spawn(
        [this, stream = std::move(stream)]
        (io::yield_context yield) mutable
        {
          std::vector<std::string> keys = xtd::get_keys(experiments_);
          send_message(stream, Message::Register::mk(std::move(keys)), yield);
          for (;;) {
            SPDLOG_DEBUG(spdlog::get("log"),
                "Runner::register_with waiting for command");
            auto req = get_message(stream, yield);
            SPDLOG_DEBUG(spdlog::get("log"),
                "Runner::register_with got command {}",
                xtd::lazy_json_dump(req));
            if (!handle_request(stream, std::move(req), yield)) {
              break;
            }
          }
        });
    });
}

void Runner::run()
{
  for (;;) {
    try {
      ioc().run();
      break;
    } catch (...) {
      xtd::log_exception(spdlog::get("log"), "Runner::run",
          std::current_exception());
    }
  }
}

} // namespace royale
