cd c:\compiler\openMSX\

del c:\compiler\openmsx\upload\  /Q

copy C:\compiler\openMSX\derived\x64-VC-Release\package-windows\*.zip c:\compiler\openmsx\upload\ /y
copy C:\compiler\openMSX\derived\x64-VC-Release\package-windows\*.zip y:\buildlog\zips\ /y
del c:\compiler\openmsx\upload\*pdb*.zip  /Q
call "winscp" /privatekey=fixato.ppk /script=upload_script
del "C:\compiler\openMSX\derived\x64-VC-Release\install\openmsx.exe" /Q
del C:\compiler\openMSX\derived\x64-VC-Release\package-windows\*.* /Q
