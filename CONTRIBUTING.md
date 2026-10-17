# Contributing

Rustacity is a fork of Audacity 2.4.1, and it lives at
<https://github.com/prantosmss/Rustacity>. Issues and pull requests here are
the place to report a problem or propose a change. The [upstream wiki][wiki]
and [forum][forum] describe upstream Audacity: they remain a useful reference
for the application this forked from, but they cannot help with Rustacity
itself, and they are not our support desk.

## Before you start

* **Rust is where audio processing goes.** The C++ tree in `src/` is the
  legacy shell, and it is not extended: new DSP is written in `rust/dsp/`.
  [docs/architecture.md](docs/architecture.md) has the C ABI rules, the panic
  policy and the porting rule; read it before writing DSP.
* **`lib-src/` is vendored third-party code.** It is never edited to make our
  code fit and never restructured.
* **Renames are out of scope.** Files, classes and functions keep the names
  they have; a patch that renames things across the tree is not what this fork
  is doing.
* **What is ported** is in [docs/port-status.md](docs/port-status.md), and what
  is next is in [docs/roadmap.md](docs/roadmap.md), so that a first
  contribution does not duplicate work in flight.

## Building and testing

Building the application needs a C++ toolchain, CMake, a Rust toolchain (the
CMake build runs `cargo build --release` and links the result) and wxWidgets
from the pinned fork. [README.md](README.md) has the short version and
[`.github/workflows/cmake_build.yml](.github/workflows/cmake_build.yml) is the
canonical per-platform recipe - what CI runs is what produces the releases.

Work inside the Rust crate needs neither CMake nor a C++ compiler:

```sh
cd rust
cargo test --workspace
```

Before pushing, run the two checks CI runs first:

```sh
cargo test --workspace             # unit tests and the golden-value tests
python3 scripts/check_dsp_abi.py   # header and exports must agree
```

CI then captures golden values and builds the whole application on four
platforms; expect a run to take about twenty minutes.

## Making a change

* Small commits, one area each, written in the imperative mood. The
  `docs:`/`fix:`/`chore:` prefixes in the log are only a convention; the
  size of a commit is not, because a reviewable commit is one idea.
* Zero compiler warnings is the bar. `cargo fmt` is not enforced and clippy is
  not run in CI, so match the surrounding code by hand.
* **A port is not finished until the C++ implementation is deleted.** Capture
  the reference values while the C++ still exists - the loop is in
  [docs/goldens.md](docs/goldens.md) - and say so in the commit message when a
  port knowingly changes behaviour or drops code that was dead.
* Update the documentation a change makes wrong. `docs/` is indexed from the
  README, and `docs/repository-layout.md` says what each directory is for.
* Contributions must be GPL v2 compatible. Keep the per-file licence headers,
  and by opening a pull request you agree to license your work under the same
  terms as the rest of the project ([LICENSE.txt](LICENSE.txt)).

## Reporting a bug

Open an issue with what you did, what happened, what you expected instead, the
version from *Help > About*, and your platform. A minimal project file or a
short recording is worth more than a description. For a security problem, use
the *Report a vulnerability* form under this repository's Security tab rather
than a public issue.

Unacceptable behaviour is a different matter, and is covered by
[CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md).

[wiki]: https://wiki.audacityteam.org/wiki/Contribute
[forum]: https://forum.audacityteam.org/
