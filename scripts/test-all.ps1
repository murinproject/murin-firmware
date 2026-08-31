<#
.SYNOPSIS
Runs the pytest and GoogleTest suites.

.DESCRIPTION
Forwards all arguments to scripts/test-all.py.
#>

$ErrorActionPreference = 'Stop'
python "$PSScriptRoot\test-all.py" @args
exit $LASTEXITCODE
