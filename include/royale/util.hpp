#ifndef INCL_ROYALE_UTIL_HPP
#define INCL_ROYALE_UTIL_HPP

#include <cstdlib>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <utility>
#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>
#include <boost/variant.hpp>
#include <boost/filesystem.hpp>
#include <boost/coroutine/all.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/preprocessor/variadic/to_seq.hpp>
#include <boost/preprocessor/seq/variadic_seq_to_seq.hpp>
#include <boost/preprocessor/punctuation/comma.hpp>
#include <boost/preprocessor/facilities/overload.hpp>
#include <boost/preprocessor/seq/for_each.hpp>

using json = nlohmann::json;

/**
 * Macro which generates feature testing traits, to allow enabling features
 * based on what a given type supports. The tests provide ::value member
 * which is true if the given expr can compile correctly with the given
 * type; false otherwise
 *
 * var is a value of the type being tested
 */
#define ROYALE_MAKE_SUPPORT_TEST(name, var, expr) template <typename T> \
struct supports_##name##_impl { \
    template<typename U> static auto test(U var) -> \
      decltype((expr), std::true_type()); \
    template<typename U> static auto test(...) -> std::false_type; \
    using type = decltype(test<T>(std::declval<T>())); \
}; \
template<typename T> \
constexpr bool supports_##name() { \
  return supports_##name##_impl<T>::type::value; } \
template<typename T> \
struct supports_##name##_t : supports_##name##_impl<T>::type {}

/**
 * Iterate over a json object, with var as the iterator
 **/
#define ROYALE_JSON_OBJ_FOR(var, obj) \
  for (auto var = obj.begin(); var != obj.end(); ++var)

namespace royale { namespace xtd {

template<bool Pred, typename T = void>
using enable_if = typename std::enable_if<Pred, T>::type;

template<typename T>
using decay = typename std::decay<T>::type;

template<typename L, typename R>
constexpr bool is_same() { return std::is_same<L, R>::value; }

template<typename L, typename R>
constexpr bool decayed_is_same() { return is_same<decay<L>, decay<R>>(); }

template<typename L, typename R>
constexpr bool is_base_of() { return std::is_base_of<L, R>::value; }

/*
template<typename T, typename... Args>
inline std::unique_ptr<T> make_unique(Args&&... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}*/

template<typename T>
inline std::unique_ptr<decay<T>> into_unique(T&& t) {
  return std::make_unique<decay<T>>(std::forward<T>(t));
}

template<typename T>
inline std::shared_ptr<decay<T>> into_shared(T&& t) {
  return std::make_shared<decay<T>>(std::forward<T>(t));
}

template<typename T>
inline std::string pretty_name() {
  return boost::typeindex::type_id<T>().pretty_name();
}

template<typename T>
struct lazy_pretty_name_t
{
  friend std::ostream &operator<<(std::ostream &o, const lazy_pretty_name_t &)
  {
    o << pretty_name<T>();
    return o;
  }
};

template<typename T>
inline lazy_pretty_name_t<T> lazy_pretty_name() {
  return {};
}

struct lazy_pretty_name_runtime_t
{
  boost::typeindex::type_index tid;

  friend std::ostream &operator<<(std::ostream &o,
      const lazy_pretty_name_runtime_t &i)
  {
    o << i.tid.pretty_name();
    return o;
  }
};

template<typename T>
inline lazy_pretty_name_runtime_t lazy_pretty_name_runtime(const T &val) {
  return {boost::typeindex::type_id_runtime(val)};
}

template<typename Logger>
static void log_exception(Logger log, const char *context, std::exception_ptr e)
{
  try {
    if (e) {
      std::rethrow_exception(e);
    }
  } catch (const boost::coroutines::detail::forced_unwind&) {
    throw;
  } catch (const boost::system::system_error &e) {
    log->error("{} {}[{}]: {}", context,
        xtd::lazy_pretty_name_runtime(e),
        e.code().category().name(),
        e.what());
  } catch (const std::exception &e) {
    log->error("{} {}: {}", context,
        xtd::lazy_pretty_name_runtime(e), e.what());
  } catch (...) {
    log->critical("{} unknown exception thrown", context);
  }
}

template<typename T = void, typename... Args>
T discard(Args&&...) {}

template<typename T>
class pair_inserter
{
public:
  pair_inserter(T &c) : container_(&c) {}

  template<typename L, typename R>
  pair_inserter operator()(L&& l, R&& r) {
    container_->emplace_back(std::forward<L>(l), std::forward<R>(r));
    return *this;
  }
private:
  T *container_;
};

template<typename T, typename O>
class vector_inserter
{
public:
  vector_inserter(T &c, O &o)
    : container_(&c), owner_(&o) {}

