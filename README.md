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

## Documentation

| | |
| --- | --- |
| [Repository layout](docs/repository-layout.md) | what each top-level directory is, what is ours, what is vendored, and what has been removed |
| [Architecture](docs/architecture.md) | the C++ shell and the Rust core, the C ABI, the panic policy, what CI proves, and how a unit is ported |
| [Port status](docs/port-status.md) | which DSP units are Rust, which are not yet, and why |
| [Golden values](docs/goldens.md) | how each port is checked against the implementation it replaced |
| [Roadmap](docs/roadmap.md) | an FFT in Rust next, then the extension engine |

## Getting Started

Rustacity is built from source with CMake. The build compiles the Rust core and links it, so a Rust toolchain is required in addition to a C++ one, and wxWidgets is not optional: the build wants the pinned fork (`audacity/wxWidgets`, branch `audacity-fixes-3.1.3`).

The canonical recipe per platform is the CI workflow, [`.github/workflows/cmake_build.yml`](.github/workflows/cmake_build.yml) - what it runs is what produces the releases.

Because Rustacity is a fork of Audacity 2.4.1, end-user documentation, forums, and developer references are still available from the upstream [Audacity website](https://www.audacityteam.org/), [Audacity Forum](https://forum.audacityteam.org/), and [Audacity Wiki](https://wiki.audacityteam.org/wiki/For_Developers).

### The Rust DSP core

Parts of the audio processing are implemented in Rust, in the `rustacity-dsp` crate under `rust/dsp/`. The build invokes `cargo` and links the crate as a static library; the C interface between the two halves is `rust/dsp/include/rustacity_dsp.h`.

The ported code is checked against reference values captured from the original C++ implementation (`rust/dsp/goldens/reference.txt`):

```
cd rust
cargo test --workspace
```

The ABI conventions, the panic policy and the rules for porting more of the audio engine to Rust are in [`docs/architecture.md`](docs/architecture.md), with the crate-level detail in [`rust/README.md`](rust/README.md).

## Releasing

Releases are cut from `main` with an annotated tag, whose name is the release
number; the version the application itself reports stays the upstream 2.4.x
string it has always been:

```sh
git tag -a rustacity-0.4.0 -F notes.md   # the tag message is the release prose
git push origin rustacity-0.4.0
```

The tag push runs the `Publish Release` job. It waits for the build matrix and
the Rust tests, takes the four platform zips from the run, names them
`Rustacity-<version>-<platform>.zip`, and publishes the GitHub release with
notes generated from the commits since the previous tag.
