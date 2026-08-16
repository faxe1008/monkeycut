# Import the MSVC x64 (vcvars64) environment into the current PowerShell
# session so subsequent cl.exe / link.exe invocations (e.g. `cmake --build`)
# can find the MSVC + UCRT headers and libs. GitHub Actions runners do not
# start in a VS developer shell.
param([Parameter(Mandatory)][string]$Vcvars)

if (-not (Test-Path $Vcvars)) { throw "vcvars64.bat not found: $Vcvars" }

$lines = & cmd /c "`"$Vcvars`" && set"
foreach ($l in $lines) {
    if ($l -match '^(.*?)=(.*)$') {
        [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2])
    }
}

