@rem
@rem **** This is a very temporary thing, to be replaced by a Python script as soon as we're ready for integration ****
@rem **** Run this from the root openMSX source directory with no arguments: ****
@rem **** build\package-windows\package.cmd ****
@rem

@setlocal

@rem
@rem **** Variables that need to come from the broader build environment ****
@rem **** OPENMSX_ARCH is { x86, x64 } ****
@rem **** OPENMSX_CONFIGURATION is { Release, Developer, Debug } ****
@rem
@set OPENMSX_ARCH=x64
@set OPENMSX_CONFIGURATION=Release
@set OPENMSX_VERSION=0.7.1
@set CATAPULT_BASEPATH=..\wxCatapult

@rem
@rem **** Variables derived from the above ****
@rem
@if "%OPENMSX_ARCH%"=="x86" (@set OPENMSX_PLATFORM=Win32) else (@set OPENMSX_PLATFORM=%OPENMSX_ARCH%)
@echo OPENMSX_PLATFORM is %OPENMSX_PLATFORM%

@set OPENMSX_PACKAGE_WINDOWS_PATH=build\package-windows
@echo OPENMSX_PACKAGE_WINDOWS_PATH is %OPENMSX_PACKAGE_WINDOWS_PATH%

@set OPENMSX_BUILD_FLAVOR=%OPENMSX_PLATFORM%-VC-%OPENMSX_CONFIGURATION%
@echo OPENMSX_BUILD_FLAVOR is %OPENMSX_BUILD_FLAVOR%

@set OPENMSX_BUILD_PATH=derived\%OPENMSX_BUILD_FLAVOR%
@echo OPENMSX_BUILD_PATH is %OPENMSX_BUILD_PATH%

@set OPENMSX_MAKEINSTALL_PATH=%OPENMSX_BUILD_PATH%\build\install
@echo OPENMSX_MAKEINSTALL_PATH is %OPENMSX_MAKEINSTALL_PATH%

@set OPENMSX_ICON_PATH=%OPENMSX_MAKEINSTALL_PATH%\bin\openmsx.exe
@echo OPENMSX_ICON_PATH is %OPENMSX_ICON_PATH%

@echo CATAPULT_BASEPATH is $CATAPULT_BASEPATH%
@set CATAPULT_BUILD_FLAVOR=%OPENMSX_PLATFORM%-VC-Unicode %OPENMSX_CONFIGURATION%
@echo CATAPULT_BUILD_FLAVOR is %CATAPULT_BUILD_FLAVOR%

@set CATAPULT_BUILD_PATH=%CATAPULT_BASEPATH%\derived\%CATAPULT_BUILD_FLAVOR%
@echo CATAPULT_BUILD_PATH is %CATAPULT_BUILD_PATH%

@set WIX_INTERMEDIATE_PATH=%OPENMSX_BUILD_PATH%\build\Wix
@echo WIX_INTERMEDIATE_PATH is %WIX_INTERMEDIATE_PATH%

@set WIX_OUTPUT_PATH=%OPENMSX_BUILD_PATH%\package-windows
@echo WIX_OUTPUT_PATH is %WIX_OUTPUT_PATH%

@set WIX_OUTPUT_FILE=%WIX_OUTPUT_PATH%\openmsx-%OPENMSX_VERSION%-VC-%OPENMSX_ARCH%.msi
@echo WIX_OUTPUT_FILE is %WIX_OUTPUT_FILE%

@rem
@rem **** Run install.py to generate openMSX install directories ****
@rem
@if exist %OPENMSX_MAKEINSTALL_PATH% (rd /s /q %OPENMSX_MAKEINSTALL_PATH%)
@set INSTALL_PREFIX=%OPENMSX_MAKEINSTALL_PATH%\
@set INSTALL_BINARY_DIR=bin
@set INSTALL_SHARE_DIR=share
@set INSTALL_DOC_DIR=doc
@set BINARY_FULL=%OPENMSX_BUILD_PATH%\install\openmsx.exe
@set OPENMSX_TARGET_OS=mingw32
@set INSTALL_VERBOSE=true
@set INSTALL_CONTRIB=true
python build\install.py %INSTALL_PREFIX% %INSTALL_BINARY_DIR% %INSTALL_SHARE_DIR% %INSTALL_DOC_DIR% %BINARY_FULL% %OPENMSX_TARGET_OS% %INSTALL_VERBOSE% %INSTALL_CONTRIB%

