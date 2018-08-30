#define CATCH_CONFIG_RUNNER
#include "catch.hpp"

#include "royale/Runner.hpp"

using namespace royale;

using xtd::dbl;
using xtd::str;
using xtd::str_ref;
using xtd::within;
using xtd::among;

class Zero : public xtd::EnableJsonObject<Zero, ValueSpec>
{
  ROYALE_JSON_NO_FIELDS(Zero);

public:
  Zero() = default;

  Value sample() const override
  {
    return 0;
  }
};

class Hello : public xtd::EnableJsonObject<Hello, ValueSpec>
{
  ROYALE_JSON_NO_FIELDS(Hello);

public:
  Hello() = default;

  Value sample() const override
  {
    return "Hello!";
  }
};

TEST_CASE("Experiment", "[experiment]") {
  ValueSpec::Enum::register_runtime_construct("Zero",
      [](){ return Zero::mk(); } );

  ValueSpec::Enum::register_runtime_construct<Hello>();

  Experiment exp;
  Experiment::input_type inputs;
  inputs["x"] = 42;
  inputs["y"] = 47;
  inputs["hello"] = "world";

  exp.name("test")
     .cmd("ls", "-alh", "/")
     .env({
         {"PATH","/bin:/usr/bin"},
         {"ROOT", "/"},
       })
     .inputs(std::move(inputs))
     .extend_env()
          ("A", "1")
          ("B", "2")
          ("C", "3")
     ;

  auto seeded_pick4 = std::move(ValueSpec::Choose(1, 3, 6, 9)
                                  .seed(0));

  exp.extend_inputs()
      ("z", 0)
      ("uniform", ValueSpec::Uniform::mk(1, 10.5))
      ("uniform_int", ValueSpec::UniformInt::mk(1, 20))
      ("default_uniform", ValueSpec::Uniform::mk())
      ("default_uniform_int", ValueSpec::UniformInt::mk())
      ("pick0", ValueSpec::Choose::mk())
      ("pick1", ValueSpec::Choose::mk(1))
      ("pick4", ValueSpec::Choose::mk(2, 4, 6, 8))
      ("pick4str", ValueSpec::Choose::mk("2", "4", "6", "8"))
      ("seeded_uniform", ValueSpec::Uniform::mk(1, 10.5, 0))
      ("seeded_uniform_int", ValueSpec::UniformInt::mk(1, 20, 0))
      ("seeded_pick4", ValueSpec::Choose::mk(std::move(seeded_pick4)))
      ("zero", Zero::mk())
      ("say", Hello::mk())
    ;

  CHECK(exp.name() == "test");

  SECTION("Check command vector") {
    const auto &cmd = exp.cmd();
    REQUIRE(cmd.size() == 3);
    CHECK(cmd.at(0) == "ls");
    CHECK(cmd.at(1) == "-alh");
    CHECK(cmd.at(2) == "/");
  }

  SECTION("Check env map") {
    const auto &env = exp.env();
    REQUIRE(env.size() == 5);
    CHECK(env.at("PATH") == "/bin:/usr/bin");
    CHECK(env.at("ROOT") == "/");
    CHECK(env.at("A") == "1");
    CHECK(env.at("B") == "2");
    CHECK(env.at("C") == "3");
  }

  SECTION("Check inputs map") {
    const auto &i = exp.inputs().inputs();
    REQUIRE(i.size() == 17);

    const auto s = exp.inputs().sample();
    REQUIRE(s.size() == 17);

    CHECK(dbl(s.at("x")) == 42);
    CHECK_THROWS(str(s.at("x")));
    CHECK_THROWS(str_ref(s.at("x")));
    CHECK(str(s.at("x"), "foo") == "foo");
    CHECK(xtd::to_str(s.at("x")) == "42");

    CHECK(dbl(s.at("y")) == 47);
    CHECK(dbl(s.at("z")) == 0);
    CHECK(str(s.at("hello")) == "world");
    CHECK(str_ref(s.at("hello")) == "world");
    CHECK_THROWS(dbl(s.at("hello")));
    CHECK_THROWS(xtd::to_dbl(s.at("hello")));
    CHECK(dbl(s.at("hello"), -12) == -12);

    CHECK(within(dbl(s.at("uniform")), 1, 10.5));
    CHECK(within(dbl(s.at("default_uniform")), 0, 1));

    CHECK(within(dbl(s.at("uniform_int")), 1, 20));
    CHECK(within(dbl(s.at("default_uniform_int")), 0, 1));

    CHECK(str(s.at("pick0")) == "<empty>");
    CHECK(dbl(s.at("pick1")) == 1);
    CHECK(among(dbl(s.at("pick4")), 2, 4, 6, 8));
    CHECK(among(str(s.at("pick4str")), "2", "4", "6", "8"));
    CHECK(among(xtd::to_dbl(s.at("pick4str")), 2, 4, 6, 8));

    CHECK(dbl(s.at("seeded_uniform")) == Approx(2.518036952));
    CHECK(dbl(s.at("seeded_uniform_int")) == Approx(4));
    CHECK(dbl(s.at("seeded_pick4")) == Approx(1));

    CHECK(dbl(s.at("zero")) == 0);
    CHECK(str(s.at("say")) == "Hello!");
  }

  SECTION("Check JSON round-trip") {
    auto s = json(exp).dump();
    std::cerr << s << std::endl;
    CHECK(s == json(json::parse(s)).dump());

    Experiment exp2 = json::parse(s);

    const auto s2 = exp2.inputs().sample();
    REQUIRE(s2.size() == 17);

    CHECK(dbl(s2.at("zero")) == 0);
    CHECK(str(s2.at("say")) == "Hello!");
  }
}

int main(int argc, char *argv[]) {
  auto console = spdlog::stderr_color_st("log");
  auto json_log = spdlog::stderr_color_st("json");
  (void)console;
  (void)json_log;

  int result = Catch::Session().run( argc, argv );

  return result;
}
