<#
.SYNOPSIS
  Local pre-commit validation for native Windows (MinGW), mirrors the
  "Windows MinGW" matrix job in .github/workflows/build-validation.yml.

.DESCRIPTION
  Configures with BUILD_INTEGRATION_TESTS=ON (like CI always does, since the
  concurrency_test target requires it), builds, then runs unit_tests,
  concurrency_test, and any other registered integration tests.

.PARAMETER Clean
  Remove the preset's build directory before configuring.

.EXAMPLE
  validation-scripts/validate_windows.ps1
  validation-scripts/validate_windows.ps1 -Clean
#>
[CmdletBinding()]
param(
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
$preset = 'windows-mingw'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$buildDir = Join-Path $projectRoot "build\$preset"

function Assert-LastExitCode([string]$Step) {
    if ($LASTEXITCODE -ne 0) {
        throw "$Step failed with exit code $LASTEXITCODE"
    }
}

if ($Clean -and (Test-Path $buildDir)) {
    Remove-Item -Recurse -Force $buildDir
}

Push-Location $projectRoot
try {
    Write-Host "[validate] Configuring preset '$preset' (BUILD_INTEGRATION_TESTS=ON)"
    cmake --preset $preset "-DBUILD_INTEGRATION_TESTS=ON"
    Assert-LastExitCode 'CMake configure'

    Write-Host '[validate] Building'
    cmake --build --preset $preset --parallel
    Assert-LastExitCode 'Build'

    Write-Host '[validate] Running unit tests'
    ctest --test-dir $buildDir --output-on-failure -R '^unit_tests$' --no-tests=error
    Assert-LastExitCode 'Unit tests'

    Write-Host '[validate] Running concurrency test'
    ctest --test-dir $buildDir --output-on-failure -R '^concurrency_test$' --no-tests=error
    Assert-LastExitCode 'Concurrency test'

    Write-Host '[validate] Running remaining integration tests (if any)'
    ctest --test-dir $buildDir --output-on-failure -E '^(unit_tests|concurrency_test)$' --no-tests=ignore
    Assert-LastExitCode 'Integration tests'

    Write-Host '[validate] Validation passed.' -ForegroundColor Green
}
finally {
    Pop-Location
}
