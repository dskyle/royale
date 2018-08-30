#ifndef INCL_ROYALE_RUNNER_HPP
#define INCL_ROYALE_RUNNER_HPP

#include <utility>
#include <boost/asio/spawn.hpp>
#include <boost/process.hpp>
#include <boost/beast/websocket.hpp>
#include "royale/util.hpp"
#include "royale/Experiment.hpp"
#include "royale/Trial.hpp"

namespace royale {

namespace io = boost::asio;
using tcp = io::ip::tcp;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace bp = boost::process;


class Registry
{
public:
  class Remote;

  using remotes_type = std::list<Remote>;
  using remotes_iterator_type = remotes_type::iterator;
  using executors_type = std::multimap<std::string, remotes_iterator_type>;
  using iterator_type = executors_type::iterator;
  using const_iterator_type = executors_type::const_iterator;
  using range_type = boost::iterator_range<iterator_type>;
  using const_range_type = boost::iterator_range<const_iterator_type>;
private:
  remotes_type remotes_;
  executors_type executors_;
public:
  class Remote
  {
  public:
    using stream_type = websocket::stream<tcp::socket>;
    using experiments_type = std::vector<std::string>;
    using uptr = std::unique_ptr<Remote>;
  private:
    stream_type stream_;
    experiments_type experiments_;

    remotes_iterator_type iter_;

    struct private_key {};

  public:
    /// Must be public to allow emplace to work, but should be treated otherwise
    /// as private. Enforced by taking a type tag "private_key" which only
    /// those with private access can instantiate.
    Remote(private_key, stream_type stream, experiments_type experiments = {})
      : stream_(std::move(stream)), experiments_(std::move(experiments)) {}

    Remote(const Remote &) = delete;
    Remote(Remote &&) = delete;

    Remote &operator =(const Remote &) = delete;
    Remote &operator =(Remote &&) = delete;

    stream_type &stream() { return stream_; }
    experiments_type &experiments() { return experiments_; }

    friend class Registry;
  };

  Remote *register_remote(
      Remote::stream_type stream, Remote::experiments_type experiments = {})
  {
    auto iter = remotes_.emplace(remotes_.end(),
        Remote::private_key{}, std::move(stream), std::move(experiments));

    Remote *ret = &*iter;
    ret->iter_ = iter;

    for (const auto &cur : ret->experiments_) {
      executors_.emplace(std::piecewise_construct,
          std::forward_as_tuple(cur),
          std::forward_as_tuple(std::move(iter)));
    }

    return ret;
  };

  const_range_type lookup(const std::string &experiment_name) const
  {
    const_iterator_type start = executors_.lower_bound(experiment_name);
    const_iterator_type stop = executors_.upper_bound(experiment_name);
    return {start, stop};
  }

  range_type lookup(const std::string &experiment_name)
  {
    iterator_type start = executors_.lower_bound(experiment_name);
    iterator_type stop = executors_.upper_bound(experiment_name);
    return {start, stop};
  }

  void remove(remotes_iterator_type dead_remote)
  {
    for (auto i = executors_.begin();
         i != executors_.end();) {
      if (i->second == dead_remote) {
        auto next = i;
        ++next;
        executors_.erase(i);
        i = next;
      } else {
        ++i;
      }
    }
    remotes_.erase(dead_remote);
  }

