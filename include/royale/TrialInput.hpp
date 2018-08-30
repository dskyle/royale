#ifndef INCL_ROYALE_TRIALINPUT_HPP
#define INCL_ROYALE_TRIALINPUT_HPP

#include <iostream>
#include <utility>
#include <vector>
#include "royale/util.hpp"
#include "royale/InputSpec.hpp"
#include "royale/ErrorKind.hpp"

namespace royale {

class TrialInput
{
public:
  using sample_type = std::map<std::string, Value>;

  ROYALE_JSON_FIELDS(TrialInput,
      (std::string, experiment_name)
      (sample_type, sample)
      (json, replicate)
    );

public:
  TrialInput() = default;

  TrialInput(
      std::string name,
      InputSpec::sample_type sample = InputSpec::sample_type{},
      json replicate = json{})
    : experiment_name_(std::move(name)),
      sample_(std::move(sample)),
      replicate_(std::move(replicate)) {}

  const std::string &experiment_name() const { return experiment_name_; }
  TrialInput &experiment_name(std::string name)
  {
    experiment_name_ = std::move(name);
    return *this;
  }

  const sample_type &sample() const { return sample_; }
  TrialInput &sample(sample_type sample)
  {
    sample_ = std::move(sample);
    return *this;
  }

  const json &replicate() const { return replicate_; }
  TrialInput &replicate(json r) { replicate_ = std::move(r); return *this; }
};

} // namespace royale

#endif // INCL_ROYALE_TRIALINPUT_HPP
