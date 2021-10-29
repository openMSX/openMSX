@echo off

rem
rem **** Run this from the top of the openMSX source tree: ****
rem
rem Usage: package.cmd OPENMSX_PLATFORM OPENMSX_CONFIGURATION CATAPULT_BASEPATH
rem
rem **** OPENMSX_PLATFORM is { Win32, x64 } ****
rem **** OPENMSX_CONFIGURATION is { Release, Developer, Debug } ****
rem **** CATAPULT_BASEPATH is an absolute or relative path; e.g. ..\wxCatapult ****

if "%3" == "" goto usage
if "%4" NEQ "" goto usage

setlocal

set OPENMSX_PLATFORM=%1
echo OPENMSX_PLATFORM is %OPENMSX_PLATFORM%

set OPENMSX_CONFIGURATION=%2
echo OPENMSX_CONFIGURATION is %OPENMSX_CONFIGURATION%

set CATAPULT_BASEPATH=%3
echo CATAPULT_BASEPATH is %CATAPULT_BASEPATH%

set OPENMSX_PACKAGE_WINDOWS_PATH=.\build\package-windows
set PYTHONPATH=%PYTHONPATH%;.\build

python3 %OPENMSX_PACKAGE_WINDOWS_PATH%\packagezip.py %OPENMSX_PLATFORM% %OPENMSX_CONFIGURATION% %CATAPULT_BASEPATH%
python3 %OPENMSX_PACKAGE_WINDOWS_PATH%\packagemsi.py %OPENMSX_PLATFORM% %OPENMSX_CONFIGURATION% %CATAPULT_BASEPATH%

endlocal
goto end

:usage
echo Usage: package.cmd platform configuration catapultPath
:end
