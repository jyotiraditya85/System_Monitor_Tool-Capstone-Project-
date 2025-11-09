# build.ps1
param(
  [string]$WinLibsBin = "C:\Users\cross\mingw64\bin"
)
$env:Path = "$WinLibsBin;$env:Path"
Write-Host "g++ version:"; & g++ --version
Set-Location $PSScriptRoot\..\src
& g++ -std=c++20 -O2 -Wall -Wextra win_sysmon_day5.cpp -o ..\win-sysmon-day5 -lpsapi -ladvapi32
if ($LASTEXITCODE -ne 0) { Write-Error "Build failed"; exit 1 }
Write-Host "Built ..\win-sysmon-day5"
