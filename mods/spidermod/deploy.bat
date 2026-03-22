@echo off
:: SpiderMod deploy — copies built ASI to game root directory
:: Usage: double-click or run from cmd

set BUILD_DIR=%~dp0build2\Release
set GAME_ROOT=h:\Games\.Archive\SPIDEWICK

if not exist "%BUILD_DIR%\spidermod.asi" (
    echo ERROR: spidermod.asi not found in %BUILD_DIR%
    echo Build first: cd build2 ^&^& cmake --build . --config Release
    pause
    exit /b 1
)

copy /Y "%BUILD_DIR%\spidermod.asi" "%GAME_ROOT%\"
echo Deployed spidermod.asi

if exist "%BUILD_DIR%\dinput8.dll" (
    copy /Y "%BUILD_DIR%\dinput8.dll" "%GAME_ROOT%\"
    echo Deployed dinput8.dll
)

echo Done.
