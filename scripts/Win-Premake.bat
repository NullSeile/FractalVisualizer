@echo off

pushd ..\
call vendor\bin\premake\premake5.exe vs2022 --scripts=vendor\bin\premake\scripts
popd
PAUSE