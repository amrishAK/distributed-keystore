# Setup toolchain action

This composite action prepares the GitHub runner for the repository’s C/C++ jobs.

## Purpose

The action does two things:
- On Linux, it verifies that the requested compiler toolchain is present and executable.
- On Windows, it installs the MinGW toolchain required by the project.

It does not configure CMake or build the project itself. Those tasks happen in the calling workflow after this action finishes.

## Inputs

### `compiler`

Type: string

Default: `gcc`

Allowed values:
- `gcc`
- `clang`
- `both`

Examples:

```yaml
- uses: ./.github/actions/setup-toolchain
  with:
    compiler: gcc
```

```yaml
- uses: ./.github/actions/setup-toolchain
  with:
    compiler: both
```

## What it verifies

On Linux, the action confirms that the required commands are available:
- `gcc-13`, `g++-13`, and `gcov-13` for GCC builds
- `clang-18` and `clang++-18` for Clang builds
- `cmake`
- `python3`

On Windows, it installs MinGW via `msys2/setup-msys2` and expects the MSYS2 shell environment to resolve the toolchain correctly.

## Further reading

For broader workflow details, status reporting, and release pipeline behavior, see [../../../docs/CI_CD_PIPELINES.md](../../../docs/CI_CD_PIPELINES.md).
