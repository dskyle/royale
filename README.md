# Royale Statistical Model Checker

**Royale SMC** uses JSON documents for configuration, and as interchange format
between the Royale SMC daemon and experiment runner, as well as between the
runner and user-provided experiment executors.

Note: currently the daemon is not implemented.

## Compilation

Royale SMC currently only supports **Linux**, and has only been tested on
**Ubuntu 16.04** with **g++ 5.4**. It requries a recent boost, which includes
`boost::process`. Note that Ubuntu 16.04 does not have a sufficient version
in APT. Royale SMC has been tested with boost version 1.67.

To install necessary prereqs on Ubuntu, run:

```
sudo apt install build-essential g++ libboost-dev
```

Royale SMC uses Boost, but only the header-only parts.

Clone the Royale SMC git repository, including submodules:

```
git clone --recursive $ROYALE_SMC_REPO_URL
```

Compile by running `make`. This will produce the `runner` executable, in `bin`
as well as tests in `bin/tests`.

## Example

To try a simple example, try running the following command:

```
bin/runner -f examples/triangle.experiment.json -x triangles -R 2 -P
```

Which should produce output similar to:

```
[
  {
    "input": {
      "experiment_name": "triangles",
      "replicate": null,
      "sample": {
        "x0": 3.6924800226244345,
        "x1": 1.487196543731423,
        "x2": 7.22640880481649,
        "y0": 2.96838391329332,
        "y1": 5.009947149023825,
        "y2": 0.40003738476995365
      }
    },
    "status": {
      "Complete": {
        "output": {
          "aux": {
            "angles": [
              -3.0231925430431152,
              0.0701586340957947,
              -0.04824147645088406
            ]
          },
          "preds": {
            "acute": false
          },
          "replicate": null
        },
        "stderr": ""
      }
    }
  },
  {
    "input": {
      "experiment_name": "triangles",
      "replicate": null,
      "sample": {
        "x0": 7.540708068633011,
        "x1": 7.964798165119306,
        "x2": 9.104731053232861,
        "y0": 1.20221927312986,
        "y1": 5.876173056513179,
        "y2": 7.6908707636737805
      }
    },
    "status": {
      "Complete": {
        "output": {
          "aux": {
            "angles": [
              -0.14604089103165174,
              2.6712063020106864,
              -0.3243454605474547
            ]
          },
          "preds": {
            "acute": false
          },
          "replicate": null
        },
        "stderr": ""
      }
    }
  }
]
```

In this simple example, each job creates a random triangle, by sampling from a
uniform distribution for each coordinate. The job "succeeds" if the resulting
triangle is acute.

Run `bin/runner -h` for a description of available options, and read on for
details of the JSON used for input, configuration, and output.

## Deployment

TODO once daemon is implemented. For now, run `runner` directly with the `-r`
option to test running experiments.

## Experiments

An **Experiment** in Royale SMC represents a particular system, in combination
with an **Input Specification** which defines the random inputs to that system.
An experiment is typically run many times. Each run is called a **Job**

### Experiment Executor

Royale SMC's interaction with a system flows through a user-provided executor,
an executable which accepts inputs in JSON format on standard input, and
provides output in JSON format on standard output. Royale SMC logs the process
standard error, but does not ascribe any meaning to it.

See `examples/` for an example of experiment executor `triangle_executor.py`
and experiment definition `triangle.experiment.json`.

### Experiment Definitions

At startup, the Royale SMC runner must be provided a set of experiments. These
experiments may be provided directly as command line strings, with the `-j` or
`--json` arguments, in a specific files, with the `-f` or `--file` to read from
a file, or `-d` or `--directory` to read all files in that directory
with extension `.experiment.json` as separate experiments. An arbitrary number
of these options may be used.

Experiments are defined as a JSON object with the following fields:

* `name`: an identifier naming this experiment. Must be unique among all
experiments registered with Royale SMC.

* `version`: a version string. Royale SMC does not ascribe any particular
meaning to this string, except that results from different versions of an
experiment will not be aggregated together.

