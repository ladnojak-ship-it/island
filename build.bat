@echo off
REM Запускать из "Developer Command Prompt for VS" (нужен Visual Studio с C++ и Windows SDK)
if not exist build mkdir build
cl /nologo /std:c++20 /EHsc /O2 /utf-8 /D_SILENCE_EXPERIMENTAL_COROUTINE_DEPRECATION_WARNINGS /DUNICODE /D_UNICODE /Fo:build\ /Fe:build\DynamicIsland.exe src\*.cpp /link /SUBSYSTEM:WINDOWS
if not %errorlevel%==0 goto fail
if exist fonts xcopy /E /I /Y fonts build\fonts >nul
echo.
echo Готово: build\DynamicIsland.exe
goto :eof
:fail
echo.
echo Ошибка сборки
