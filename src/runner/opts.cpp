#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <iostream>
#include <utility>
#include <vector>
#include <experimental/filesystem>
#include <boost/lexical_cast.hpp>
#include <cxxopts.hpp>
#include <royale/util.hpp>
#include <royale/Runner.hpp>

namespace fs = std::experimental::filesystem;

namespace royale {

static const char experiment_json_extension[] = ".experiment.json";

static std::pair<std::string, std::string> parse_host_port(std::string in,
    const char *default_host = "localhost")
{
  size_t col_pos = in.rfind(":");
  if (col_pos == in.npos)
  {
    return {default_host, std::move(in)};
  } else {
    std::string port = in.substr(col_pos + 1);
    in.resize(col_pos);
    return {std::move(in), std::move(port)};
  }
}

static std::unique_ptr<Runner> handle_option_result(
    const cxxopts::Options &options,
    cxxopts::ParseResult result)
{
  auto log = spdlog::get("log");

  int level = result["log"].as<int>();

  if (level > ROYALE_LOG_MAX) {
    log->set_level(spdlog::level::warn);
    log->warn("Log level {} exceeds compiled max of {}", level, ROYALE_LOG_MAX);
    level = ROYALE_LOG_MAX;
  }

  if (level < ROYALE_LOG_MIN) {
    log->set_level(spdlog::level::warn);
    log->warn("Log level {} exceeds min of {}", level, ROYALE_LOG_MIN);
    level = ROYALE_LOG_MIN;
  }

  log->set_level(spdlog::level::level_enum(6 - level));

  level = log->level();

  SPDLOG_DEBUG(log, "Log level set to {} ({})",
      spdlog::level::to_str(spdlog::level::level_enum(level)), 6 - level);

  if (result.count("help") > 0) {
    std::cout << options.help() << std::endl;
    std::exit(0);
  }

  auto ret = std::make_unique<Runner>();

  ret->pretty = result["pretty"].as<int>();

  auto get_str = [&](const char *s) {
    return result.count(s) > 0 ?
      result[s].as<std::string>() :
      std::string{};
  };

  auto get_vec = [&](const char *s) {
    return result.count(s) > 0 ?
      result[s].as<std::vector<std::string>>() :
      std::vector<std::string>{};
  };

  ret->cd = get_str("cd");

  if (ret->cd != "") {
    ROYALE_ERRNO_THROW(chdir, (ret->cd.c_str()));
  }

  auto add_str = [&](const char *str) -> Experiment & {
    auto j = json::parse(str);
    SPDLOG_TRACE(log, "Adding Experiment json: {}", j.dump());
    return ret->add_experiment(j);
  };

  auto add_file = [&](const char *filename) -> Experiment & {
    log->info("Adding Experiment file {}", filename);
    auto str = xtd::file_to_string(filename);
    return add_str(str.c_str());
  };

  for (const auto &dir : get_vec("directory")) {
    SPDLOG_TRACE(log, "Adding experiments from directory: {}", dir);
    for (const auto &file : fs::directory_iterator(dir)) {
      if (xtd::ends_with(file.path().c_str(), experiment_json_extension)) {
        auto &exp = add_file(file.path().c_str());
        if (exp.cd() == "") {
          exp.cd(dir);
        }
      } else {
        SPDLOG_TRACE(log, "Skipping non-Experiment file: {}", file.path());
      }
    }
  }

  for (const auto &filename : get_vec("file")) {
    add_file(filename.c_str());
  }

  for (const auto &literal : get_vec("json")) {
    add_str(literal.c_str());
  }

  auto use_results =
    [&runner = *ret, analysis = get_str("analysis"), log]
    (std::vector<Trial> results, io::yield_context yield)
    {
      json jresults;
      if (analysis == "") {
        jresults = json(results);
      } else {
        SPDLOG_TRACE(log, "Instantiating analyzer {}", analysis);
        Analysis analyzer(analysis.c_str(), std::move(results));
        SPDLOG_TRACE(log, "Created analyzer: {}",
            xtd::lazy_json_dump(analyzer));
        analyzer.run(yield);
        SPDLOG_TRACE(log, "Ran analyzer: {}",
            xtd::lazy_json_dump(analyzer));
        jresults = json(analyzer.status());
      }
      std::cout << xtd::dump(jresults, runner.pretty) << std::endl;
    };

  bool batch = result.count("batch") > 0;

  auto make_experiment_runner =
    [repeat = result["repeat"].as<int>(),
     runs = get_vec("exec"), batch]
    (Runner &runner, auto callback) {
      if (runs.size() > 0) {
        runner.spawn(
          [repeat, runs = std::move(runs), &runner, callback, batch]
          (io::yield_context yield)
          {
            std::vector<Trial> results;

            for (const auto &run : runs) {
              for (int i = 0; i < repeat; ++i) {
                if (batch) {
                  auto trials = runner.run_batch(run, yield);
                  results.insert(results.end(),
                      std::make_move_iterator(trials.begin()),
                      std::make_move_iterator(trials.end()));
                } else {
                  Trial trial = runner.run_trial(run, yield);
                  results.emplace_back(std::move(trial));
                }
              }
            }

            callback(std::move(results), yield);
          });
      }
    };

  if (result.count("serve") > 0) {
    std::string remote = result["serve"].as<std::string>();
    auto endpoint = parse_host_port(std::move(remote));

    ret->launch_listener(endpoint.first, endpoint.second);
  }

  if (result.count("register") > 0) {
    std::string remote = result["register"].as<std::string>();
    auto endpoint = parse_host_port(std::move(remote));

    ret->register_with(endpoint.first, endpoint.second);
  }

  if (result.count("input") > 0) {
    std::string input = result["input"].as<std::string>();
    std::vector<Trial> inputs = json::parse(input);
    ret->spawn(
      [use_results, inputs = std::move(inputs)]
      (io::yield_context yield) mutable
      {
        use_results(std::move(inputs), yield);
      });
  } else if (result.count("remote") > 0) {
    std::string remote = result["remote"].as<std::string>();
    auto endpoint = parse_host_port(std::move(remote));

    ret->connect_to(endpoint.first, endpoint.second,
      [=, &runner = *ret](Runner::stream_type stream) {
        runner.remote(std::move(stream));
        SPDLOG_TRACE(log, "Connected to remote, queueing any execs");
        make_experiment_runner(runner,
            [&runner, use_results]
            (std::vector<Trial> results, io::yield_context yield)
            {
              runner.remote()->async_close(
                  websocket::close_code::normal, yield);
              use_results(std::move(results), yield);
            }
          );
      });
  } else {
    if (batch) {
      log->error("-b/--batch options requires -r/--remote option");
      throw std::runtime_error("bad command line options");
    }
    make_experiment_runner(*ret, use_results);
  }

  return std::move(ret);
}

std::unique_ptr<Runner> handle_options(int argc, char *argv[])
{
  cxxopts::Options options(argv[0], "Experiment runner for Royale SMC system");

  options.add_options()
    ("h,help", "Print help message and exit")
    ("P,pretty", "Pass JSON in pretty format",
       cxxopts::value<int>()->default_value("-1")->implicit_value("2"))
    ("d,directory", "directory to load experiment files from. All files with "
      "extension .experiment.json will be loaded",
      cxxopts::value<std::vector<std::string>>())
    ("f,file", "experiment file to load, JSON format",
      cxxopts::value<std::vector<std::string>>())
    ("j,json", "experiment definition as string, JSON format",
      cxxopts::value<std::vector<std::string>>())
    ("C,cd", "Directory to cd at startup",
      cxxopts::value<std::string>())
    ("x,exec", "Run the named experiment, print JSON output to stdout",
      cxxopts::value<std::vector<std::string>>())
    ("R,repeat", "Run all --exec experiments N times before exiting",
      cxxopts::value<int>()->default_value("1"))
    ("s,serve", "Listen for HTTP requests on given ip:port. "
      "Default ip is 127.0.0.1",
      cxxopts::value<std::string>())
    ("g,register", "Register as a runner with given server. "
      "Give argument as \"ip:port\"; default ip is 127.0.0.1",
      cxxopts::value<std::string>())
    ("r,remote", "Instruct remote server, instead of running locally. "
      "Give argument as \"ip:port\"; default ip is 127.0.0.1",
      cxxopts::value<std::string>())
    ("B,batch", "Run as a batch, on all registered runners. Requires -r/"
      "--remote option, for registry server. -R/--repeat will repeat batches")
    ("i,input", "Don't run experiments, use results JSON from given file. "
      "If \"-\", read results JSON from stdin",
      cxxopts::value<std::string>())
    ("A,analysis", "Analyze results (either from -x/--exec or -i/--input) with "
      "given analysis engine: logistic_regression (logreg)",
      cxxopts::value<std::string>())
    ("l,log", "Set log level: "
#if (ROYALE_MAX_LOG >= 6)
     "6 (trace), "
#endif
#if (ROYALE_MAX_LOG >= 5)
     "5 (debug), "
#endif
     "4 (info), 3 (warn), 2 (err), 1 (critical), or 0 (off)",
      cxxopts::value<int>()
#if (ROYALE_MAX_LOG >= 5)
    ->default_value("5")
#else
    ->default_value("3")
#endif
    )
    ;

  //options.parse_positional("cmd");

  try {
    return handle_option_result(options, options.parse(argc, argv));
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    std::cerr << options.help() << std::endl;
    std::exit(-1);
  }
}

}
