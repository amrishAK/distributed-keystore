# GitHub Actions automation

This directory contains the repository’s workflow and reusable-action definitions for CI/CD.

## Contents

- [actions/setup-toolchain](./actions/setup-toolchain/README.md): prepares the runner for the project’s C/C++ jobs
- [workflows/build-and-validate.yml](./workflows/build-and-validate.yml): main validation gate for pull requests and pushes
- [workflows/build-validation.yml](./workflows/build-validation.yml): reusable test, coverage, and sanitizer matrix
- [workflows/build-and-package.yml](./workflows/build-and-package.yml): release packaging and publishing flow
- [workflows/benchmark.yml](./workflows/benchmark.yml): benchmark execution and comparison

## Setup toolchain action

The custom action validates the expected compiler toolchain on Linux and installs the MinGW toolchain on Windows. It is intentionally lightweight: it checks environment readiness before the build steps in the calling workflow.

## Further reading

For the complete CI/CD process, workflow responsibilities, artifacts, and release behavior, see [../docs/CI_CD_PIPELINES.md](../docs/CI_CD_PIPELINES.md).
