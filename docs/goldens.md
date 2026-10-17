# Golden values

A Rust port has to reproduce the C++ implementation it replaces, quirks
included, and once that C++ is deleted there is nothing left to compare
against. So the values are captured while both implementations exist, and
checked in.

They live in `rust/dsp/goldens/reference.txt`, one record per line, with a
header naming the source, the capture run and the format of every record kind.
`rust/dsp/tests/*_golden.rs` read that file: the tests are ordinary `cargo
test` cases, so `cargo test --workspace` in `rust/` is the whole check.

## The loop

1. **Extract** the maths into a unit that compiles on its own - a header with
   no wxWidgets - and leave the effect class calling it, unchanged.
2. **Add a dumper** to `rust/dsp/goldens/capture/`. It includes exactly one
   header from `src/effects/` and prints the values the port must match.
   Pushing is enough: the `Goldens` job in the workflow builds and runs it, or
   run that job alone from the Actions tab with *Run workflow*. Its
   `golden-values` artifact holds one `<unit>.txt` per dumper.
3. **Append** those records to `reference.txt` and extend its header with the
   source, the run that printed them and the record format. This is a manual
   step on purpose: it is where a human confirms that the numbers come from
   the code they claim to come from.
4. **Write** the Rust module and a golden test that reads the records.
5. **Delete** the C++ maths and its dumper.

`capture/` is normally empty - a dumper exists only while a port is in flight,
and it goes when the C++ does. The `Goldens` job tolerates that.

The dumper mechanics (which header to include, how to compile one by hand) are
in `rust/dsp/goldens/capture/README.md`.

## Platform

The checked-in values were captured on Ubuntu 24.04 with GCC and glibc, by the
`Goldens` job, and the tests run there. Floating-point results depend on the
platform's `libm`, so:

- the tests compare with a tolerance rather than for bit equality;
- the Windows and macOS builds are exercised by the build matrix, but the
  golden tests themselves run on Linux only - cross-platform behaviour of the
  ports is not yet tested;
- capturing on another platform is possible, but the values then need a
  header entry saying so, and the tolerances may need revisiting.