  O &done() const {
    return *owner_;
  }

  O &operator()() const {
    return done();
  }

  template<typename V>
  vector_inserter operator()(V&& v) {
    container_->emplace_back(std::forward<V>(v));
    return *this;
  }

private:
  T *container_;
  O *owner_;
};

template<typename T>
class map_inserter
{
public:
  map_inserter(T &c) : container_(&c) {}

  template<typename L, typename R>
  map_inserter operator()(L&& l, R&& r) {
    container_->emplace(std::piecewise_construct,
        std::forward_as_tuple(std::forward<L>(l)),
        std::forward_as_tuple(std::forward<R>(r)));
    return *this;
  }
private:
  T *container_;
};

template<typename T, typename O>
class finalizable_map_inserter : public map_inserter<T>
{
public:
  finalizable_map_inserter(T &c, O &o) : map_inserter<T>(&c), owner_(o) {}

  O &done() const {
    return owner_;
  }

  O &operator()() const {
    return done();
  }

private:
  O *owner_;
};

template<size_t N>
using index = std::integral_constant<size_t, N>;

inline bool ends_with(const char *s, size_t slen,
    const char *suffix, size_t suflen)
{
  if (slen < suflen) {
    return false;
  }
  return std::strncmp(s + slen - suflen, suffix, suflen) == 0;
}

inline bool ends_with(const char *s, size_t slen, const char *suffix)
{
  return ends_with(s, slen, suffix, std::strlen(suffix));
}

inline bool ends_with(const char *s, const char *suffix, size_t suflen)
{
  return ends_with(s, std::strlen(s), suffix, suflen);
}

inline bool ends_with(const char *s, const char *suffix)
{
  return ends_with(s, std::strlen(s), suffix, std::strlen(suffix));
}

inline bool ends_with(const std::string &s, const char *suffix, size_t len)
{
  return ends_with(s.c_str(), s.size(), suffix, len);
}

template<size_t N>
inline bool ends_with(const std::string &s, const char (&suffix)[N])
{
  return ends_with(s, suffix, N - 1);
}

inline bool ends_with(const std::string &s, const std::string &suffix)
{
  return ends_with(s, suffix.c_str(), suffix.size());
}

inline bool begins_with(const char *s, size_t slen,
    const char *prefix, size_t prelen)
{
  if (slen < prelen) {
    return false;
  }
  return std::strncmp(s, prefix, prelen) == 0;
}

inline bool begins_with(const char *s, size_t slen, const char *prefix)
{
  return begins_with(s, slen, prefix, std::strlen(prefix));
}

inline bool begins_with(const char *s, const char *prefix, size_t prelen)
{
  return begins_with(s, std::strlen(s), prefix, prelen);
}

inline bool begins_with(const char *s, const char *prefix)
{
  return begins_with(s, std::strlen(s), prefix, std::strlen(prefix));
}

inline bool begins_with(const std::string &s, const char *prefix, size_t len)
{
  return begins_with(s.c_str(), s.size(), prefix, len);
}

template<size_t N>
inline bool begins_with(const std::string &s, const char (&prefix)[N])
{
  return begins_with(s, prefix, N - 1);
}

inline bool begins_with(const std::string &s, const std::string &prefix)
{
  return begins_with(s, prefix.c_str(), prefix.size());
}

template<typename T>
inline double dbl(T&& t)
{
  return boost::get<double>(std::forward<T>(t));
}

template<typename T>
inline double &dbl_ref(T &t)
{
  return boost::get<double>(t);
}

template<typename T>
inline const double &dbl_ref(const T &t)
{
  return boost::get<double>(t);
}

template<typename T>
inline std::string str(T&& t)
{
  return boost::get<std::string>(std::forward<T>(t));
}

template<typename T>
inline std::string &str_ref(T &t)
{
  return boost::get<std::string>(t);
}

template<typename T>
inline const std::string &str_ref(const T &t)
{
  return boost::get<std::string>(t);
}

template<typename T>
inline double dbl(T&& t, double def)
{
  struct visitor : boost::static_visitor<double> {
    double def;

    visitor(double d) : def(d) {}

    double operator()(double d) const { return d; }
    double operator()(...) const { return def; }
  };
  return boost::apply_visitor(visitor{def}, std::forward<T>(t));
}

template<typename T>
inline std::string str(T&& t, std::string &&def)
{
  struct visitor : boost::static_visitor<std::string> {
    std::string &&def;

    visitor(std::string &&d) : def(d) {}

    std::string operator()(const std::string s) const { return s; }
    std::string operator()(std::string &&s) const { return std::move(s); }
    std::string operator()(...) const { return std::move(def); }
  };
  return boost::apply_visitor(visitor{def}, std::forward<T>(t));
}

template<typename T>
inline std::string str(T&& t, const std::string &def)
{
  struct visitor : boost::static_visitor<std::string> {
    const std::string &def;

    visitor(const std::string &d) : def(d) {}

    std::string operator()(const std::string s) const { return s; }
    std::string operator()(std::string &&s) const { return std::move(s); }
    std::string operator()(...) const { return def; }
  };
  return boost::apply_visitor(visitor{def}, std::forward<T>(t));
}

template<typename T>
inline std::string str(T&& t, const char *def)
{
  struct visitor : boost::static_visitor<std::string> {
    const char *def;

    visitor(const char *d) : def(d) {}

    std::string operator()(const std::string s) const { return s; }
    std::string operator()(std::string &&s) const { return std::move(s); }
    std::string operator()(...) const { return def; }
  };
  return boost::apply_visitor(visitor{def}, std::forward<T>(t));
}

template<typename T>
inline double to_dbl(T&& t)
{
  struct visitor : boost::static_visitor<double> {
    double operator()(double d) const { return d; }

    double operator()(std::string s) const {
      return boost::lexical_cast<double>(s);
    }
  };
  return boost::apply_visitor(visitor{}, std::forward<T>(t));
}

template<typename T>
inline std::string to_str(T&& t)
{
  struct visitor : boost::static_visitor<std::string> {
    std::string operator()(double d) const
    {
      return boost::lexical_cast<std::string>(d);
    }

    std::string operator()(std::string s) const { return s; }
  };
  return boost::apply_visitor(visitor{}, std::forward<T>(t));
}

template<typename T0, typename T1, typename T2>
inline bool within(T0 val, T1 min, T2 max)
{
  return (val >= min ) && (val <= max);
}

template<typename T>
inline bool among(T)
{
  return false;
}

template<typename T, typename V, typename... O>
inline bool among(T val, V first, O&&... others)
{
  if (val == first) {
    return true;
  } else {
    return among(std::move(val), std::forward<O>(others)...);
  }
}

template<typename T>
inline auto get_keys(const T &map) ->
  std::vector<typename T::key_type>
{
  using Key = typename T::key_type;
  std::vector<Key> ret;
  ret.reserve(map.size());
  for (const auto &cur : map) {
    ret.emplace_back(cur.first);
  }
  return ret;
}

template<typename... Funcs>
struct overload_t;

template<typename Func>
struct overload_t<Func> : Func
{
  using Func::operator();

