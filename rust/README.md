# The Rust half of Rustacity

`rustacity-dsp` is the DSP core of Rustacity: audio maths that used to live in
`src/effects/`, ported to Rust and linked into the C++ application as a static
library.

## Layout

| path | what it is |
| --- | --- |
| `Cargo.toml` | the workspace; `panic = "unwind"` on purpose, see below |
| `dsp/src/bass_treble.rs` | EffectBassTreble's low/high shelf filters |
| `dsp/src/biquad.rs` | biquad filter design and processing |
| `dsp/src/ebur128.rs` | EBU R128 loudness measurement |
| `dsp/src/phaser.rs` | EffectPhaser's allpass filter |
| `dsp/src/wahwah.rs` | EffectWahwah's wah filter |
| `dsp/src/distortion.rs` | EffectDistortion's waveshaper |
| `dsp/src/ffi.rs` | the C ABI: every entry point, each under a panic guard |
| `dsp/include/rustacity_dsp.h` | the C header the C++ side includes |
| `dsp/goldens/reference.txt` | reference values captured from the original C++ |
| `dsp/goldens/capture/` | dumpers that print those values, one per port in progress |
| `dsp/tests/` | golden-value tests that read that file |

## Building and testing

```sh
cd rust
cargo test --workspace
```

The crate has no dependencies, so this needs nothing but a Rust toolchain. The
C++ build runs `cargo build --release` itself (see `src/CMakeLists.txt`), so a
Rust toolchain is required to build Rustacity at all.

`scripts/check_dsp_abi.py` compares this crate's exports with the C header and
is run by CI; run it yourself after touching either side.

## The ABI

* Every entry point is `#[no_mangle] pub extern "C"` and must be declared in
  `dsp/include/rustacity_dsp.h`. `scripts/check_dsp_abi.py` fails the build if
  the two lists differ, so change both in one commit.
* Pointers and numbers are validated before use: a null or nonsensical argument
  returns a harmless value instead of taking the editor down.
* **No panic may cross the boundary.** Each entry point runs under
  `ffi::guard`, which catches a panic and returns a fallback, so a bug in the
  core yields a wrong value rather than an abort that would cost the user their
  unsaved work. That is why the release profile unwinds; do not set
  `panic = "abort"` again.
* Prefer an opaque handle with block calls - `rustacity_ebur128_*` is the model
  (`_new` / `_free` / `_initialize` / `_process_block`) - over one call per
  sample. The Rust side should own the loop, not the C++ side.

## Porting an effect to Rust

The C++ class stays: it owns the dialog, the track iteration and the buffer
copying. Rust takes everything between the input and the output buffers.

**A port is finished only when the C++ implementation is deleted.** Leaving
both copies in place grows the Rust half without shrinking the C++ half, which
turns one port into two implementations to keep in step - exactly what the
biquad work had to clean up afterwards.

1. Extract the maths into a unit that compiles on its own (no wxWidgets),
   and capture reference values from it *before* changing anything: drop a
   dumper into `dsp/goldens/capture/` (see its `README.md`) and add what it
   printed to `dsp/goldens/reference.txt`. The `Goldens` job of the CMake
   Build workflow builds and runs the dumpers - on a push it rides along, or
   run it alone with *Run workflow* in the Actions tab.
2. Write the maths as a new module with unit tests of its own.
3. Expose it through `ffi.rs` and the header, and thin the C++ class down to
   parameter marshalling.
4. Delete the C++ maths (and, with it, its dumper), then run
   `cargo test --workspace` and a build.

`src/effects/Biquad.{h,cpp}` with `src/effects/ScienFilter.cpp` is the worked
example: the coefficients and the sample loop are Rust, the class is a wrapper,
and the leftover C++ copies are gone.
