@echo off
powershell "Measure-Command{misc\\build.bat %1 | Out-Default} | findstr -i TotalMilliseconds"