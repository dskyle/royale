#ifndef INCL_ROYALE_RUNNER_OPTS_HPP
#define INCL_ROYALE_RUNNER_OPTS_HPP

#include <royale/Runner.hpp>

namespace royale {
  std::unique_ptr<Runner> handle_options(int argc, char *argv[]);
}

#endif // INCL_ROYALE_RUNNER_OPTS_HPP
