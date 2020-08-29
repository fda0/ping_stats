@echo off

set BuildPath=build
IF NOT EXIST %BuildPath% mkdir %BuildPath%
pushd %BuildPath%

rem Delete pdb file spam
del *.pdb

set WinSourcePath=..\code\ping_stats.cpp


rem ~~~~~~~~~~RELEASE MODE~~~~~~~~~~
set Release=/O2 /fp:fast

rem ~~~~~~~~~~DEBUG MODE~~~~~~~~~~
set Debug=/Od

rem ~~~~~~~~~~SELECT MODE~~~~~~~~~~
set Mode=%Debug%
if "%1" == "release" (set Mode=%release% 
echo ===_===_=== OPTIMIZED MODE ===_===_===) else (echo === debug mode ===)



rem ~~~~~~~~~~WARININGS~~~~~~~~~~
set IgnoredWarnings=-wd4100 -wd4201 -wd4189 -wd4505 -wd4101 -wd4996


rem ~~~~~~~~~~FLAGS~~~~~~~~~~
set CommonCompilerFlags=-MTd -nologo /Gm- -GR- -EHa- -Oi -WX -W4 %IgnoredWarnings% -DBUILD_INTERNAL=1 -DBUILD_SLOW=1 -DBUILD_WIN32=1 -FC -Zi

set WindowsLayerFlags=-FmWin32_CppGame.map /link -incremental:no -opt:ref shell32.lib


rem ~~~~~~~~~~BUILD x64~~~~~~~~~~

cl %CommonCompilerFlags% %Mode% %WinSourcePath% %WindowsLayerFlags%


popd