  template<typename This>
  overload_t(This&& t) : Func(std::forward<This>(t)) {}
};

template<typename Func, typename Next, typename... Funcs>
struct overload_t<Func, Next, Funcs...>
  : Func, overload_t<Next, Funcs...>
{
  using Base = overload_t<Next, Funcs...>;
  using Func::operator();
  using Base::operator();

  template<typename This, typename... Others>
  overload_t(This&& t, Others&&... others)
    : Func(std::forward<This>(t)),
      Base(std::forward<Others>(others)...) {}
};

template<typename... Funcs>
inline overload_t<decay<Funcs>...> overload(Funcs&&... funcs)
{
  return {std::forward<Funcs>(funcs)...};
}

template<typename R = void>
struct discarder
{
  template<typename... T>
  R operator()(T &&...) { return R{}; }
};

template<>
struct discarder<void>
{
  template<typename... T>
  void operator()(T &&...) {}
};

/// An object for spawning multiple Boost ASIO coroutines, and waiting until
/// they complete.
class CoroutineWaiter
{
public:
  using io_context = boost::asio::io_context;
  using yield_context = boost::asio::yield_context;
private:
  using steady_timer = boost::asio::steady_timer;

  size_t wait_until_ = std::numeric_limits<size_t>::max();
  size_t counter_ = 0;
  io_context *ioc_;
  steady_timer timer_;

public:
  /// Construct with a given io_context, which will be used to spawn all
  /// coroutines, and perform the wait.
  CoroutineWaiter(io_context &ioc)
    : ioc_(&ioc),
      timer_(*ioc_, steady_timer::clock_type::duration::max()) {}

  /// Spawn a coroutine, with the io_context given in constructor
  template<typename Func>
  void spawn(Func func)
  {
    boost::asio::spawn(*ioc_,
      [this, func = std::move(func)](yield_context yield) mutable {
        func(yield);
        ++counter_;
        if (counter_ == wait_until_) {
          timer_.cancel();
        }
      });
  }

