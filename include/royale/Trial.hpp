#ifndef INCL_ROYALE_TRIAL_HPP
#define INCL_ROYALE_TRIAL_HPP

#include <iostream>
#include <utility>
#include <vector>
#include <boost/asio.hpp>
#include "royale/util.hpp"
#include "royale/TrialInput.hpp"
#include "royale/ErrorKind.hpp"

namespace io = boost::asio;

namespace royale {

enum class StatusCode
{
  Created,
  InProgress,
  Error,
  Complete,
};

class TrialStatus : public xtd::JsonObject
{
public:
  virtual StatusCode code() const = 0;
  virtual bool final() const { return false; }

  ROYALE_JSON_ENUM(TrialStatus, Created, InProgress, Error, Complete);
};

class TrialOutput
{
public:
  using preds_type = std::map<std::string, bool>;
  using aux_type = std::map<std::string, json>;

  ROYALE_JSON_FIELDS(TrialOutput,
      (preds_type, preds)
      (aux_type, aux)
      (json, replicate)
    );

public:
  const preds_type &preds() const { return preds_; }
  const aux_type &aux() const { return aux_; }
  const json &replicate() const { return replicate_; }
};

class TrialStatus::Created : public xtd::EnableJsonObject<Created, TrialStatus>
{
  ROYALE_JSON_NO_FIELDS(Created);
public:
  StatusCode code() const override { return StatusCode::Created; }
};

class TrialStatus::InProgress
  : public xtd::EnableJsonObject<InProgress, TrialStatus>
{
  ROYALE_JSON_NO_FIELDS(InProgress);
public:
  StatusCode code() const override { return StatusCode::InProgress; }
};

class TrialStatus::Error : public xtd::EnableJsonObject<Error, TrialStatus>
{
  ROYALE_JSON_FIELDS(Error,
      (ErrorKind::Enum, kind)
    );
public:
  StatusCode code() const override { return StatusCode::Error; }
  virtual bool final() const { return true; }

  Error() = default;
  Error(ErrorKind::Enum kind) : kind_(std::move(kind)) {}

  Error(const std::exception &e) : kind_(ErrorKind::Exception::mk(e)) {}

  friend void to_json(json &j, const TrialStatus::Error &v)
  {
    j = v.kind_;
  }

  friend void from_json(const json &j, TrialStatus::Error &v)
  {
    v.kind_ = j;
  }
};

class TrialStatus::Complete : public xtd::EnableJsonObject<Complete, TrialStatus>
{
  ROYALE_JSON_FIELDS(Complete,
      (TrialOutput, output)
      (std::string, stderr)
    );

public:
  StatusCode code() const override { return StatusCode::Complete; }
  virtual bool final() const { return true; }

  Complete() = default;
  Complete(TrialOutput output, std::string stderr = "")
    : output_(output), stderr_(stderr) {}

  const TrialOutput &output() const { return output_; }
};

class Trial
{
  ROYALE_JSON_FIELDS(Trial,
      (TrialStatus::Enum, status, TrialStatus::Created::mk())
      (TrialInput, input)
    );

public:
  Trial() = default;

  explicit Trial(std::string name,
      TrialInput::sample_type sample = TrialInput::sample_type{})
    : input_(std::move(name), std::move(sample)) {}

  const TrialInput &input() const { return input_; }
  TrialInput &input() { return input_; }
  Trial &input(TrialInput trial_input)
  {
    input_ = std::move(trial_input);
    return *this;
  }

  const TrialInput::sample_type &sample() const { return input_.sample(); }
  Trial &sample(TrialInput::sample_type sample)
  {
    input_.sample(std::move(sample));
    return *this;
  }

  Trial &exception(const std::exception &e)
  {
    status_ = TrialStatus::Error::mk(e);
    return *this;
  }

  const TrialStatus::Enum &status() const { return status_; }
  Trial &status(TrialStatus::Enum status)
  {
    status_ = std::move(status);
    return *this;
  }
};


class AnalysisStatus : public xtd::JsonObject
{
public:
  virtual StatusCode code() const = 0;
  virtual bool final() const { return false; }

  ROYALE_JSON_ENUM(AnalysisStatus, Created, InProgress, Error, Complete);
};

class AnalysisOutput : public xtd::JsonObject
{
public:
  ROYALE_JSON_ENUM(AnalysisOutput, LogisticRegression);
};

class AnalysisStatus::Created
  : public xtd::EnableJsonObject<Created, AnalysisStatus>
{
  ROYALE_JSON_NO_FIELDS(Created);
public:
  StatusCode code() const override { return StatusCode::Created; }
};

class AnalysisStatus::InProgress
  : public xtd::EnableJsonObject<InProgress, AnalysisStatus>
{
  ROYALE_JSON_NO_FIELDS(InProgress);
public:
  StatusCode code() const override { return StatusCode::InProgress; }
};

class AnalysisStatus::Error
  : public xtd::EnableJsonObject<Error, AnalysisStatus>
{
  ROYALE_JSON_FIELDS(Error,
      (ErrorKind::Enum, kind)
    );
public:
  StatusCode code() const override { return StatusCode::Error; }
  virtual bool final() const { return true; }

  Error() = default;
  explicit Error(ErrorKind::Enum kind) : kind_(std::move(kind)) {}

  explicit Error(const std::exception &e)
    : kind_(ErrorKind::Exception::mk(e)) {}

  friend void to_json(json &j, const AnalysisStatus::Error &v)
  {
    j = v.kind_;
  }

  friend void from_json(const json &j, AnalysisStatus::Error &v)
  {
    v.kind_ = j;
  }
};

class AnalysisStatus::Complete
  : public xtd::EnableJsonObject<Complete, AnalysisStatus>
{
  ROYALE_JSON_FIELDS(Complete,
      (AnalysisOutput::Enum, output)
      (std::string, stderr)
    );

public:
  StatusCode code() const override { return StatusCode::Complete; }
  virtual bool final() const { return true; }

  Complete() = default;
  explicit Complete(AnalysisOutput::Enum output, std::string stderr = "")
    : output_(std::move(output)), stderr_(stderr) {}
};

class AnalysisInput : public xtd::EnableJsonObject<AnalysisInput>
{
public:
  using data_type = std::vector<Trial>;

