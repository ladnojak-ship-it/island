@echo off
REM Запускать из "Developer Command Prompt for VS" (нужен Visual Studio с C++ и Windows SDK)
if not exist build mkdir build
cl /nologo /std:c++17 /EHsc /O2 /utf-8 /DUNICODE /D_UNICODE /Fo:build\ /Fe:build\DynamicIsland.exe src\*.cpp /link /SUBSYSTEM:WINDOWS
if %errorlevel%==0 (echo. & echo Готово: build\DynamicIsland.exe) else (echo. & echo Ошибка сборки)