* `timeout`: optional. A number of seconds (may be fractional) to allow the
experiment to run. If the experiment exceeds this time, it will be killed an
treated as an error. Experiments have an internal timeout that results in
failure outcome that is shorter than this timeout.

* `cd`: optional. Runner will change directory, as if by the `cd` shell command,
when starting the experiment executor. This is relative to the working directory
of the runner process.

* `cmd`: an array of strings, of at least one element. This is the command to
run the experiment executor. The first element is the executable name. The
others are arguments to pass to the executable.

* `env`: optional. A JSON object with arbitrary keys, each to a string value.
Each key-value pair will become a new environment variable when running the
executor.

* `input`: an Input Specification JSON object, described in the following
section.

### Input Specification

An Input Specification defines the input variables the experiment will be
provided by Royale SMC. These are the inputs which will be analyzed for
input attribution. Input Specification is defined by a JSON object, where
each field names a variable to generate. The value is a `Value Specification`,
one of the following:

* a number or string: the variable will be assigned the given value in each
Job. This is useful for passing configuration through input variables. These
variables will be ignored for input attribution. This is a shorthand for the
`Constant` Value Specification, described below.

* an array: when generating inputs, a random element will be chosen, and
interpreted as a Value Specification and sampled from recursively. Typically,
though, the elements are numeric or string constants. This is a shorthand for
the `Choose` Value Specification, described below.

* an object with a single field: the name of the field defines the type of
Value Specification, while the value defines any parameters. All Value
Specifications can be given as objects, with fields described below. In
addition, most offer shorthand for which uses just one parameter directly,
instead of as part of an object. This form leaves all other parameters default.
For example, a Uniform value may be specified as:

```
{"x": {"Uniform": {"range": [0, 1]}}}
```

or

```
{"x": {"Uniform": [0, 1]}}
```

All randomized Value Specification can take a `seed` parameter. If unspecified,
or given the value `-1`, the seed will be generated randomly, from an
unpredictable source. Otherwise, the given seed will be used, and will ensure
that a sequence of Jobs will always produce the same set of variates.

The following types of Value Specification are available:

* `Constant`: an object with the field `val`, which must contain a number or
string. In shorthand form, `val` is implied.

* `Choose`: an object with the field `options`, which is an array of other
Value Specifications. When sampled, will pick one randomly (equal chance),
and sample it recursively. Optionally may include a field `seed`. In shorthand
form, `options` is implied.

* `Uniform`: an object with the field `range`, which is a two-sized array. The
first element is the minimum, and second maximum, generated uniform distributed
double-precision floating-point value. Optionally may include a field `seed`.
In shorthand form, `range` is implied.

* `UniformInt`: same as `Uniform`, except generates uniform integers between
given minimum and maximum (inclusive).

### Samples

When spawning a Job, an experiment's Input Specification will be sampled to
produce a **Sample**. This is an object with the same fields as the Input
Specification, but for each key, a value is drawn from the given distribution.
These values are always numbers or strings.

## Jobs

When `runner` executes a Job, it will pass input on standard input. The executor
should parse the JSON provided, described below, run the experiment, and return
by printing output JSON, also described below, on standard output. Any standard
error output the executor produces is captured and logged, but otherwise ignored
by Royale SMC itself.

### Job Input

Job input is given as a JSON object with the following fields:

* `experiment_name`: the name of the experiment to run. `runner` will report
an error if this experiment is unknown to it.

* `sample`: a Sample object. The inputs to use when running the experiment.

* `replicate`: a JSON value, either `null`, or a value previously returned by
the output of an experiment run by the same executor. This value is meant to
capture whatever information is needed to replicate previous results, or at
least, to ensure they are consistent with other Jobs as expected. At the least,
this object should contain Git hashes or other repository version information
for the software the executor is running.

### Job Output

Job output is given as a JSON object with the following fields:

* `preds`: an object mapping keys to boolean values. These are the outputs that
will be analyzed for input attribution. An experiment may produce an arbitrary
number of outputs, but all must be boolean.

* `aux`: an object mapping keys to arbitrary json values. These are additional
data that might be useful for user analysis, but will be ignored for input
attribution analysis.

* `replication`: see Job Input. TODO: not currently used.
