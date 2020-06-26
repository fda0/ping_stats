@echo off
powershell "Measure-Command{./build.bat | Out-Default} | findstr -i TotalMilliseconds"