  /// Wait until at least @a wait_until coroutines have completed
  /// Once this returns, this CoroutineWaiter should not be used again.
  void async_wait(size_t wait_until, yield_context yield)
  {
    if (counter_ >= wait_until) {
      timer_.cancel();
      return;
    }
    wait_until_ = wait_until;
    boost::system::error_code ec;
    timer_.async_wait(yield[ec]);
  }
};

ROYALE_MAKE_SUPPORT_TEST(for_each_field, x,
    (for_each_field(x, discarder<bool>{})));

template<typename Signature, typename Func,
  typename Handler>
auto do_async_func(Func func, Handler&& handler) ->
  typename boost::asio::async_result<Handler, Signature>::return_type
{
  using ares = boost::asio::async_result<Handler, Signature>;
  typename ares::completion_handler_type
    handler_(std::forward<Handler>(handler));
  ares result(handler_);
  func(handler_);
  return result.get();
}

struct for_each_field_to_json
{
  json &j;

  template<typename I, typename T>
  bool operator()(I, const char *name, T &&v)
  {
    j[name] = v;
    return true;
  }
};

struct for_each_field_from_json
{
  const json &j;

  template<typename I, typename T>
  bool operator()(I, const char *name, T &&v)
  {
    auto iter = j.find(name);
    if (iter != j.end()) {
      v = iter->get<decay<T>>();
    }
    return true;
  }
};

inline std::string dump(const json &j, int pretty = -1)
{
  return pretty >= 0 ? j.dump(pretty) : j.dump();
}

template<typename T>
struct lazy_json_dump_t
{
  lazy_json_dump_t(const T &v, int pretty = -1) : val_(&v), pretty_(pretty) {}

private:
  const T *val_;
  int pretty_;

  friend std::ostream &operator<<(std::ostream& o, const lazy_json_dump_t &self)
  {
    o << dump(json(*self.val_), self.pretty_);
    return o;
  }
};

template<>
struct lazy_json_dump_t<json>
{
  lazy_json_dump_t(const json &v, int pretty = -1) : val_(&v), pretty_(pretty) {}

private:
  const json *val_;
  int pretty_;

  friend std::ostream &operator<<(std::ostream& o, const lazy_json_dump_t &self)
  {
    o << dump(*self.val_, self.pretty_);
    return o;
  }
};

template<typename T>
inline lazy_json_dump_t<T> lazy_json_dump(const T &v, int pretty = -1)
{
  return {v, pretty};
}

template<typename... Args>
struct lazy_string_t
{
  std::tuple<Args...> args;

  template<size_t... N>
  std::string mk_string(std::index_sequence<N...>) const
  {
    return std::string(std::get<N>(args)...);
  }

  std::string mk_string() const
  {
    return mk_string(std::index_sequence_for<Args...>{});
  }

