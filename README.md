<p align="center">
  <img src="images/rustacity.svg" alt="Rustacity" width="96" height="96">
</p>

# Rustacity

**An [Audacity](https://www.audacityteam.org) v2.4.1 fork created for the Rustaceans.**

Rustacity is an easy-to-use, multi-track audio editor and recorder for Windows, Mac OS X, GNU/Linux and other operating systems. It keeps everything you know about Audacity, and will grow a Rust-based extension engine for adding new functionality.

- **Recording** from any real, or virtual audio device that is available to the host system.
- **Export / Import** a wide range of audio formats, extendible with FFmpeg.
- **High quality** using 32-bit float audio processing.
- **Plug-ins** Support for multiple audio plug-in formats, including VST, LV2, AU.
- **Macros** for chaining commands and batch processing.
- **Scripting** in Python, Perl, or any language that supports named pipes.
- **Nyquist** Very powerful built-in scripting language that may also be used to create plug-ins.
- **Editing** multi-track editing with sample accuracy and arbitrary sample rates.
- **Accessibility** for VI users.
- **Analysis and visualization** tools to analyze audio, or other signal data.

## Roadmap

- **Rust extension engine** — a Rust-based plugin/extension layer for adding new functionality to the editor.

## Getting Started

Rustacity is built from source. The project ships GitHub Actions workflows (`.github/workflows/`) and `appveyor.yml` that produce CI builds.

Because Rustacity is a fork of Audacity 2.4.1, end-user documentation, forums, and developer references are still available from the upstream [Audacity website](https://www.audacityteam.org/), [Audacity Forum](https://forum.audacityteam.org/), and [Audacity Wiki](https://wiki.audacityteam.org/wiki/For_Developers).

### The Rust DSP core

Parts of the audio processing are implemented in Rust, in the `rustacity-dsp` crate under `rust/dsp/`. The build invokes `cargo` and links the crate as a static library, so a Rust toolchain is required in addition to the C++ one; the C interface between the two halves is `rust/dsp/include/rustacity_dsp.h`.

The ported code is checked against reference values captured from the original C++ implementation (`rust/dsp/goldens/reference.txt`):

```
cd rust
cargo test --workspace
```

The ABI conventions, the panic policy and the rules for porting more of the audio engine to Rust are in [`rust/README.md`](rust/README.md).
