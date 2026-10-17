# Golden-value capture

The dumpers here compile a DSP unit of the C++ tree **on its own** (plain
`g++`, no wxWidgets, no CMake) and print the values the Rust port has to
reproduce byte for byte. They are the capture step of the porting rule in
[`docs/architecture.md`](../../../../docs/architecture.md): *extract, capture,
port, delete*. A dumper and the C++ unit it captured are deleted together,
when the port is finished.

Each dumper includes exactly one header from `src/effects/` — the unit that
was moved out of its effect class for the capture. Anything a dumper needs
from the outside is therefore a signal that the unit is not free of
wxWidgets yet.

## Running them

They are built and run by the `Goldens` job in
`.github/workflows/cmake_build.yml`, which uploads everything they printed as
the `golden-values` artifact:

* on a push (or a pull request) the job rides along with the rest of CI;
* from the Actions tab, *Run workflow* runs the dumpers alone, without the
  four platform builds (the `Goldens` job is the only job that runs).

The job prints one `<unit>.txt` per dumper. Take those records, append them
to `dsp/goldens/reference.txt`, and document them (source, capture run,
record format) in that file's header. The tests in `dsp/tests/` then read
them like any other record.

To run a dumper by hand, from the root of the checkout:

```sh
g++ -std=c++17 -O2 -Isrc/effects -o /tmp/<unit> <unit>.cpp
/tmp/<unit>
```

The printed values depend on the C++ standard library's `libm`, so capture
them on the same platform the checked-in values came from (Ubuntu, GCC/glibc)
rather than locally, unless you know you need them from another platform.
