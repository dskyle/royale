#include <cstdlib>
#include <unistd.h>
#include <iostream>
#include <utility>
#include <vector>
#include <boost/lexical_cast.hpp>
#include <cxxopts.hpp>
#include <royale/util.hpp>
#include <royale/Runner.hpp>
#include "opts.hpp"

using namespace royale;

int main(int argc, char *argv[])
{
  auto console = spdlog::stderr_color_mt("log");
  auto json_console = spdlog::stderr_color_mt("json");
  auto runner = handle_options(argc, argv);

  runner->run();

  return 0;
}
