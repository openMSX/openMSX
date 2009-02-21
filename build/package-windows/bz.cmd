@rem This is a very temporary thing, to be replaced by a Python script asap
@rem
@rem python build\install-recursive.py share . c:\ftp\dist\share
@rem python build\install-recursive.py Contrib\cbios . c:\ftp\dist\share\machines

@setlocal

@rem arch should be set to either x86 or x64
@set arch=x64
@set openMSXVersion=0.7.1
@set openMSXSourcePath=C:\Scratch\Build\openmsx\8625
@set CatapultSourcePath=C:\Scratch\Build\catapult

@set IntDir=build\%arch%
@rd /s /q %IntDir%
@md %IntDir%

@set platform=%arch%
@if "%arch%"=="x86" set platform=Win32

@set flavor=%platform%-VC-Release
@set OutDir=..\..\derived\%flavor%\install

@rem Harvest files

python harvest.py -c openMSXExe -r OPENMSXINSTALLDIR -s "%openMSXSourcePath%\derived\%flavor%\install\exe\openmsx.exe" > %IntDir%\openmsxexe.wxs
python harvest.py -c openMSXShare -r OPENMSXINSTALLDIR -v share -s "%openMSXSourcePath%\share" > %IntDir%\openmsxshare.wxs
python harvest.py -c openMSXDoc -r OPENMSXINSTALLDIR -v doc -s "%openMSXSourcePath%\doc" > %IntDir%\openmsxdoc.wxs

python harvest.py -c CatapultBin -r OPENMSXINSTALLDIR -v Catapult\bin -s "%CatapultSourcePath%\derived\%platform%-VC-Unicode Release\bin\catapult.exe" > %IntDir%\catapultbin.wxs
python harvest.py -c CatapultResources -r OPENMSXINSTALLDIR -v Catapult\resources -s "%CatapultSourcePath%\resources" > %IntDir%\catapultresources.wxs
python harvest.py -c CatapultDoc -r OPENMSXINSTALLDIR -v Catapult\doc -s "%CatapultSourcePath%\doc" > %IntDir%\catapultdoc.wxs

python harvest.py -c ZMBVCodecFiles -r OPENMSXINSTALLDIR -v codec -s "..\..\Contrib\codec\Win32" > %IntDir%\zmbvcodec.wxs

@rem Compile the MSI

candle.exe -arch %arch% -o %IntDir%\ -ext WixUtilExtension openmsx.wxs %IntDir%\openmsxexe.wxs %IntDir%\openmsxdoc.wxs %IntDir%\openmsxshare.wxs %IntDir%\catapultbin.wxs %IntDir%\catapultresources.wxs %IntDir%\catapultdoc.wxs %IntDir%\zmbvcodec.wxs
light.exe -o %OutDir%\openmsx-%openMSXVersion%-VC-%platform%.msi -ext WixUtilExtension -ext WixUIExtension -loc openmsx1033.wxl %IntDir%\openmsx.wixobj %IntDir%\openmsxexe.wixobj %IntDir%\openmsxdoc.wixobj %IntDir%\openmsxshare.wixobj %IntDir%\catapultbin.wixobj %IntDir%\catapultresources.wixobj %IntDir%\catapultdoc.wixobj %IntDir%\zmbvcodec.wixobj

@endlocal