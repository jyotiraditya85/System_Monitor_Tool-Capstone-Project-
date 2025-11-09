# run.ps1
Set-Location $PSScriptRoot\..
.\win-sysmon-day5 --delay=2 --rows=200 --sort=cpu
