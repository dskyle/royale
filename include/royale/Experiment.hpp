#ifndef INCL_ROYALE_EXPERIMENT_HPP
#define INCL_ROYALE_EXPERIMENT_HPP

#include <iostream>
#include <utility>
#include <vector>
#include <map>
#include <random>
#include <boost/lexical_cast.hpp>
#include <royale/util.hpp>
#include <boost/preprocessor/variadic/to_seq.hpp>
#include "royale/InputSpec.hpp"

namespace royale {

class Experiment
{
public:
  using env_type = std::map<std::string, std::string>;
  using input_type = std::map<std::string, ValueSpec::Enum>;
private:
  ROYALE_JSON_FIELDS(Experiment,
      (std::string, name)
      (std::string, version)
      (double, timeout, 0)
      (std::string, cd, ".")
      (std::vector<std::string>, cmd)
      (env_type, env)
      (InputSpec, input)
    );

public:
  Experiment &name(std::string name)
  {
    name_ = std::move(name);
    return *this;
  }

  const std::string &name() const { return name_; }

  Experiment &cd(std::string cd)
  {
    cd_ = std::move(cd);
    return *this;
  }

  const std::string &cd() const { return cd_; }

  template<typename... Args>
  auto cmd(Args&&... args) ->
    xtd::enable_if<(sizeof...(Args) > 0), Experiment &>
  {
    cmd_ = {
      boost::lexical_cast<std::string>(std::forward<Args>(args))...
    };
    return *this;
  }

  Experiment &cmd(std::initializer_list<std::string> i)
  {
    cmd_ = i;
    return *this;
  }

  Experiment &cmd(std::vector<std::string> c)
  {
    cmd_ = c;
    return *this;
  }

  const std::vector<std::string> &cmd() const { return cmd_; }

  Experiment &env(env_type e) { env_ = std::move(e); return *this; }

  Experiment &env(
      std::initializer_list<std::pair<const std::string, std::string>> i)
  {
    env_ = i;
    return *this;
  }

  const env_type & env() const { return env_; }

  xtd::map_inserter<decltype(env_)> extend_env() { return {env_}; }

  Experiment &inputs(input_type v)
  {
    input_.inputs(std::move(v)); return *this;
  }

  const InputSpec &inputs() const { return input_; }

  xtd::map_inserter<input_type> extend_inputs()
  {
    return input_.extend_inputs();
  }
};

} // namespace royale

#endif // INCL_ROYALE_EXPERIMENT_HPP
