<#
.SYNOPSIS
Formats C/C++ source files in main, Python source files in the project, and
CMake files.

.DESCRIPTION
Uses the repository's .clang-format configuration for C/C++ files, Ruff for
Python files under tests, tools, and utils, and cmake-format for CMake files.
Generated and vendored directories are excluded from Python and CMake scans.

.NOTES
Required tools: clang-format, ruff, and cmake-format.
#>

$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$mainRoot = Join-Path $repositoryRoot 'main'
$clangFormat = Get-Command clang-format -ErrorAction SilentlyContinue

if ($null -eq $clangFormat) {
    throw 'clang-format was not found on PATH.'
}

# Format C/C++ source and header files under main.
$extensions = @('*.c', '*.h', '*.cc', '*.cpp', '*.cxx', '*.hh', '*.hpp', '*.hxx')
$files = Get-ChildItem -Path $mainRoot -Recurse -File -Include $extensions

foreach ($file in $files) {
    Write-Host "Formatting $($file.FullName)"
    & $clangFormat.Source -i --style=file $file.FullName
    if ($LASTEXITCODE -ne 0) {
        throw "clang-format failed for $($file.FullName)"
    }
}

Write-Host "Formatted $($files.Count) file(s) under $mainRoot."

# Format Python files in the project utility and test directories.
$pythonRoots = @('tests', 'tools', 'utils') |
    ForEach-Object { Join-Path $repositoryRoot $_ } |
    Where-Object { Test-Path $_ }
$ruff = Get-Command ruff -ErrorAction SilentlyContinue

if ($null -eq $ruff) {
    throw 'ruff was not found on PATH; it is required to format Python files.'
}

$pythonFiles = $pythonRoots |
    ForEach-Object {
        Get-ChildItem -Path $_ -Recurse -File -Filter '*.py'
    } |
    Where-Object {
        $_.FullName -notmatch '[\\/](build|\.venv|__pycache__)[\\/]'
    }

if ($pythonFiles.Count -gt 0) {
    Write-Host "Formatting $($pythonFiles.Count) Python file(s) under tests, tools, and utils."
    & $ruff.Source format $pythonFiles.FullName
    if ($LASTEXITCODE -ne 0) {
        throw 'ruff format failed.'
    }
}

# Format project CMake files, excluding generated and vendored content.
$cmakeFormat = Get-Command cmake-format -ErrorAction SilentlyContinue

if ($null -eq $cmakeFormat) {
    throw 'cmake-format was not found on PATH; install cmake-format to format CMake files.'
}

$cmakeFiles = Get-ChildItem -Path $repositoryRoot -Recurse -File `
    -Include 'CMakeLists.txt', '*.cmake' -ErrorAction SilentlyContinue |
    Where-Object {
        $_.FullName -notmatch '[\\/](build|build_codex|\.venv|node_modules|managed_components|\.git|\.pytest_cache)[\\/]'
    }

if ($cmakeFiles.Count -gt 0) {
    Write-Host "Formatting $($cmakeFiles.Count) CMake file(s)."
    foreach ($file in $cmakeFiles) {
        & $cmakeFormat.Source -i $file.FullName
        if ($LASTEXITCODE -ne 0) {
            throw "cmake-format failed for $($file.FullName)"
        }
    }
}
