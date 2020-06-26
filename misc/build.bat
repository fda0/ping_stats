@echo off
pushd ../build

cl -Zi -W4 -nologo ..\code\ping_stats.cpp
popd