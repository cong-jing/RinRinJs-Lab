@echo off
setlocal

set UE_ROOT=C:\Program Files\Epic Games\UE_5.7
set UBT="%UE_ROOT%\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe"
set PROJECT_FILE="%~dp0RinRinGame.uproject"
set TARGET="RinRinGame Win64 Development"
rem set TARGET="RinRinGame Win64 Development"
rem You can change "Development" to "DebugGame" or "Shipping" as needed

%UBT% -Mode=Build -Project=%PROJECT_FILE% -Target=%TARGET% -NoHotReload

endlocal