  ROYALE_JSON_FIELDS(AnalysisInput,
      (data_type, data)
    );

public:
  AnalysisInput() = default;

  explicit AnalysisInput(data_type data)
    : data_(std::move(data)) {}

  const data_type &data() const { return data_; }
  AnalysisInput &data(data_type data)
  {
    data_ = std::move(data);
    return *this;
  }
};

class AnalysisType : public xtd::JsonObject
{
public:
  ROYALE_JSON_ENUM(AnalysisType, LogisticRegression);

  virtual AnalysisOutput::Enum do_analysis(const AnalysisInput &input,
      io::yield_context yield) = 0;
};

class Analysis
{
  ROYALE_JSON_FIELDS(Analysis,
      (AnalysisStatus::Enum, status, AnalysisStatus::Created::mk())
      (AnalysisInput, input)
      (AnalysisType::Enum, type)
    );

public:
  Analysis() = default;

  explicit Analysis(
      const char *type,
      AnalysisInput::data_type data = AnalysisInput::data_type{})
    : input_{std::move(data)},
      type_{} { type_.init(type, json{}); }

  const AnalysisInput &input() const { return input_; }
  AnalysisInput &input() { return input_; }
  Analysis &input(AnalysisInput trial_input)
  {
    input_ = std::move(trial_input);
    return *this;
  }

  const AnalysisType::Enum &type() const { return type_; }
  Analysis &sample(AnalysisType::Enum type)
  {
    type_ = std::move(type);
    return *this;
  }

  Analysis &exception(const std::exception &e)
  {
    status_ = AnalysisStatus::Error::mk(e);
    return *this;
  }

  const AnalysisStatus::Enum &status() const { return status_; }
  Analysis &status(AnalysisStatus::Enum status)
  {
    status_ = std::move(status);
    return *this;
  }

  void run(io::yield_context yield)
  {
    auto out = type_->do_analysis(input_, yield);
    status_ = AnalysisStatus::Complete::mk(std::move(out));
  }
};

class PredicateOutput : public xtd::EnableJsonObject<PredicateOutput>
{
  ROYALE_JSON_FIELDS(PredicateOutput,
      (std::string, name)
      (size_t, sat_count, 0)
      (size_t, error_count, 0)
      (size_t, count, 0)
      (double, prob, 0)
      (double, rel_error, 0)
    );
public:
  const std::string &name() const { return name_; }
  size_t sat_count() const { return sat_count_; }
  size_t error_count() const { return error_count_; }
  size_t count() const { return count_; }
  double prob() const { return prob_; }
  double error_prob() const { return prob_; }
  double rel_error() const { return rel_error_; }

  void add_sat()
  {
    ++sat_count_;
    add_unsat();
  }

  void add_unsat()
  {
    ++count_;
    prob_ = sat_count_ / (double)(count_ - error_count_);
  }

  void add_error()
  {
    ++error_count_;
    ++count_;
  }
};

class LogisticPredicateOutput
  : public xtd::EnableJsonObject<LogisticPredicateOutput, PredicateOutput>
{
public:
  using coeffs_type = std::map<std::string, double>;

  ROYALE_JSON_SUBFIELDS(LogisticPredicateOutput, PredicateOutput,
      (coeffs_type, coeffs)
    );
public:
  const coeffs_type &coeffs() const { return coeffs_; }
  LogisticPredicateOutput &coeffs(coeffs_type coeffs)
  {
    coeffs_ = coeffs;
    return *this;
  }
};

class AnalysisOutput::LogisticRegression :
  public xtd::EnableJsonObject<LogisticRegression, AnalysisOutput>
{
public:
  using preds_type = std::map<std::string, LogisticPredicateOutput>;
  ROYALE_JSON_FIELDS(LogisticRegression,
        (preds_type, preds)
      );
public:
  LogisticRegression() = default;

  LogisticRegression(preds_type preds)
    : preds_(std::move(preds)) {}

  preds_type &preds() { return preds_; }
};

class AnalysisType::LogisticRegression :
  public xtd::EnableJsonObject<LogisticRegression, AnalysisType>
{
public:
  using output_type = AnalysisOutput::LogisticRegression;

  ROYALE_JSON_NO_FIELDS(LogisticRegression);

public:
  AnalysisOutput::Enum do_analysis(const AnalysisInput &input,
      io::yield_context) override;
};

} // namespace royale

#endif // INCL_ROYALE_TRIAL_HPP
