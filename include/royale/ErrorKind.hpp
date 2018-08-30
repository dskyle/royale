#ifndef INCL_ROYALE_ERRORKIND_HPP
#define INCL_ROYALE_ERRORKIND_HPP

#include <utility>
#include "royale/util.hpp"

namespace royale {

class ErrorKind : public xtd::JsonObject
{
public:
  ROYALE_JSON_ENUM(ErrorKind, Exception, ErrorCode, ExitStatus, BadOutput,
      UnknownExperiment);
};

class ErrorKind::Exception : public xtd::EnableJsonObject<Exception, ErrorKind>
{
  ROYALE_JSON_FIELDS(Exception,
      (std::string, typeid)
      (std::string, what)
    );

public:
  Exception() = default;

  Exception(const std::exception &e)
    : typeid_(boost::typeindex::type_id_runtime(e).pretty_name()),
      what_(e.what()) {}
};

class ErrorKind::UnknownExperiment : public xtd::EnableJsonObject<UnknownExperiment, ErrorKind>
{
  ROYALE_JSON_FIELDS(UnknownExperiment,
      (std::string, name)
    );

public:
  UnknownExperiment() = default;

  UnknownExperiment(const std::string &name) : name_(name) {}
};

class ErrorKind::ErrorCode : public xtd::EnableJsonObject<ErrorCode, ErrorKind>
{
  ROYALE_JSON_FIELDS(ErrorCode,
      (int, value)
      (std::string, message)
      (std::string, category)
      (std::string, stdout)
      (std::string, stderr)
    );
public:
  ErrorCode() = default;

  ErrorCode(const std::error_code &code, std::string stdout, std::string stderr)
    : value_(code.value()), message_(code.message()),
      category_(code.category().name()),
      stdout_(stdout), stderr_(stderr) {}
};

class ErrorKind::ExitStatus : public xtd::EnableJsonObject<ExitStatus, ErrorKind>
{
  ROYALE_JSON_FIELDS(ExitStatus,
      (int, code)
      (std::string, stdout)
      (std::string, stderr)
    );
public:
  ExitStatus() = default;

  ExitStatus(int code, std::string stdout, std::string stderr)
    : code_(code), stdout_(stdout), stderr_(stderr) {}
};

class ErrorKind::BadOutput : public xtd::EnableJsonObject<BadOutput, ErrorKind>
{
  ROYALE_JSON_FIELDS(BadOutput,
      (std::string, stdout)
      (std::string, stderr)
    );
public:
  BadOutput() = default;

  BadOutput(std::string stdout, std::string stderr)
    : stdout_(stdout), stderr_(stderr) {}
};

} // namespace royale

#endif // INCL_ROYALE_ERRORKIND_HPP