@rem
@rem **** Harvest openMSX install directories into WiX input files ****
@rem

@if exist %WIX_INTERMEDIATE_PATH% (rd /s /q %WIX_INTERMEDIATE_PATH%)
@md %WIX_INTERMEDIATE_PATH%

python "%OPENMSX_PACKAGE_WINDOWS_PATH%\harvest.py" -c openMSXExe -r OPENMSXINSTALLDIR -s "%OPENMSX_MAKEINSTALL_PATH%\bin\openmsx.exe" > "%WIX_INTERMEDIATE_PATH%\openmsxexe.wxs"
python "%OPENMSX_PACKAGE_WINDOWS_PATH%\harvest.py" -c openMSXDoc -r OPENMSXINSTALLDIR -v doc -s "%OPENMSX_MAKEINSTALL_PATH%\doc" > "%WIX_INTERMEDIATE_PATH%\openmsxdoc.wxs"
python "%OPENMSX_PACKAGE_WINDOWS_PATH%\harvest.py" -c openMSXShare -r OPENMSXINSTALLDIR -v share -s "%OPENMSX_MAKEINSTALL_PATH%\share" > "%WIX_INTERMEDIATE_PATH%\openmsxshare.wxs"
python "%OPENMSX_PACKAGE_WINDOWS_PATH%\harvest.py" -c openMSXIcon -r OPENMSXINSTALLDIR -v share\icons -s "src\resource\openmsx.ico" > "%WIX_INTERMEDIATE_PATH%\openmsxicon.wxs"

python "%OPENMSX_PACKAGE_WINDOWS_PATH%\harvest.py" -c ZMBVCodecFiles -r OPENMSXINSTALLDIR -v codec -s "Contrib\codec\Win32" > "%WIX_INTERMEDIATE_PATH%\zmbvcodec.wxs"

python "%OPENMSX_PACKAGE_WINDOWS_PATH%\harvest.py" -c CatapultBin -r OPENMSXINSTALLDIR -v Catapult\bin -s "%CATAPULT_BUILD_PATH%\install\catapult.exe" > "%WIX_INTERMEDIATE_PATH%\catapultbin.wxs"
python "%OPENMSX_PACKAGE_WINDOWS_PATH%\harvest.py" -c CatapultDoc -r OPENMSXINSTALLDIR -v Catapult\doc -s "%CATAPULT_BASEPATH%\doc" -x release-process.txt > "%WIX_INTERMEDIATE_PATH%\catapultdoc.wxs"
python "%OPENMSX_PACKAGE_WINDOWS_PATH%\harvest.py" -c CatapultBitmaps -r OPENMSXINSTALLDIR -v Catapult\resources\bitmaps -s "%CATAPULT_BASEPATH%\resources\bitmaps" > "%WIX_INTERMEDIATE_PATH%\catapultbitmaps.wxs"
python "%OPENMSX_PACKAGE_WINDOWS_PATH%\harvest.py" -c CatapultDialogs -r OPENMSXINSTALLDIR -v Catapult\resources\dialogs -s "%CATAPULT_BUILD_PATH%\install\dialogs" > "%WIX_INTERMEDIATE_PATH%\catapultdialogs.wxs"
python "%OPENMSX_PACKAGE_WINDOWS_PATH%\harvest.py" -c CatapultIcons -r OPENMSXINSTALLDIR -v Catapult\resources\icons -s "%CATAPULT_BASEPATH%\src\catapult.xpm" > "%WIX_INTERMEDIATE_PATH%\catapulticons.wxs"
python "%OPENMSX_PACKAGE_WINDOWS_PATH%\harvest.py" -c CatapultAuthors -r OPENMSXINSTALLDIR -v Catapult\doc -s "%CATAPULT_BASEPATH%\AUTHORS" > "%WIX_INTERMEDIATE_PATH%\catapultauthors.wxs"
python "%OPENMSX_PACKAGE_WINDOWS_PATH%\harvest.py" -c CatapultGpl -r OPENMSXINSTALLDIR -v Catapult\doc -s "%CATAPULT_BASEPATH%\GPL" > "%WIX_INTERMEDIATE_PATH%\catapultgpl.wxs"
python "%OPENMSX_PACKAGE_WINDOWS_PATH%\harvest.py" -c CatapultReadme -r OPENMSXINSTALLDIR -v Catapult\doc -s "%CATAPULT_BASEPATH%\README" > "%WIX_INTERMEDIATE_PATH%\catapultreadme.wxs"