  void remove(const std::vector<remotes_iterator_type> dead_remotes)
  {
    for (auto cur : dead_remotes) {
      remove(cur);
    }
  }
};

class Message : public xtd::JsonObject
{
public:
  ROYALE_JSON_ENUM(Message, RunTrial, TrialDone, Register,
      RunBatch, BatchDone);
};

class Message::RunTrial
  : public xtd::EnableJsonObject<RunTrial, Message>
{
  ROYALE_JSON_FIELDS(RunTrial,
      (Trial, trial)
    );

public:
  RunTrial() = default;
  RunTrial(Trial trial) : trial_(std::move(trial)) {}

  Trial &trial() { return trial_; }
  const Trial &trial() const { return trial_; }
  RunTrial &trial(Trial trial) { trial_ = std::move(trial); return *this; }
};

class Message::TrialDone
  : public xtd::EnableJsonObject<TrialDone, Message>
{
  ROYALE_JSON_FIELDS(TrialDone,
      (Trial, trial)
    );

public:
  TrialDone() = default;
  TrialDone(Trial trial) : trial_(std::move(trial)) {}

  Trial &trial() { return trial_; }
  const Trial &trial() const { return trial_; }
  TrialDone &trial(Trial trial) { trial_ = std::move(trial); return *this; }
};

class Message::Register
  : public xtd::EnableJsonObject<Register, Message>
{
  ROYALE_JSON_FIELDS(Register,
      (std::vector<std::string>, experiments)
    );

public:
  using experiments_type = std::vector<std::string>;

  Register() = default;
  Register(experiments_type experiments) :
    experiments_(std::move(experiments)) {}

  experiments_type &experiments() { return experiments_; }
  const experiments_type &experiments() const { return experiments_; }
  Register &experiments(experiments_type experiments) {
    experiments_ = std::move(experiments);
    return *this;
  }
};

class Message::RunBatch
  : public xtd::EnableJsonObject<RunBatch, Message>
{
  ROYALE_JSON_FIELDS(RunBatch,
      (std::string, experiment_name)
    );

public:
  RunBatch() = default;
  RunBatch(std::string experiment_name) :
    experiment_name_(std::move(experiment_name)) {}

  std::string &experiment_name() { return experiment_name_; }
};

class Message::BatchDone
  : public xtd::EnableJsonObject<BatchDone, Message>
{
  ROYALE_JSON_FIELDS(BatchDone,
      (std::string, experiment_name)
      (std::vector<Trial>, trials)
    );

public:
  BatchDone() = default;
  BatchDone(std::string experiment_name, std::vector<Trial> trials) :
    experiment_name_(std::move(experiment_name)),
    trials_(std::move(trials)) {}

  std::string &experiment_name() { return experiment_name_; }
  std::vector<Trial> &trials() { return trials_; }
};

class Runner
{
public:
  using experiments_type = std::map<std::string, std::unique_ptr<Experiment>>;
  using stream_type = websocket::stream<tcp::socket>;
private:
  experiments_type experiments_;
  io::io_context ioc_;//{new io::io_context{}};
  Registry registry_;
  std::unique_ptr<stream_type> remote_;

private:
  void exec_experiment_impl(const Experiment &exp, Trial trial,
      std::function<void(Trial)> handler);

  template<typename Handler>
  auto exec_experiment(const Experiment &exp, Trial trial, Handler &&handler)
  {
    return xtd::do_async_func<void(Trial)>(
        [&](std::function<void(Trial)> f) {
          exec_experiment_impl(exp, std::move(trial), f);
        },
        std::forward<Handler>(handler));
  }

  Trial exec_remote_experiment(stream_type &stream, const Experiment &exp,
    Trial trial, io::yield_context yield);

  bool handle_request(Runner::stream_type &stream, Message::Enum req,
      io::yield_context yield);

  void send_message(stream_type &stream, Message::Enum message,
      io::yield_context yield);

  Message::Enum get_message(stream_type &stream,
      io::yield_context yield);

public:
  Experiment &add_experiment(Experiment e);

  const experiments_type &experiments() const {
    return experiments_;
  }

  Trial run_trial(const std::string &name,
    io::yield_context yield, stream_type *stream = nullptr);

  std::vector<Trial> run_batch(const std::string &name,
    io::yield_context yield);

  template<typename Func>
  void spawn(Func func)
  {
    io::spawn(ioc(), std::move(func));
  }

  void connect_to(std::string addr, std::string port,
      std::function<void(stream_type)> callback = {});
  void launch_listener(std::string host, std::string port);
  void register_with(std::string addr, std::string port);

  void run();

  bool connected() { return (bool)remote_; }
  stream_type *remote() { return remote_.get(); }
  stream_type &remote(stream_type stream)
  {
    remote_ = xtd::into_unique(std::move(stream));
    return *remote_;
  }

  io::io_context &ioc() { return ioc_; }

  int pretty = -1;
  std::string cd;
};

} // namespace royale

#endif // INCL_ROYALE_RUNNER_HPP
