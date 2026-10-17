# Pull requests

Thanks for contributing to Rustacity. [CONTRIBUTING.md](CONTRIBUTING.md) has
the rules; the short version is one idea per commit, no renames, no edits to
`lib-src/`, and the checks below run first.

## What this changes

<!-- One paragraph. Link the issue it addresses, if there is one. -->

## How it was tested

<!-- For example:
- `cd rust && cargo test --workspace`
- `python3 scripts/check_dsp_abi.py`
- a full CI run, if the change touches C++
-->

## Checklist

- [ ] One idea per commit, and the commit messages say why.
- [ ] `cargo test --workspace` passes, and `scripts/check_dsp_abi.py` passes
      if an entry point was added or removed.
- [ ] No file, class or function was renamed, and `lib-src/` was not touched.
- [ ] If this ports a unit: the reference values were captured first, and the
      C++ implementation is deleted.
- [ ] The documentation this change makes wrong has been updated.
