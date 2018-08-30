#ifndef INCL_ROYALE_VALUESPEC_HPP
#define INCL_ROYALE_VALUESPEC_HPP

#include <iostream>
#include <utility>
#include <vector>
#include <map>
#include <random>
#include <boost/lexical_cast.hpp>
#include <royale/util.hpp>
#include <boost/preprocessor/variadic/to_seq.hpp>

namespace royale {

class ValueSpec : public xtd::JsonObject
{
public:
  ROYALE_JSON_ENUM_NAMED(ValueSpec, RawEnum,
      Constant,
      Uniform, UniformInt,
      Choose);

  struct Enum;

  /// Return true if to_json on ValueSpec::Enum should save this
  /// object directly as a single json entity, rather than a nested map.
  virtual bool save_direct_value() const { return false; }
  virtual Value sample() const = 0;
};

class ValueSpec::Constant : public xtd::EnableJsonObject<Constant, ValueSpec>
{
  ROYALE_JSON_FIELDS(Constant,
      (Value, val, 0)
    );

public:
  Constant() = default;
  Constant(double val) : val_(val) {}
  Constant(std::string val) : val_(std::move(val)) {}
  Constant(const char *val) : val_(std::string(val)) {}
  Constant(const char *val, size_t size) : val_(std::string(val, size)) {}

  template<size_t Size>
  Constant(const char (&val)[Size]) : val_(std::string(val, Size - 1)) {}

  bool save_direct_value() const override { return true; }
  Value sample() const override
  {
    return val_;
  }

protected:
  friend void to_json(json &j, const Constant &v)
  {
    j = v.val_;
  }

  friend void from_json(const json &j, Constant &v)
  {
    if (j.is_number()) {
      v.val_ = j.get<double>();
    } else if (j.is_string()) {
      v.val_ = j.get<std::string>();
    } else {
      xtd::default_from_json(j, v);
    }
  }
};

struct ValueSpec::Enum : ValueSpec::RawEnum {
  using RawEnum::RawEnum;

  Enum() = default;
  Enum(double v) : RawEnum(Constant::mk(v)) {}
  Enum(std::string v) : RawEnum(Constant::mk(std::move(v))) {}
  Enum(const char *v, size_t size) : RawEnum(Constant::mk(v, size)) {}

