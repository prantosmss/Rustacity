# Architecture

Rustacity is two halves joined by a C ABI: a C++ application that owns the
user interface and the project, and a Rust core that owns the audio maths.

## The C++ shell

`src/` is the Audacity 2.4.1 application. It owns the wxWidgets user
interface, the project and track model, the audio I/O and file formats, the
effect framework and every effect dialog. It is legacy: it is not extended,
and where a unit has been ported, the class that used to hold the maths is now
a caller of the Rust core.

## The Rust core

`rust/dsp/` is the `rustacity-dsp` crate. It has no dependencies, and
`src/CMakeLists.txt` runs `cargo build --release` during the CMake build and
links the result as a static library, so a Rust toolchain is required to build
Rustacity at all. The crate is also buildable and testable on its own:

```sh
cd rust
cargo test --workspace
```

## The ABI

The C interface is `rust/dsp/include/rustacity_dsp.h`; the implementations are
in `rust/dsp/src/ffi.rs`. The rules that keep it safe:

- **Opaque handles, one per channel.** A unit exposes `_new`, `_free` and a
  block call (usually `_process_block`), not one call per sample. The
  `rustacity_ebur128_*` entry points are the model.
- **Rust owns the loop.** Block calls take the input and output pointers,
  which may alias, plus the sample count and the current parameter values; the
  iteration over samples happens inside the crate.
- **No C++ type crosses the boundary.** Pointers, `size_t` lengths and scalars
  only - no wxWidgets type, no C++ class, no STL container, no `bool` that is
  really a C++ `bool`.
- **Arguments are validated.** A null or nonsensical argument returns a
  harmless value instead of taking the editor down.
- **The header and the exports must agree.** `scripts/check_dsp_abi.py` fails
  if they differ, so an entry point is added to both in the same commit.

## Panic policy

`panic = "unwind"` in `Cargo.toml`, and every entry point runs under
`ffi::guard`, which catches a panic and returns a fallback. A bug in the core
must yield a wrong value, not an abort: the user may have unsaved work. Do not
set `panic = "abort"`.

## What CI proves

`.github/workflows/cmake_build.yml` is the only workflow. Its jobs:

| job | what it does |
| --- | --- |
| `build` (4 platforms) | builds wxWidgets from the pinned fork (`audacity/wxWidgets`, branch `audacity-fixes-3.1.3`), then configures and builds the application with CMake - which compiles the Rust core and links it - and packages the platform artifact |
| `Rust Tests` | `cargo test --workspace` (unit tests and the golden-value tests) and `python3 scripts/check_dsp_abi.py` |
| `Goldens` | builds and runs any dumper left in `rust/dsp/goldens/capture/` and uploads what they print as an artifact |
| `Publish Release` | on a `rustacity-*` tag only: waits for the builds and the tests, collects the four platform zips and publishes the release |

Because the capture is what makes a goldens comparison meaningful, the build
matrix is also the only check that the ports behave on Windows and macOS;
capture and tests run on Linux only (see [`docs/goldens.md`](goldens.md)).

## Porting a unit to Rust

The rule is **extract, capture, port, delete**, and a port is not finished
until the C++ implementation is deleted. Leaving both copies in place grows
the Rust half without shrinking the C++ half, which turns one implementation
into two to keep in step.

1. **Extract.** Move the maths into a unit that compiles on its own, with no
   wxWidgets, and leave the effect class calling it. Nothing may change in the
   move - the extracted header is a copy, not a revision.
2. **Capture.** Add a dumper to `rust/dsp/goldens/capture/` and append what it
   prints to `rust/dsp/goldens/reference.txt`, while the C++ still exists to
   print it. The dumper mechanics are in
   `rust/dsp/goldens/capture/README.md`; the workflow is in
   [`docs/goldens.md`](goldens.md).
3. **Port.** Write the maths as a module under `rust/dsp/src/` with unit tests
   of its own, then expose it in `ffi.rs` and the header, and thin the C++
   class down to parameter marshalling. Test against the captured values.
4. **Delete.** Remove the C++ maths and the dumper, run `cargo test
   --workspace`, and build.

Where a port has deliberately changed behaviour, or dropped C++ code that was
provably dead, say so in the commit message: the deviation is part of the
port. `src/effects/Biquad.cpp` with `ScienFilter.cpp`, and the four realtime
effects (`BassTreble`, `Phaser`, `Wahwah`, `Distortion`), are the worked
examples - see [`docs/port-status.md`](port-status.md).

The Rust-side detail of each step - crate layout, how to add a module and an
entry point - is in [`rust/README.md`](../rust/README.md).
