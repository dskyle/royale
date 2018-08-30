#ifndef INCL_ROYALE_INPUTSPEC_HPP
#define INCL_ROYALE_INPUTSPEC_HPP

#include <iostream>
#include <utility>
#include <vector>
#include <map>
#include <random>
#include <boost/lexical_cast.hpp>
#include <royale/util.hpp>
#include <boost/preprocessor/variadic/to_seq.hpp>
#include "royale/ValueSpec.hpp"

namespace royale {

class InputSpec
{
public:
  using input_type = std::map<std::string, ValueSpec::Enum>;

  using sample_type = std::map<std::string, Value>;

private:
  ROYALE_JSON_FIELDS(InputSpec,
      (input_type, input)
    );

  friend class ::nlohmann::adl_serializer<InputSpec>;

public:
  InputSpec() = default;
  InputSpec(input_type input) : input_(std::move(input)) {}

  InputSpec &inputs(input_type v) { input_ = std::move(v); return *this; }

  const input_type &inputs() const { return input_; }

  xtd::map_inserter<decltype(input_)> extend_inputs() { return {input_}; }

  sample_type sample() const
  {
    sample_type ret;
    for (const auto &i : input_) {
      ret.emplace(std::piecewise_construct,
          std::forward_as_tuple(i.first),
          std::forward_as_tuple(i.second->sample()));
    }
    return ret;
  }
};

} // namespace royale

namespace nlohmann
{
  template<>
  struct adl_serializer<::royale::InputSpec>
  {
    using InputSpec = ::royale::InputSpec;

    static void to_json(json &j, const InputSpec &v)
    {
      SPDLOG_TRACE(spdlog::get("log"),
          "Entering to_json InputSpec overload");
      j = v.input_;
      SPDLOG_TRACE(spdlog::get("log"),
          "Leaving to_json InputSpec overload ({})",
          ::royale::xtd::lazy_json_dump(j));
    }

    static void from_json(const json &j, InputSpec &v)
    {
      SPDLOG_TRACE(spdlog::get("log"),
          "Entering from_json InputSpec overload ({})",
          ::royale::xtd::lazy_json_dump(j));
      v.input_ = j.get<InputSpec::input_type>();
      SPDLOG_TRACE(spdlog::get("log"),
          "Leaving from_json InputSpec overload");
    }
  };
}

#endif // INCL_ROYALE_INPUTSPEC_HPP