@rem
@rem **** Compile the MSI ****
@rem

@rem Variables needed inside the WiX scripts:
@rem OPENMSX_VERSION to tell it the product version
@rem OPENMSX_ICON_PATH to locate the MSI's control panel icon
@rem OPENMSX_PACKAGE_WINDOWS_PATH to locate the bmps used in the UI

candle.exe -arch %OPENMSX_ARCH% -o %WIX_INTERMEDIATE_PATH%\ -ext WixUtilExtension %OPENMSX_PACKAGE_WINDOWS_PATH%\openmsx.wxs %WIX_INTERMEDIATE_PATH%\openmsxexe.wxs %WIX_INTERMEDIATE_PATH%\openmsxdoc.wxs %WIX_INTERMEDIATE_PATH%\openmsxshare.wxs %WIX_INTERMEDIATE_PATH%\openmsxicon.wxs %WIX_INTERMEDIATE_PATH%\zmbvcodec.wxs %WIX_INTERMEDIATE_PATH%\catapultbin.wxs %WIX_INTERMEDIATE_PATH%\catapultdoc.wxs %WIX_INTERMEDIATE_PATH%\catapultbitmaps.wxs %WIX_INTERMEDIATE_PATH%\catapultdialogs.wxs %WIX_INTERMEDIATE_PATH%\catapulticons.wxs %WIX_INTERMEDIATE_PATH%\catapultauthors.wxs %WIX_INTERMEDIATE_PATH%\catapultgpl.wxs %WIX_INTERMEDIATE_PATH%\catapultreadme.wxs
light.exe -o %WIX_OUTPUT_FILE% -ext WixUtilExtension -ext WixUIExtension -loc %OPENMSX_PACKAGE_WINDOWS_PATH%\openmsx1033.wxl %WIX_INTERMEDIATE_PATH%\openmsx.wixobj %WIX_INTERMEDIATE_PATH%\openmsxexe.wixobj %WIX_INTERMEDIATE_PATH%\openmsxdoc.wixobj %WIX_INTERMEDIATE_PATH%\openmsxshare.wixobj %WIX_INTERMEDIATE_PATH%\openmsxicon.wixobj %WIX_INTERMEDIATE_PATH%\zmbvcodec.wixobj %WIX_INTERMEDIATE_PATH%\catapultbin.wixobj %WIX_INTERMEDIATE_PATH%\catapultdoc.wixobj %WIX_INTERMEDIATE_PATH%\catapultbitmaps.wixobj %WIX_INTERMEDIATE_PATH%\catapultdialogs.wixobj %WIX_INTERMEDIATE_PATH%\catapulticons.wixobj %WIX_INTERMEDIATE_PATH%\catapultauthors.wixobj %WIX_INTERMEDIATE_PATH%\catapultgpl.wixobj %WIX_INTERMEDIATE_PATH%\catapultreadme.wixobj

@endlocal