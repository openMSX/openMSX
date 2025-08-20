from packagewindows import PackageInfo, generateInstallFiles

from os.path import abspath, basename, exists, join as joinpath, relpath
from zipfile import ZIP_DEFLATED, ZipFile
import os, sys

def addFile(zipFile, path, zipPath):
	print('Adding ' + path)
	zipFile.write(path, zipPath, ZIP_DEFLATED)

def addDirectory(zipFile, root, zipPath):
	for path, dirs, files in os.walk(root):
		for name in files:
			thisZipPath = zipPath
			if abspath(root) != abspath(path):
				thisZipPath = joinpath(thisZipPath, relpath(path, root))
			addFile(zipFile, joinpath(path, name), joinpath(thisZipPath, name))

def packageZip(info):
	print('Generating install files...')
	generateInstallFiles(info)

	if not exists(info.packagePath):
		os.mkdir(info.packagePath)

	zipFileName = info.packageFileName + '-bin.zip'
	zipFilePath = joinpath(info.packagePath, zipFileName)
	if exists(zipFilePath):
		os.unlink(zipFilePath)

	print('Generating ' + zipFilePath)
	zipFile = ZipFile(zipFilePath, 'w')

	addDirectory(zipFile, joinpath(info.makeInstallPath, 'doc'), 'doc')
	addDirectory(zipFile, joinpath(info.makeInstallPath, 'share'), 'share')
	addDirectory(zipFile, info.codecPath, 'codec')
	addFile(zipFile, info.openmsxExePath, basename(info.openmsxExePath))
	addFile(
		zipFile, joinpath(info.sourcePath, 'resource\\openmsx.ico'),
		'share\\icons\\openmsx.ico'
		)

	if info.packageCatapult:
		addFile(zipFile, info.catapultExePath, 'Catapult\\bin\\Catapult.exe')
		addDirectory(zipFile, joinpath(info.catapultPath, 'doc'), 'Catapult\\doc')
		addDirectory(
			zipFile, joinpath(info.catapultPath, 'resources\\bitmaps'),
			'Catapult\\resources\\bitmaps'
			)
		addDirectory(
			zipFile, joinpath(info.catapultBuildPath, 'install\\dialogs'),
			'Catapult\\resources\\dialogs'
			)
		addFile(
			zipFile, joinpath(info.catapultSourcePath, 'catapult.xpm'),
			'Catapult\\resources\\icons\\catapult.xpm'
			)
		addFile(
			zipFile, joinpath(info.catapultPath, 'README'),
			'Catapult\\doc\\README'
			)
	zipFile.close()

	zipFileName = info.packageFileName + '-pdb.zip'
	zipFilePath = joinpath(info.packagePath, zipFileName)
	if exists(zipFilePath):
		os.unlink(zipFilePath)

	print('Generating ' + zipFilePath)
	zipFile = ZipFile(zipFilePath, 'w')
	addFile(zipFile, info.openmsxPdbPath, basename(info.openmsxPdbPath))
	if info.packageCatapult:
		addFile(zipFile, info.catapultPdbPath, basename(info.catapultPdbPath))
	zipFile.close()

if __name__ == '__main__':
	if len(sys.argv) == 4:
		packageZip(PackageInfo(*sys.argv[1 : ]))
	else:
		print(
			'Usage: py packagezip.py '
			'platform configuration catapultPath',
			file=sys.stderr
			)
		sys.exit(2)
