@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64
cl /std:c++17 /EHsc /I backend backend\main.cpp /Fe:server.exe ws2_32.lib
