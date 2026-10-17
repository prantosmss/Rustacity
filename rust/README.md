# The Rust half of Rustacity

`rustacity-dsp` is the DSP core of Rustacity: audio maths that used to live in
`src/effects/`, ported to Rust and linked into the C++ application as a static
library. The project-level picture - the two halves, the ABI contract, the
panic policy and the porting rule - is in
[`docs/architecture.md`](../docs/architecture.md); this page is the crate.

## Layout

| path | what it is |
| --- | --- |
| `Cargo.toml` | the workspace; `panic = "unwind"` on purpose, see below |
| `dsp/src/lib.rs` | the module list |
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

## Adding a module or an entry point

1. Write the maths as a module in `dsp/src/` with unit tests of its own, and
   add it to `lib.rs`.
2. Expose it: a `#[no_mangle] pub extern "C"` function in `ffi.rs` under
   `ffi::guard`, plus a declaration in `dsp/include/rustacity_dsp.h`.
3. Run `cargo test --workspace` and `python3 scripts/check_dsp_abi.py`.

The contract those functions have to keep - an opaque handle instead of one
call per sample, Rust owning the loop, arguments validated before use, and no
panic crossing the boundary - is in
[`docs/architecture.md`](../docs/architecture.md). Do not set
`panic = "abort"`: a panic must become a wrong value, not an abort that costs
the user their unsaved work.

## Adding a golden test

Capture the values while the C++ still exists (that is the porting rule, and
the step-by-step is in [`docs/goldens.md`](../docs/goldens.md) with the dumper
mechanics in `dsp/goldens/capture/README.md`), append them to
`dsp/goldens/reference.txt`, then add a test as `dsp/tests/<unit>_golden.rs`.
`dsp/tests/common/mod.rs` loads the reference file for every test in the
directory.
