@echo off
setlocal

if "%UE_ROOT%"=="" set "UE_ROOT=C:\Program Files\Epic Games\UE_5.7"
set "PROJECT_DIR=%~dp0"
set "UPROJECT=%PROJECT_DIR%RinRinJsLab.uproject"
set "UBT_DLL=%UE_ROOT%\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll"

dotnet "%UBT_DLL%" ^
  -projectfiles -project="%UPROJECT%" -game -rocket -progress
