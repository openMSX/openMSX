import os, sys
import zipfile
import packagewindows
import harvest

def WriteFragment(wxsFile, sourcePath, componentGroup, directoryRef, virtualDir, excludedFile, win64):
	print 'Generating ' + wxsFile
	file = open(wxsFile, 'w')
	for line in harvest.GenerateWixFragment(sourcePath, componentGroup, directoryRef, virtualDir, excludedFile, win64):
		file.write(line)
		file.write('\n')
	file.close()

def PackageMsi(info):

	print 'Generating install files...'
	packagewindows.GenerateInstallFiles(info);

	wixIntermediatePath = os.path.join(info.buildPath, 'build\\WiX')
	packagewindows.EmptyOrCreateDirectory(wixIntermediatePath)
	
	if not os.path.exists(info.packagePath):
		os.mkdir(info.packagePath)

	print 'Generating fragments...'

	# openMSX files
	openMSXExeFile = os.path.join(wixIntermediatePath, 'openmsxexe.wxs')
	openMSXExeObjFile = os.path.join(wixIntermediatePath, 'openmsxexe.wixobj')
	sourcePath = os.path.join(info.makeInstallPath, 'bin\\openmsx.exe')
	WriteFragment(openMSXExeFile, sourcePath, 'openMSXExe', 'OPENMSXINSTALLDIR', None, None, info.win64)
	
	openMSXDocFile = os.path.join(wixIntermediatePath, 'openmsxdoc.wxs')
	openMSXDocObjFile = os.path.join(wixIntermediatePath, 'openmsxdoc.wixobj')
	sourcePath = os.path.join(info.makeInstallPath, 'doc')
	WriteFragment(openMSXDocFile, sourcePath, 'openMSXDoc', 'OPENMSXINSTALLDIR', 'doc', None, info.win64)
	
	openMSXShareFile = os.path.join(wixIntermediatePath, 'openmsxshare.wxs')
	openMSXShareObjFile = os.path.join(wixIntermediatePath, 'openmsxshare.wixobj')
	sourcePath = os.path.join(info.makeInstallPath, 'share')
	WriteFragment(openMSXShareFile, sourcePath, 'openMSXShare', 'OPENMSXINSTALLDIR', 'share', None, info.win64)
	
	openMSXIconFile = os.path.join(wixIntermediatePath, 'openmsxicon.wxs')
	openMSXIconObjFile = os.path.join(wixIntermediatePath, 'openmsxicon.wixobj')
	sourcePath = os.path.join(info.sourcePath, 'resource\\openmsx.ico')
	WriteFragment(openMSXIconFile, sourcePath, 'openMSXIcon', 'OPENMSXINSTALLDIR', 'share\\icons', None, info.win64)
	
	# ZMBV files
	ZMBVCodecFile = os.path.join(wixIntermediatePath, 'zmbvcodec.wxs')
	ZMBVCodecObjFile = os.path.join(wixIntermediatePath, 'zmbvcodec.wixobj')
	sourcePath = os.path.join(info.codecPath, 'zmbv.dll')
	WriteFragment(ZMBVCodecFile, sourcePath, 'ZMBVCodec', 'SystemFolder', None, None, False)
	
	ZMBVFilesFile = os.path.join(wixIntermediatePath, 'zmbvfiles.wxs')
	ZMBVFilesObjFile = os.path.join(wixIntermediatePath, 'zmbvfiles.wixobj')
	sourcePath = info.codecPath
	WriteFragment(ZMBVFilesFile, sourcePath, 'ZMBVFiles', 'OPENMSXINSTALLDIR', 'codec', 'zmbv.dll', info.win64)

	# Catapult files
	CatapultBinFile = os.path.join(wixIntermediatePath, 'catapultbin.wxs')
	CatapultBinObjFile = os.path.join(wixIntermediatePath, 'catapultbin.wixobj')
	sourcePath = os.path.join(info.catapultBuildPath, 'install\\catapult.exe')
	WriteFragment(CatapultBinFile, sourcePath, 'CatapultBin', 'OPENMSXINSTALLDIR', 'Catapult\\bin', None, info.win64)
	
	CatapultDocFile = os.path.join(wixIntermediatePath, 'catapultdoc.wxs')
	CatapultDocObjFile = os.path.join(wixIntermediatePath, 'catapultdoc.wixobj')
	sourcePath = os.path.join(info.catapultPath, 'doc')
	WriteFragment(CatapultDocFile, sourcePath, 'CatapultDoc', 'OPENMSXINSTALLDIR', 'Catapult\\doc', 'release-process.txt', info.win64)
	
	CatapultBitmapsFile = os.path.join(wixIntermediatePath, 'catapultbitmaps.wxs')
	CatapultBitmapsObjFile = os.path.join(wixIntermediatePath, 'catapultbitmaps.wixobj')
	sourcePath = os.path.join(info.catapultPath, 'resources\\bitmaps')
	WriteFragment(CatapultBitmapsFile, sourcePath, 'CatapultBitmaps', 'OPENMSXINSTALLDIR', 'Catapult\\resources\\bitmaps', 'release-process.txt', info.win64)

	CatapultDialogsFile = os.path.join(wixIntermediatePath, 'catapultdialogs.wxs')
	CatapultDialogsObjFile = os.path.join(wixIntermediatePath, 'catapultdialogs.wixobj')
	sourcePath = os.path.join(info.catapultBuildPath, 'install\\dialogs')
	WriteFragment(CatapultDialogsFile, sourcePath, 'CatapultDialogs', 'OPENMSXINSTALLDIR', 'Catapult\\resources\\dialogs', None, info.win64)
	
	CatapultIconsFile = os.path.join(wixIntermediatePath, 'catapulticons.wxs')
	CatapultIconsObjFile = os.path.join(wixIntermediatePath, 'catapulticons.wixobj')
	sourcePath = os.path.join(info.catapultBuildPath, 'src\\catapult.xpm')
	WriteFragment(CatapultIconsFile, sourcePath, 'CatapultIcons', 'OPENMSXINSTALLDIR', 'Catapult\\resources\\icons', None, info.win64)

	CatapultReadmeFile = os.path.join(wixIntermediatePath, 'catapultreadme.wxs')
	CatapultReadmeObjFile = os.path.join(wixIntermediatePath, 'catapultreadme.wixobj')
	sourcePath = os.path.join(info.catapultPath, 'README')
	WriteFragment(CatapultReadmeFile, sourcePath, 'CatapultReadme', 'OPENMSXINSTALLDIR', 'Catapult\\doc', None, info.win64)
	
	# Variables needed inside the WiX scripts:
	# OPENMSX_VERSION to tell it the product version
	# OPENMSX_ICON_PATH to locate the MSI's control panel icon
	# OPENMSX_PACKAGE_WINDOWS_PATH to locate the bmps used in the UI

	os.environ['OPENMSX_VERSION'] = info.version
	os.environ['OPENMSX_ICON_PATH'] = info.openmsxExePath
	os.environ['OPENMSX_PACKAGE_WINDOWS_PATH'] = info.packageWindowsPath
	
	openMSXFile = os.path.join(info.packageWindowsPath, 'openmsx.wxs')
	openMSXObjFile = os.path.join(wixIntermediatePath, 'openmsx.wixobj')
	
	candleCmd = 'candle.exe'
	candleCmd += ' -arch ' + info.cpu
	candleCmd += ' -o ' + '\"' + wixIntermediatePath + '\\\\\"'
	candleCmd += ' -ext WixUtilExtension'
	candleCmd += ' \"' + openMSXFile + '\"'
	candleCmd += ' \"' + openMSXExeFile + '\"'
	candleCmd += ' \"' + openMSXDocFile + '\"'
	candleCmd += ' \"' + openMSXShareFile + '\"'
	candleCmd += ' \"' + openMSXIconFile + '\"'
	candleCmd += ' \"' + ZMBVCodecFile + '\"'
	candleCmd += ' \"' + ZMBVFilesFile + '\"'
	candleCmd += ' \"' + CatapultBinFile + '\"'
	candleCmd += ' \"' + CatapultDocFile + '\"'
	candleCmd += ' \"' + CatapultBitmapsFile + '\"'
	candleCmd += ' \"' + CatapultDialogsFile + '\"'
	candleCmd += ' \"' + CatapultIconsFile + '\"'
	candleCmd += ' \"' + CatapultReadmeFile + '\"'

	# Run Candle
	print candleCmd
	os.system(candleCmd)
	
	msiFileName = info.packageFileName + '-bin.msi'
	msiFilePath = os.path.join(info.packagePath, msiFileName)
	if os.path.exists(msiFilePath):
		os.unlink(msiFilePath)
		
	print 'Generating ' + msiFilePath

	lightCmd = 'light.exe'
	lightCmd += ' -o \"' + msiFilePath + '\"'
	lightCmd += ' -sw1076'
	lightCmd += ' -ext WixUtilExtension'
	lightCmd += ' -ext WixUIExtension'
	lightCmd += ' -loc \"' + os.path.join(info.packageWindowsPath, 'openmsx1033.wxl') + '\"'
	lightCmd += ' \"' + openMSXObjFile + '\"'
	lightCmd += ' \"' + openMSXExeObjFile + '\"'
	lightCmd += ' \"' + openMSXDocObjFile + '\"'
	lightCmd += ' \"' + openMSXShareObjFile + '\"'
	lightCmd += ' \"' + openMSXIconObjFile + '\"'
	lightCmd += ' \"' + ZMBVCodecObjFile + '\"'
	lightCmd += ' \"' + ZMBVFilesObjFile + '\"'
	lightCmd += ' \"' + CatapultBinObjFile + '\"'
	lightCmd += ' \"' + CatapultDocObjFile + '\"'
	lightCmd += ' \"' + CatapultBitmapsObjFile + '\"'
	lightCmd += ' \"' + CatapultDialogsObjFile + '\"'
	lightCmd += ' \"' + CatapultIconsObjFile + '\"'
	lightCmd += ' \"' + CatapultReadmeObjFile + '\"'
	
	# Run Light
	print lightCmd
	os.system(lightCmd)
	
	# Zip up the MSI
	zipFileName = info.packageFileName + '-bin-msi.zip'
	zipFilePath = os.path.join(info.packagePath, zipFileName)
	
	print 'Generating ' + zipFilePath
	zip = zipfile.ZipFile(zipFilePath, 'w')
	zip.write(msiFilePath, msiFileName, zipfile.ZIP_DEFLATED)
	zip.close()

if __name__ == '__main__':
	if len(sys.argv) != 4:
		print >> sys.stderr, 'Usage: python packagemsi.py platform configuration catapultPath'
		sys.exit(2)
	else:
		info = packagewindows.PackageInfo(sys.argv[1], sys.argv[2], sys.argv[3])
		PackageMsi(info)
