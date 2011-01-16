cd ..
cd ..
@echo off
color 9f
:start
cls
echo ------------------------------------------------------
echo openMSX build menu for VC++
echo ------------------------------------------------------
echo.
echo [1] Update from Repository 
echo [2] Get 3rd Party Files
echo.
echo  Win32 Options
echo   [3] Compile Release
echo   [4] Compile 3rd-party files
echo   [5] Generate Installer and Packages
echo.
echo  x64 Options
echo   [6] Compile release (needs 64bit compiler)
echo   [7] Compile 3rd-party files (needs 64bit compiler)
echo   [8] Generate Installer and Packages
echo.
echo [9] Exit
echo.

choice /M "Make your choice : " /C 123456789 /T 600 /D 3

if errorlevel 9 goto exit   
if errorlevel 8 goto win64inst 
if errorlevel 7 goto win64pkg                                      
if errorlevel 6 goto win64 
if errorlevel 5 goto win32inst 
if errorlevel 4 goto win32pkg                                      
if errorlevel 3 goto win32                                           
if errorlevel 2 goto 3rd                                        
if errorlevel 1 goto svn 

:svn
cls
echo -- Updating SVN Repository 
svn up
pause
cls
goto start

:3rd
cls
echo -- Downloading and extracting 3rd-party files
python build\3rdparty_download.py windows
pause
cls
goto start

:win32
cls
echo --- Set Compiler ---
call "C:\Program Files\Microsoft SDKs\Windows\v7.1\Bin\SetEnv.cmd" /x86
echo --- Build openMSX ---
msbuild -p:Configuration=Release;Platform=Win32 build\msvc\openmsx.sln 
pause
goto start

:win32pkg
cls
echo --- Set Compiler ---
call "C:\Program Files\Microsoft SDKs\Windows\v7.1\Bin\SetEnv.cmd" /x86
echo --- Build 3rd party ---
msbuild -p:Configuration=Release;Platform=win32  build\3rdparty\3rdparty.sln 
rem /verbosity:diag 
pause
goto start

:win32inst
cls
echo --- Build Installer ---
build\package-windows\package.cmd win32 release ..\wxcatapult
pause
goto start

:win64
cls
echo --- Build openMSX Win64 ---
call "C:\Program Files\Microsoft SDKs\Windows\v7.1\Bin\SetEnv.cmd" /x64
msbuild -p:Configuration=Release;Platform=x64  build\msvc\openmsx.sln
pause
goto start

:win64pkg
cls
echo --- Build 3rd party Win64 ---
call "C:\Program Files\Microsoft SDKs\Windows\v7.1\Bin\SetEnv.cmd" /x64
msbuild -p:Configuration=Release;Platform=x64  build\3rdparty\3rdparty.sln 
rem /verbosity:diag 
pause
goto start

:win64inst
cls
echo --- Build Installer ---
build\package-windows\package.cmd x64 release ..\wxcatapult
pause
goto start

:exit
cls
color 0f
echo Goodbye