  friend std::ostream &operator<<(std::ostream &o, const lazy_string_t &self)
  {
    o << self.mk_string();
    return o;
  }
};

template<typename... Args>
inline lazy_string_t<Args...> lazy_string_tup(std::tuple<Args...> args)
{
  return {args};
}

template<typename... Args>
inline auto lazy_string(Args&&... args)
  -> lazy_string_t<decay<Args>...>
{
  return lazy_string_tup(std::make_tuple(std::forward<Args>(args)...));
}

template<typename T>
inline auto default_to_json(json &j, T &&t) ->
  enable_if<supports_for_each_field<T>()>
{
  SPDLOG_TRACE(spdlog::get("json"),
      "Entering default_to_json<{}>", lazy_pretty_name<T>());

  for_each_field(t, for_each_field_to_json{j});

  SPDLOG_TRACE(spdlog::get("json"),
      "Leaving default_to_json<{}> ({})",
      lazy_pretty_name<T>(), lazy_json_dump(j));
}

template<typename T>
inline auto default_from_json(const json &j, T &&t) ->
  enable_if<supports_for_each_field<T>()>
{
  SPDLOG_TRACE(spdlog::get("json"),
      "Entering default_from_json<{}> ({})",
      lazy_pretty_name<T>(), lazy_json_dump(j));

  for_each_field(t, for_each_field_from_json{j});

  SPDLOG_TRACE(spdlog::get("json"),
      "Leaving default_from_json<{}>", lazy_pretty_name<T>());
}

template<typename T>
inline auto to_json(json &j, T &&t) ->
  enable_if<supports_for_each_field<T>()>
{
  SPDLOG_TRACE(spdlog::get("json"),
      "Entering default from_json<{}>", lazy_pretty_name<T>());

  default_to_json(j, t);

  SPDLOG_TRACE(spdlog::get("json"),
      "Leaving default to_json<{}> ({})",
      lazy_pretty_name<T>(), lazy_json_dump(j));
}

template<typename T>
inline auto from_json(const json &j, T &&t) ->
  enable_if<supports_for_each_field<T>()>
{
  SPDLOG_TRACE(spdlog::get("json"),
      "Entering default from_json<{}> ({})",
      lazy_pretty_name<T>(), lazy_json_dump(j));

  default_from_json(j, t);

  SPDLOG_TRACE(spdlog::get("json"),
      "Leaving default from_json<{}>", lazy_pretty_name<T>());
}

int errchk_throw(const char *fn, const char *file, int line);

inline int errchk(int i, const char *fn, const char *file, int line)
{
  if (i < 0) {
    errchk_throw(fn, file, line);
  }
  return i;
}

#define ROYALE_ERRNO_THROW(fn, args) \
  ::royale::xtd::errchk(fn args, #fn, __FILE__, __LINE__)

std::string file_to_string(const char *filename);

template<typename Base>
class JsonPolymorphicBase
{
private:
  using constructor_map = std::map<std::string,
    std::function<std::unique_ptr<Base>()>>;

  static constructor_map &get_map() {
    static constructor_map map;
    return map;
  }

public:
  template<typename T,
    enable_if<is_base_of<Base, T>(), int> = 0>
  static void register_runtime_construct(const std::string &s)
  {
    register_runtime_construct(s,
        [](){ return std::unique_ptr<Base>(new T()); });
  }

  template<typename T,
    enable_if<is_base_of<Base, T>(), int> = 0>
  static void register_runtime_construct()
  {
    register_runtime_construct<T>(T::type_name());
  }

  static void register_runtime_construct(
      const std::string &s,
      std::function<std::unique_ptr<Base>()> f)
  {
    get_map()[s] = f;
  }

protected:
  static std::unique_ptr<Base> runtime_construct(const std::string &s)
  {
    const auto &map = get_map();
    auto i = map.find(s);
    if (i == map.end()) {
      return nullptr;
    }
    return i->second();
  }

  static std::unique_ptr<Base> runtime_construct(const char *s)
  {
    if (get_map().size() == 0) {
      return nullptr;
    } else {
      return runtime_construct(std::string(s));
    }
  }
};

template<typename Base, typename... Types>
class JsonPolymorphic : public JsonPolymorphicBase<Base>
{
protected:
  std::unique_ptr<Base> ptr;

public:
  Base *get() noexcept { return ptr.get(); }
  const Base *get() const noexcept { return ptr.get(); }

  Base &operator*() noexcept { return *get(); }
  const Base &operator*() const noexcept { return *get(); }

  Base *operator->() noexcept { return get(); }
  const Base *operator->() const noexcept { return get(); }

  explicit operator bool() const noexcept { return get() != nullptr; }

private:
  template<typename... Types2>
  auto init_impl(const char *name, const json &j) ->
    enable_if<(sizeof...(Types2) == 0), bool>
  {
    auto p = this->runtime_construct(name);
    if (!p) {
      return false;
    }
    p->this_from_json(j);
    ptr = std::move(p);
    return true;
  }

  template<typename Type, typename... Types2>
  bool init_impl(const char *type_name, const json &j)
  {
    if (std::strcmp(type_name, Type::type_name()) == 0) {
      std::unique_ptr<Type> val{new Type()};
      *val = j;
      ptr.reset(val.release());
      return true;
    } else {
      return init_impl<Types2...>(type_name, j);
    }
  }

public:
  void init(const char *type_name, const json &j) {
    if (!init_impl<Types...>(type_name, j)) {
      throw std::runtime_error(std::string("Tried to init "
            "JsonPolymorphic with unknown type: ") + type_name);
    }
  }

  JsonPolymorphic() = default;

  JsonPolymorphic(std::nullptr_t) {}

  JsonPolymorphic &operator=(std::nullptr_t)
  {
    ptr = nullptr;
    return *this;
  }

  template<typename T,
    enable_if<is_base_of<Base, T>(), int> = 0>
  JsonPolymorphic(std::unique_ptr<T> p) : ptr(p.release()) {}

  template<typename T,
    enable_if<is_base_of<Base, T>(), int> = 0>
  JsonPolymorphic &operator=(std::unique_ptr<T> &&p)
  {
    ptr.reset(p.release());
    return *this;
  }

  template<typename T, typename... Args>
  void visit(T&& visitor, Args&&... args)
  {
    using boost::typeindex::type_id;
    using boost::typeindex::type_id_runtime;

    bool short_circuit = false;
    using iterate = int[];
    (void) iterate {
      (!short_circuit && type_id<Types>() == type_id_runtime(*ptr.get()) ?
        (visitor(static_cast<Types&>(*ptr),
                 std::forward<Args>(args)...), short_circuit = true, 0) :
        0)...
    };
  }

  template<typename T, typename... Args>
  void visit(T&& visitor, Args&&... args) const
  {
    using boost::typeindex::type_id;
    using boost::typeindex::type_id_runtime;

    bool short_circuit = false;
    using iterate = int[];
    (void) iterate {
      (!short_circuit && type_id<Types>() == type_id_runtime(*ptr.get()) ?
        (visitor(static_cast<const Types&>(*ptr),
                 std::forward<Args>(args)...), short_circuit = true, 0) :
        0)...
    };
  }
};

template<typename Base, typename... Types>
void to_json(json &j, const JsonPolymorphic<Base, Types...>& v)
{
  if (v.get()) {
    const auto *p = v.get();
    const char *key = p->virt_type_name();
    json val = *p;
    if (val.is_null()) {
      val = json::object_t{};
    }
    j[key] = val;
  } else {
    j = nullptr;
  }
}

template<typename Base, typename... Types>
void from_json(const json &j, JsonPolymorphic<Base, Types...>& v)
{
  if (j.is_null()) {
    v = nullptr;
  } else if (j.is_string()) {
    v.init(j.get_ref<const std::string &>().c_str(),
                json(nullptr));
  } else if (j.is_object() && j.size() == 1) {
    v.init(j.begin().key().c_str(), j.begin().value());
  }
}

class JsonObject
{
public:
  BOOST_TYPE_INDEX_REGISTER_CLASS
  virtual ~JsonObject() = default;

protected:
  JsonObject() = default;

public:
  virtual const char *virt_type_name() const = 0;

protected:
  virtual void virt_to_json(json &j) const = 0;
  virtual void virt_from_json(const json &j) = 0;

public:
  void this_to_json(json &j) const { virt_to_json(j); }
  void this_from_json(const json &j) { virt_from_json(j); }

  friend inline void to_json(json &j, const JsonObject& v)
  {
    SPDLOG_TRACE(spdlog::get("json"),
        "Entering JsonObject::to_json");

    v.virt_to_json(j);

    SPDLOG_TRACE(spdlog::get("json"),
        "Leaving JsonObject::to_json ({})", lazy_json_dump(j));
  }

  friend inline void from_json(const json &j, JsonObject& v)
  {
    SPDLOG_TRACE(spdlog::get("json"),
        "Entering JsonObject::from_json ({})", lazy_json_dump(j));

    v.virt_from_json(j);

    SPDLOG_TRACE(spdlog::get("json"),
        "Leaving JsonObject::from_json");
  }
};

template<typename T>
struct identity { using type = T; };

template<typename T, typename Base = JsonObject>
class EnableJsonObject : public Base
{
private:
  EnableJsonObject() = default;

  friend class identity<T>::type;

protected:
  const char *virt_type_name() const override
  {
    return T::type_name();
  }

  void virt_to_json(json &j) const override
  {
    SPDLOG_TRACE(spdlog::get("json"),
        "Entering EnableJsonObject<{}>::virt_to_json", lazy_pretty_name<T>());

    to_json(j, static_cast<const T&>(*this));

    SPDLOG_TRACE(spdlog::get("json"),
        "Leaving EnableJsonObject<{}>::virt_to_json ({})",
        lazy_pretty_name<T>(), lazy_json_dump(j));
  }

  void virt_from_json(const json &j) override
  {
    SPDLOG_TRACE(spdlog::get("json"),
        "Entering EnableJsonObject<{}>::virt_from_json ({})",
        lazy_pretty_name<T>(), lazy_json_dump(j));

    from_json(j, static_cast<T&>(*this));

    SPDLOG_TRACE(spdlog::get("json"),
        "Leaving EnableJsonObject<{}>::virt_from_json",
        lazy_pretty_name<T>());
  }

public:
  BOOST_TYPE_INDEX_REGISTER_CLASS

  friend inline void to_json(json &j, const EnableJsonObject& v)
  {
    SPDLOG_TRACE(spdlog::get("json"),
        "Entering EnableJsonObject<{}>::to_json", lazy_pretty_name<T>());

    v.virt_to_json(j);

    SPDLOG_TRACE(spdlog::get("json"),
        "Leaving EnableJsonObject<{}>::to_json ({})",
        lazy_pretty_name<T>(), lazy_json_dump(j));
  }

  friend inline void from_json(const json &j, EnableJsonObject& v)
  {
    SPDLOG_TRACE(spdlog::get("json"),
        "Entering EnableJsonObject<{}>::from_json ({})",
        lazy_pretty_name<T>(), lazy_json_dump(j));

    v.virt_from_json(j);

    SPDLOG_TRACE(spdlog::get("json"),
        "Leaving EnableJsonObject<{}>::from_json",
        lazy_pretty_name<T>());
  }

  template<typename... Args>
  static std::unique_ptr<T> mk(Args&&... args)
  {
    return std::make_unique<T>(std::forward<Args>(args)...);
  }
};

#define ROYALE_JSON_FIELD_MEMBER_IMPL_2(type, name) \
  type name##_;

#define ROYALE_JSON_FIELD_MEMBER_IMPL_3(type, name, init) \
  type name##_{init};

#define ROYALE_JSON_FIELD_MEMBER_IMPL(...) \
  BOOST_PP_OVERLOAD(ROYALE_JSON_FIELD_MEMBER_IMPL_,__VA_ARGS__)(__VA_ARGS__)

#define ROYALE_JSON_FIELD_MEMBER(r, data, elem) \
  ROYALE_JSON_FIELD_MEMBER_IMPL elem

#define ROYALE_JSON_FIELD_FOREACH_IMPL_2(type, name) \
  #name, e.name##_

#define ROYALE_JSON_FIELD_FOREACH_IMPL_3(type, name, init) \
  ROYALE_JSON_FIELD_FOREACH_IMPL_2(type, name)

#define ROYALE_JSON_FIELD_FOREACH_IMPL(...) \
  BOOST_PP_OVERLOAD(ROYALE_JSON_FIELD_FOREACH_IMPL_,__VA_ARGS__)(__VA_ARGS__)

#define ROYALE_JSON_FIELD_FOREACH(r, data, i, elem) \
  func(index<index_offset + i>{}, ROYALE_JSON_FIELD_FOREACH_IMPL elem);

#define ROYALE_JSON_FIELDS_IMPL(Type, fields) \
private: \
  BOOST_PP_SEQ_FOR_EACH(ROYALE_JSON_FIELD_MEMBER, _, fields) \
  template<typename T, typename Func> \
  friend auto for_each_field(T &&e, Func func) -> \
    xtd::enable_if<xtd::decayed_is_same<T, Type>()> \
  { \
    using xtd::index; \
    const size_t index_offset = 0; \
    BOOST_PP_SEQ_FOR_EACH_I(ROYALE_JSON_FIELD_FOREACH, _, fields) \
  } \
public: \
  BOOST_TYPE_INDEX_REGISTER_CLASS \
  constexpr static size_t field_count = BOOST_PP_SEQ_SIZE(fields); \
  constexpr static const char *type_name() { return #Type; } \
  using Self = Type

#define ROYALE_JSON_FIELDS(Type, fields) \
  ROYALE_JSON_FIELDS_IMPL(Type, BOOST_PP_VARIADIC_SEQ_TO_SEQ(fields))

template<typename Into, typename T>
auto static_as_type(const T &v)
{
  return static_cast<const Into &>(v);
}

template<typename Into, typename T>
auto static_as_type(T &v)
{
  return static_cast<Into &>(v);
}

template<typename Into, typename T>
auto static_as_type(const T &&v)
{
  return static_cast<const Into &&>(v);
}

template<typename Into, typename T>
auto static_as_type(T &&v)
{
  return static_cast<Into &&>(v);
}

#define ROYALE_JSON_SUBFIELDS_IMPL(Type, Base, fields) \
private: \
  BOOST_PP_SEQ_FOR_EACH(ROYALE_JSON_FIELD_MEMBER, _, fields) \
  template<typename T, typename Func> \
  friend auto for_each_field(T &&e, Func func) -> \
    xtd::enable_if<xtd::decayed_is_same<T, Type>()> \
  { \
    using xtd::index; \
    const size_t index_offset = Base::field_count; \
    for_each_field(xtd::static_as_type<Base>(e), func); \
    BOOST_PP_SEQ_FOR_EACH_I(ROYALE_JSON_FIELD_FOREACH, _, fields) \
  } \
public: \
  BOOST_TYPE_INDEX_REGISTER_CLASS \
  constexpr static size_t field_count = Base::field_count + BOOST_PP_SEQ_SIZE(fields); \
  constexpr static const char *type_name() { return #Type; } \
  using Self = Type

#define ROYALE_JSON_SUBFIELDS(Type, Base, fields) \
  ROYALE_JSON_SUBFIELDS_IMPL(Type, Base, BOOST_PP_VARIADIC_SEQ_TO_SEQ(fields))

#define ROYALE_JSON_NO_FIELDS(Type) \
private: \
  template<typename T, typename Func> \
  friend auto for_each_field(T &&e, Func func) -> \
    xtd::enable_if<xtd::decayed_is_same<T, Type>()> \
  { (void)e; (void)func; } \
public: \
  BOOST_TYPE_INDEX_REGISTER_CLASS \
  constexpr static const char *type_name() { return #Type; } \
  using Self = Type

#define ROYALE_ESCAPE_COMMAS(...) __VA_ARGS__

#define ROYALE_JSON_ENUM_CLASS_DECL(r, data, elem) \
  class elem;

#define ROYALE_JSON_ENUM_TYPE_LIST(r, data, elem) \
  , data::elem

#define ROYALE_JSON_ENUM_SEQ(base, enum_name, seq) \
  BOOST_PP_SEQ_FOR_EACH(ROYALE_JSON_ENUM_CLASS_DECL, _, seq) \
  BOOST_TYPE_INDEX_REGISTER_CLASS \
  struct enum_name : ::royale::xtd::JsonPolymorphic<base \
  BOOST_PP_SEQ_FOR_EACH(ROYALE_JSON_ENUM_TYPE_LIST, base, seq) > { \
    using Base = ::royale::xtd::JsonPolymorphic<base \
  BOOST_PP_SEQ_FOR_EACH(ROYALE_JSON_ENUM_TYPE_LIST, base, seq) >; \
    using Base::Base; \
  }

#define ROYALE_JSON_ENUM_NAMED(base, enum_name, ...) \
  ROYALE_JSON_ENUM_SEQ(base, enum_name, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))

#define ROYALE_JSON_ENUM(base, ...) \
  ROYALE_JSON_ENUM_NAMED(base, Enum, __VA_ARGS__)

using Value = boost::variant<double, std::string>;

} // namespace royale::xtd

using xtd::to_json;
using xtd::from_json;
using xtd::Value;

} // namespace royale

namespace nlohmann {
  /*
  template<typename T>
  struct adl_serializer<T,
    xtd::enable_if<xtd::supports_for_each_field<T>()>>
  {
    static void to_json(json &j, const std::unique_ptr<T> &v)
    {
      for_each_field(v, xtd::for_each_field_to_json{j});
    }

    static void from_json(const json &j, std::unique_ptr<T> &v)
    {
      for_each_field(v, xtd::for_each_field_from_json{j});
    }
  };*/

  template<typename T>
  struct adl_serializer<std::unique_ptr<T>,
    ::royale::xtd::enable_if<std::is_default_constructible<T>::value>>
  {
    static void to_json(json &j, const std::unique_ptr<T> &v)
    {
      if (v) {
        j = *v;
      } else {
        j = nullptr;
      }
    }

    static void from_json(const json &j, std::unique_ptr<T> &v)
    {
      using ::royale::xtd::decay;

      if (j.is_null()) {
        v.reset(nullptr);
      } else {
        v.reset(new T());
        *v = j.get<decay<T>>();
      }
    }
  };

  template<>
  struct adl_serializer<std::unique_ptr<std::exception>>
  {
    static void to_json(json &j, const std::unique_ptr<std::exception> &v)
    {
      if (v) {
        j["type"] = boost::typeindex::type_id_runtime(v).pretty_name();
        j["what"] = v->what();
      } else {
        j = nullptr;
      }
    }

    static void from_json(const json &j, std::unique_ptr<std::exception> &v)
    {
      if (!j.is_null()) {
        v.reset(new std::runtime_error(j.at("what").get<std::string>()));
      } else {
        v.reset(nullptr);
      }
    }
  };

  template<>
  struct adl_serializer<::royale::xtd::Value>
  {
    struct visitor : public boost::static_visitor<void> {
      json *j = nullptr;

      visitor() = default;
      visitor(json &j) : j(&j) {}

      void operator()(double v) const
      {
        if (j) *j = v;
      }

      void operator()(const std::string &s) const
      {
        if (j) *j = s;
      }
    };

    static void to_json(json &j, const ::royale::xtd::Value &v)
    {
      boost::apply_visitor(visitor{j}, v);
    }

    static void from_json(const json &j, ::royale::xtd::Value &v)
    {
      if (j.is_number()) {
        v = j.get<double>();
      } else if (j.is_string()) {
        v = j.get<std::string>();
      }
    }
  };

  template<>
  struct adl_serializer<boost::filesystem::path>
  {
    static void to_json(json &j, const boost::filesystem::path &v)
    {
      j = v.string();
    }

    static void from_json(const json &j, boost::filesystem::path &v)
    {
      v = j.get_ref<const std::string&>();
    }
  };
}

#endif // INCL_ROYALE_UTIL_HPP