  template<size_t Size>
  Enum(const char (&val)[Size]) : RawEnum(Constant::mk(val)) {}
};

struct RandomValueMixin
{
protected:
  static unsigned int gen_seed(unsigned int seed)
  {
    if (seed == -1U) {
      return std::random_device()();
    } else {
      return seed;
    }
  }
};

class ValueSpec::Uniform : public xtd::EnableJsonObject<Uniform, ValueSpec>,
                           public RandomValueMixin
{
  using Base = xtd::EnableJsonObject<Uniform, ValueSpec>;
public:
  using range_type = std::array<double, 2>;

private:
  ROYALE_JSON_FIELDS(Uniform,
      (range_type, range)
      (unsigned int, seed, -1U)
    );

private:
  mutable std::mt19937_64 random_;

public:
  Value sample() const override
  {
    std::uniform_real_distribution<> dist{range_[0], range_[1]};
    auto ret = dist(random_);
    return ret;
  }

  Uniform() : range_{{0, 1}}, random_(gen_seed(seed_)) {}
  Uniform(double low, double high, unsigned int seed = -1U)
    : range_{{low, high}},
      seed_(seed),
      random_(gen_seed(seed)) {}

  Uniform &seed(unsigned int s)
  {
    seed_ = s;
    random_.seed(gen_seed(s));
    return *this;
  }
  unsigned int seed() const { return seed_; }

  Uniform &range(double low, double high)
  {
    range_ = {{low, high}};
    return *this;
  }

  const range_type &range() const { return range_; }

protected:
  friend void to_json(json &j, const ValueSpec::Uniform &v)
  {
    if (v.seed_ == -1U) {
      j = v.range_;
    } else {
      xtd::default_to_json(j, v);
    }
  }

  friend void from_json(const json &j, ValueSpec::Uniform &v)
  {
    if (j.is_array() && j.size() == 2 &&
        j[0].is_number() && j[1].is_number()) {
      v.range_[0] = j[0];
      v.range_[1] = j[1];
    } else {
      xtd::default_from_json(j, v);
      v.random_.seed(gen_seed(v.seed_));
    }
  }
};

class ValueSpec::UniformInt : public xtd::EnableJsonObject<UniformInt, ValueSpec>,
                              public RandomValueMixin
{
  using Base = xtd::EnableJsonObject<UniformInt, ValueSpec>;
  using range_type = std::array<int, 2>;

  ROYALE_JSON_FIELDS(UniformInt,
      (range_type, range)
      (unsigned int, seed, -1U)
    );


private:
  mutable std::mt19937_64 random_{seed_};

public:
  Value sample() const override
  {
    std::uniform_int_distribution<int> dist{range_[0], range_[1]};
    return dist(random_);
  }

  UniformInt() : range_{{0, 1}}, random_(gen_seed(seed_)) {}
  UniformInt(int low, int high, unsigned int seed = -1U)
    : range_{{low, high}},
      seed_(seed),
      random_(gen_seed(seed)) {}

  UniformInt &seed(unsigned int s)
  {
    seed_ = s;
    random_.seed(gen_seed(s));
    return *this;
  }
  unsigned int seed() const { return seed_; }

  UniformInt &range(int low, int high)
  {
    range_ = {{low, high}};
    return *this;
  }

  const range_type &range() const { return range_; }

protected:
  friend void to_json(json &j, const ValueSpec::UniformInt &v)
  {
    if (v.seed_ == -1U) {
      j = v.range_;
    } else {
      xtd::default_to_json(j, v);
    }
  }

  friend void from_json(const json &j, ValueSpec::UniformInt &v)
  {
    if (j.is_array() && j.size() == 2 &&
        j[0].is_number() && j[1].is_number()) {
      v.range_[0] = j[0];
      v.range_[1] = j[1];
    } else {
      xtd::default_from_json(j, v);
      v.random_.seed(gen_seed(v.seed_));
    }
  }
};

class ValueSpec::Choose : public xtd::EnableJsonObject<Choose, ValueSpec>,
                          public RandomValueMixin
{
  using Base = xtd::EnableJsonObject<Choose, ValueSpec>;

public:
  using options_type = std::vector<ValueSpec::Enum>;

private:
  ROYALE_JSON_FIELDS(Choose,
      (options_type, options)
      (unsigned int, seed, -1U)
    );

private:
  mutable std::mt19937_64 random_;

public:
  bool save_direct_value() const override { return seed_ == -1U; }

  Value sample() const override
  {
    if (options_.size() > 0) {
      std::uniform_int_distribution<size_t> dist(0, options_.size() - 1);
      size_t i = dist(random_);
      return options_[i]->sample();
    } else {
      return "<empty>";
    }
  }

  Choose() : random_(gen_seed(seed_)) {}

  Choose(options_type &&i, unsigned int seed = -1U)
    : options_(std::move(i)),
      seed_(seed),
      random_(gen_seed(seed)) {}


  xtd::vector_inserter<options_type, Choose> extend_options()
  {
    return {options_, *this};
  }

  Choose(ValueSpec::Enum &&v)
    : random_(gen_seed(-1U))
  {
    options_.emplace_back(std::move(v));
  }

  template<typename... Args,
    xtd::enable_if<(sizeof...(Args) > 1),int> = 0>
  Choose(Args&&... args)
    : random_(gen_seed(-1U))
  {
    options_.reserve(sizeof...(args));
    int dummy[] = {
      (options_.emplace_back(std::forward<Args>(args)), 0)...
    };
    (void)dummy;
  }

  Choose &seed(unsigned int s)
  {
    seed_ = s;
    random_.seed(gen_seed(s));
    return *this;
  }
  unsigned int seed() const { return seed_; }

protected:
  friend void to_json(json &j, const ValueSpec::Choose &v)
  {
    SPDLOG_TRACE(spdlog::get("log"), "Entering Choose::to_json");
    if (v.seed_ != -1U) {
      xtd::default_to_json(j, v);
    } else {
      j = v.options_;
    }
    SPDLOG_TRACE(spdlog::get("log"), "Leaving Choose::to_json ({})",
        xtd::lazy_json_dump(j));
  }

  friend void from_json(const json &j, ValueSpec::Choose &v)
  {
    SPDLOG_TRACE(spdlog::get("log"), "Entering Choose::from_json ({})",
        xtd::lazy_json_dump(j));
    if (j.is_array()) {
      v.options_ = j.get<options_type>();
    } else {
      xtd::default_from_json(j, v);
      v.random_.seed(gen_seed(v.seed_));
    }
    SPDLOG_TRACE(spdlog::get("log"), "Entering Choose::from_json");
  }
};

inline void to_json(json &j, const ValueSpec::Enum &v)
{
  SPDLOG_TRACE(spdlog::get("log"),
      "Entering to_json ValueSpec::Enum overload");
  if (v->save_direct_value()) {
    SPDLOG_TRACE(spdlog::get("log"),
        "ValueSpec::Enum to_json converting directly to value");
    to_json(j, *v);
  } else {
    SPDLOG_TRACE(spdlog::get("log"),
        "ValueSpec::Enum to_json doing full conversion");
    to_json(j, static_cast<const ValueSpec::Enum::Base &>(v));
  }
  SPDLOG_TRACE(spdlog::get("log"),
      "Leaving to_json ValueSpec::Enum overload ({})",
      xtd::lazy_json_dump(j));
}

inline void from_json(const json &j, ValueSpec::Enum &v)
{
  SPDLOG_TRACE(spdlog::get("log"),
      "Entering from_json ValueSpec::Enum overload ({})",
      xtd::lazy_json_dump(j));
  if (j.is_number()) {
    SPDLOG_TRACE(spdlog::get("log"),
        "ValueSpec::Enum from_json shorthand numeric Constant");
    v = ValueSpec::Constant::mk(j.get<double>());
  } else if (j.is_string()) {
    SPDLOG_TRACE(spdlog::get("log"),
        "ValueSpec::Enum from_json shorthand string Constant");
    v = ValueSpec::Constant::mk(j.get<std::string>());
  } else if (j.is_array()) {
    SPDLOG_TRACE(spdlog::get("log"),
        "ValueSpec::Enum from_json shorthand Choose");
    /*
    auto ptr = ValueSpec::Choose::mk();
    ptr->this_from_json(j);
    v = std::move(ptr);*/
    ValueSpec::Choose choose;
    //ptr = j.get<std::unique_ptr<ValueSpec::Choose>>();
    from_json(j, choose);
    v = xtd::into_unique(std::move(choose));
  } else {
    SPDLOG_TRACE(spdlog::get("log"),
        "ValueSpec::Enum from_json no shorthand");
    from_json(j, static_cast<ValueSpec::Enum::Base &>(v));
  }
  SPDLOG_TRACE(spdlog::get("log"),
      "Leaving from_json ValueSpec::Enum overload");
}

} // namespace royale

#endif // INCL_ROYALE_VALUESPEC_HPP
