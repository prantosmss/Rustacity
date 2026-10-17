# Repository layout

Rustacity is a fork of Audacity 2.4.1. The application is still the upstream
C++ tree; its audio maths is being replaced, unit by unit, by the Rust core in
`rust/`. This page says what every top-level entry is, which parts are ours,
and which parts are inherited and must not be touched.

## Top level

| entry | what it is | in the build |
| --- | --- | --- |
| `src/` | the C++ application: the wxWidgets shell, the effect classes, the project and track model, the file formats | yes - `add_subdirectory( src )`, and `src/CMakeLists.txt` runs `cargo` and links the Rust core |
| `rust/` | the Rust DSP core, crate `rustacity-dsp` (see [`docs/architecture.md`](architecture.md)) | yes - built by cargo, linked as a static library |
| `include/` | headers shared across `src/` | yes |
| `lib-src/` | vendored third-party libraries (portaudio, libsndfile, LV2, libvorbis, ...) plus `lib-src/FileDialog/` | yes, through `cmake-proxies/` |
| `cmake-proxies/` | the CMake project that builds each vendored library, plus the project's own CMake modules | yes |
| `help/` | help pages and manual assets installed with the application | yes |
| `images/` | brand assets and theme XPMs; `images/brand/` holds the SVG sources | yes |
| `locale/` | application translations: the `.po` catalogs, `LINGUAS` and `LanguageNames.txt` | yes |
| `nyquist/` | the Nyquist language runtime resources and system plug-ins | yes |
| `plug-ins/` | bundled Nyquist plug-ins | yes |
| `presets/` | shipped preset data (`EQDefaultCurves.xml`) | yes |
| `scripts/` | our tooling - `scripts/brand/` (brand asset generation), `scripts/check_dsp_abi.py` (the ABI gate), `scripts/maketarball.cmake` - alongside inherited wiki and image utilities | partly |
| `win/` | the Windows installer project (`Inno_Setup_Wizard/`), the application resource script and the icons it embeds, and `LADSPA_plugins-win/` | yes for `rustacity.rc` and the icons; the installer is not built |
| `mac/` | macOS bundle resources (`Resources/`), the bundle wrapper `Wrapper.c`, `Info.plist`, signing entitlements, and the local wxWidgets build helper | yes for `Wrapper.c` and `Resources/` |
| `.github/` | CI - one workflow, `cmake_build.yml` | yes |
| `docs/` | this documentation | no |

At the root, `CMakeLists.txt` is the build entry point. `README.md` is the
front page, `LICENSE.txt` the GPL (with the upstream attribution intact),
`CONTRIBUTING.md`, `CODE_OF_CONDUCT.md` and `PULL_REQUEST_TEMPLATE.md` the
inherited GitHub meta-files.

## What is ours

- `src/`, and everything the Rust migration touches inside it.
- `rust/`.
- `scripts/brand/`, `scripts/check_dsp_abi.py`, `scripts/maketarball.cmake`.
- `docs/`, `README.md`, `.github/workflows/cmake_build.yml`.
- The brand assets in `images/` and the icons and installer art in `win/` and
  `mac/Resources/`, all generated from `images/brand/` by
  `scripts/brand/build_brand_assets.py`.

Everything else is inherited from Audacity 2.4.1 or vendored third-party code.
`lib-src/` is vendored: it is never edited to make our code fit, and never
restructured.

## Why GitHub reports a fraction of a percent of Rust

GitHub counts bytes of code per language on the default branch. Before the
cleanup below its numbers were: C 36%, C++ 22%, Python 14%, Shell 12%,
Makefile 8%, M4 1.3%, Common Lisp 1.3%, and **Rust 0.22%**.

That is accurate, not stale. `src/` is about 318,000 lines of C++ and the Rust
core is about 3,500 lines: the bar will not move until `src/` is ported. The
C and Python and Makefile shares are vendored `lib-src/` and build
scaffolding, which is why they are worth being able to tell apart - hence this
page. The only metric that tracks progress honestly is the ledger in
[`docs/port-status.md`](port-status.md).

## Inherited infrastructure that was removed

The repository used to carry two CI systems, an entire second build system and
the IDE projects for both desktop platforms, none of which any command we run
referenced. It was removed in five commits, each of them checked with `git
grep` across the CMake tree and the workflow before anything was deleted:

| commit | what went |
| --- | --- |
| `chore: retire the unused CI configurations` | `.github/workflows/build.yml` (dormant: it triggered only on a branch named `disable`), `.travis.yml`, `appveyor.yml` |
| `chore: drop the autotools build` | `configure`, `configure.ac`, `Makefile.am`/`Makefile.in`, `autotools/`, `m4/`, `INSTALL`, `ABOUT-NLS`, `scripts/maketarball.sh`, `locale/Makefile.in.in`, `nyq-po/`, the `po` symlink to `locale/` |
| `chore: drop the unused doxygen config and QA scripts` | `rustacity.dox`, `dox2-src/`, `qa/` |
| `chore: drop the autotools-only tests and upstream notes` | `tests/` (built and run by autotools only - the CMake build never compiled it), `todo.txt`, upstream `README.txt` and `CHANGELOG.txt` |
| `chore: retire the Visual Studio and Xcode projects` | `win/rustacity.sln`, `win/Projects/`, `win/wxWidgets_additions/`, the MSBuild props/targets/rules, `Directory.Build.props`, `mac/Rustacity.xcodeproj/`, `mac/wxMac_additions/`, `mac/config/`, the xcconfigs, and the Xcode/dmg packaging scripts |

The CMake build is the only build. It generates its own Xcode project and
Visual Studio solution, it drives the translations from `locale/` itself, and
it needs the Windows installer to be revisited separately: see
`win/Inno_Setup_Wizard/rustacity.iss`, which still reads the old `..\release\`
output tree.

## Is a file still load-bearing?

Before deleting anything, check whether the build names it:

```sh
# everything the CMake project reads, plus the workflow
git grep -n "<name>" -- CMakeLists.txt cmake-proxies src/*/CMakeLists.txt \
    .github/workflows
```

A path that appears nowhere but in the file being considered, and in
`.gitignore`, is dead weight of the kind listed in the table above.
