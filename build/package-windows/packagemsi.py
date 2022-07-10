from harvest import generateWixFragment
from packagewindows import (
	PackageInfo, emptyOrCreateDirectory, generateInstallFiles
	)

from io import open
from os import environ, mkdir, system, unlink
from os.path import exists, join as joinpath
from zipfile import ZIP_DEFLATED, ZipFile
import sys

def _writeFragment(
	wxsFile, sourcePath, componentGroup, directoryRef,
	virtualDir, excludedFile, win64
	):
	print('Generating ' + wxsFile)
	with open(wxsFile, 'w', encoding='utf-8') as out:
		out.writelines(
			line + '\n'
			for line in generateWixFragment(
				sourcePath, componentGroup, directoryRef, virtualDir,
				excludedFile, win64
				)
			)

def packageMSI(info):
	assert info.packageCatapult, "MSI packaging is only supported when also packaging Catapult!"
	print('Generating install files...')
	generateInstallFiles(info)

	wixIntermediatePath = joinpath(info.buildPath, 'build\\WiX')
	emptyOrCreateDirectory(wixIntermediatePath)

	if not exists(info.packagePath):
		mkdir(info.packagePath)

	print('Generating fragments...')

	# openMSX files
	openMSXExeFile = joinpath(wixIntermediatePath, 'openmsxexe.wxs')
	openMSXExeObjFile = joinpath(wixIntermediatePath, 'openmsxexe.wixobj')
	sourcePath = joinpath(info.makeInstallPath, 'bin\\openmsx.exe')
	_writeFragment(
		openMSXExeFile, sourcePath, 'openMSXExe', 'OPENMSXINSTALLDIR',
		None, None, info.win64
		)

	openMSXDocFile = joinpath(wixIntermediatePath, 'openmsxdoc.wxs')
	openMSXDocObjFile = joinpath(wixIntermediatePath, 'openmsxdoc.wixobj')
	sourcePath = joinpath(info.makeInstallPath, 'doc')
	_writeFragment(
		openMSXDocFile, sourcePath, 'openMSXDoc', 'OPENMSXINSTALLDIR',
		'doc', None, info.win64
		)

	openMSXShareFile = joinpath(wixIntermediatePath, 'openmsxshare.wxs')
	openMSXShareObjFile = joinpath(wixIntermediatePath, 'openmsxshare.wixobj')
	sourcePath = joinpath(info.makeInstallPath, 'share')
	_writeFragment(
		openMSXShareFile, sourcePath, 'openMSXShare', 'OPENMSXINSTALLDIR',
		'share', None, info.win64
		)

	openMSXIconFile = joinpath(wixIntermediatePath, 'openmsxicon.wxs')
	openMSXIconObjFile = joinpath(wixIntermediatePath, 'openmsxicon.wixobj')
	sourcePath = joinpath(info.sourcePath, 'resource\\openmsx.ico')
	_writeFragment(
		openMSXIconFile, sourcePath, 'openMSXIcon', 'OPENMSXINSTALLDIR',
		'share\\icons', None, info.win64
		)

	# ZMBV files
	ZMBVCodecFile = joinpath(wixIntermediatePath, 'zmbvcodec.wxs')
	ZMBVCodecObjFile = joinpath(wixIntermediatePath, 'zmbvcodec.wixobj')
	sourcePath = joinpath(info.codecPath, 'zmbv.dll')
	_writeFragment(
		ZMBVCodecFile, sourcePath, 'ZMBVCodec', 'SystemFolder',
		None, None, False
		)

	ZMBVFilesFile = joinpath(wixIntermediatePath, 'zmbvfiles.wxs')
	ZMBVFilesObjFile = joinpath(wixIntermediatePath, 'zmbvfiles.wixobj')
	sourcePath = info.codecPath
	_writeFragment(
		ZMBVFilesFile, sourcePath, 'ZMBVFiles', 'OPENMSXINSTALLDIR',
		'codec', 'zmbv.dll', info.win64
		)

	# Catapult files
	catapultBinFile = joinpath(wixIntermediatePath, 'catapultbin.wxs')
	catapultBinObjFile = joinpath(wixIntermediatePath, 'catapultbin.wixobj')
	sourcePath = joinpath(info.catapultBuildPath, 'install\\catapult.exe')
	_writeFragment(
		catapultBinFile, sourcePath, 'CatapultBin', 'OPENMSXINSTALLDIR',
		'Catapult\\bin', None, info.win64
		)

	catapultDocFile = joinpath(wixIntermediatePath, 'catapultdoc.wxs')
	catapultDocObjFile = joinpath(wixIntermediatePath, 'catapultdoc.wixobj')
	sourcePath = joinpath(info.catapultPath, 'doc')
	_writeFragment(
		catapultDocFile, sourcePath, 'CatapultDoc', 'OPENMSXINSTALLDIR',
		'Catapult\\doc', 'release-process.txt', info.win64
		)

	catapultBitmapsFile = joinpath(wixIntermediatePath, 'catapultbitmaps.wxs')
	catapultBitmapsObjFile = joinpath(
		wixIntermediatePath, 'catapultbitmaps.wixobj'
		)
	sourcePath = joinpath(info.catapultPath, 'resources\\bitmaps')
	_writeFragment(
		catapultBitmapsFile, sourcePath, 'CatapultBitmaps', 'OPENMSXINSTALLDIR',
		'Catapult\\resources\\bitmaps', 'release-process.txt', info.win64
		)

	catapultDialogsFile = joinpath(wixIntermediatePath, 'catapultdialogs.wxs')
	catapultDialogsObjFile = joinpath(
		wixIntermediatePath, 'catapultdialogs.wixobj'
		)
	sourcePath = joinpath(info.catapultBuildPath, 'install\\dialogs')
	_writeFragment(
		catapultDialogsFile, sourcePath, 'CatapultDialogs', 'OPENMSXINSTALLDIR',
		'Catapult\\resources\\dialogs', None, info.win64
		)

	catapultIconsFile = joinpath(wixIntermediatePath, 'catapulticons.wxs')
	catapultIconsObjFile = joinpath(wixIntermediatePath, 'catapulticons.wixobj')
	sourcePath = joinpath(info.catapultBuildPath, 'src\\catapult.xpm')
	_writeFragment(
		catapultIconsFile, sourcePath, 'CatapultIcons', 'OPENMSXINSTALLDIR',
		'Catapult\\resources\\icons', None, info.win64
		)

	catapultReadmeFile = joinpath(wixIntermediatePath, 'catapultreadme.wxs')
	catapultReadmeObjFile = joinpath(
		wixIntermediatePath, 'catapultreadme.wixobj'
		)
	sourcePath = joinpath(info.catapultPath, 'README')
	_writeFragment(
		catapultReadmeFile, sourcePath, 'CatapultReadme', 'OPENMSXINSTALLDIR',
		'Catapult\\doc', None, info.win64
		)

	# Variables needed inside the WiX scripts:
	# OPENMSX_VERSION to tell it the product version
	# OPENMSX_ICON_PATH to locate the MSI's control panel icon
	# OPENMSX_PACKAGE_WINDOWS_PATH to locate the bmps used in the UI
	environ.update(
		OPENMSX_VERSION = info.version,
		OPENMSX_ICON_PATH = info.openmsxExePath,
		OPENMSX_PACKAGE_WINDOWS_PATH = info.packageWindowsPath,
		)

	openMSXFile = joinpath(info.packageWindowsPath, 'openmsx.wxs')
	openMSXObjFile = joinpath(wixIntermediatePath, 'openmsx.wixobj')

	candleCmd = ' '.join((
		'candle.exe',
		'-arch %s' % info.cpu,
		'-o "%s\\\\"' % wixIntermediatePath,
		'-ext WixUtilExtension',
		'"%s"' % openMSXFile,
		'"%s"' % openMSXExeFile,
		'"%s"' % openMSXDocFile,
		'"%s"' % openMSXShareFile,
		'"%s"' % openMSXIconFile,
		'"%s"' % ZMBVCodecFile,
		'"%s"' % ZMBVFilesFile,
		'"%s"' % catapultBinFile,
		'"%s"' % catapultDocFile,
		'"%s"' % catapultBitmapsFile,
		'"%s"' % catapultDialogsFile,
		'"%s"' % catapultIconsFile,
		'"%s"' % catapultReadmeFile,
		))

	# Run Candle
	print(candleCmd)
	system(candleCmd)

	msiFileName = info.packageFileName + '-bin.msi'
	msiFilePath = joinpath(info.packagePath, msiFileName)
	if exists(msiFilePath):
		unlink(msiFilePath)

	print('Generating ' + msiFilePath)

	lightCmd = ' '.join((
		'light.exe',
		'-o "%s"' % msiFilePath,
		'-sw1076',
		'-ext WixUtilExtension',
		'-ext WixUIExtension',
		'-loc "%s"' % joinpath(info.packageWindowsPath, 'openmsx1033.wxl'),
		'"%s"' % openMSXObjFile,
		'"%s"' % openMSXExeObjFile,
		'"%s"' % openMSXDocObjFile,
		'"%s"' % openMSXShareObjFile,
		'"%s"' % openMSXIconObjFile,
		'"%s"' % ZMBVCodecObjFile,
		'"%s"' % ZMBVFilesObjFile,
		'"%s"' % catapultBinObjFile,
		'"%s"' % catapultDocObjFile,
		'"%s"' % catapultBitmapsObjFile,
		'"%s"' % catapultDialogsObjFile,
		'"%s"' % catapultIconsObjFile,
		'"%s"' % catapultReadmeObjFile,
		))

	# Run Light
	print(lightCmd)
	system(lightCmd)

	# Zip up the MSI
	zipFileName = info.packageFileName + '-bin-msi.zip'
	zipFilePath = joinpath(info.packagePath, zipFileName)

	print('Generating ' + zipFilePath)
	zipFile = ZipFile(zipFilePath, 'w')
	zipFile.write(msiFilePath, msiFileName, ZIP_DEFLATED)
	zipFile.close()

if __name__ == '__main__':
	if len(sys.argv) == 4:
		packageMSI(PackageInfo(*sys.argv[1 : ]))
	else:
		print(
			'Usage: python3 packagemsi.py '
			'platform configuration catapultPath',
			file=sys.stderr
			)
		sys.exit(2)
