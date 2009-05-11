import os, sys
import zipfile
import packagewindows

def AddFile(zip, path, zipPath):
	print 'Adding ' + path
	zip.write(path, zipPath, zipfile.ZIP_DEFLATED)

def AddDirectory(zip, root, zipPath):
	for path, dirs, files in os.walk(root):
		if '.svn' in dirs:
			dirs.remove('.svn')  # don't visit .svn directories
		for name in files:
			thisZipPath = zipPath
			if os.path.abspath(root) != os.path.abspath(path):
				thisZipPath = os.path.join(thisZipPath, os.path.relpath(path, root))
			AddFile(zip, os.path.join(path, name), os.path.join(thisZipPath, name))

def PackageZip(info):

	print 'Generating install files...'
	packagewindows.GenerateInstallFiles(info);
	
	if not os.path.exists(info.packagePath):
		os.mkdir(info.packagePath)

	zipFileName = info.installerFileName + '.zip'
	zipFilePath = os.path.join(info.packagePath, zipFileName)
	if os.path.exists(zipFilePath):
		os.unlink(zipFilePath)

	print 'Generating ' + zipFilePath
	zip = zipfile.ZipFile(zipFilePath, 'w')
	
	AddDirectory(zip, os.path.join(info.makeInstallPath, 'doc'), 'doc')
	AddDirectory(zip, os.path.join(info.makeInstallPath, 'share'), 'share')
	AddDirectory(zip, info.codecPath, 'codec')
	AddFile(zip, info.openmsxExePath, os.path.basename(info.openmsxExePath))
	AddFile(zip, os.path.join(info.sourcePath, 'resource\\openmsx.ico'), 'share\\icons\\openmsx.ico')
	
	AddFile(zip, info.catapultExePath, 'Catapult\\bin\\Catapult.exe')
	AddDirectory(zip, os.path.join(info.catapultPath, 'doc'), 'Catapult\\doc')
	AddDirectory(zip, os.path.join(info.catapultPath, 'resources\\bitmaps'), 'Catapult\\resources\\bitmaps')
	AddDirectory(zip, os.path.join(info.catapultBuildPath, 'install\\dialogs'), 'Catapult\\resources\\dialogs')
	AddFile(zip, os.path.join(info.catapultSourcePath, 'catapult.xpm'), 'Catapult\\resources\\icons\\catapult.xpm')
	AddFile(zip, os.path.join(info.catapultPath, 'AUTHORS'), 'Catapult\doc\\AUTHORS')
	AddFile(zip, os.path.join(info.catapultPath, 'GPL'), 'Catapult\\doc\\GPL')
	AddFile(zip, os.path.join(info.catapultPath, 'README'), 'Catapult\\doc\\README')
	zip.close()
	
	zipFileName = info.installerFileName + '-pdb.zip'
	zipFilePath = os.path.join(info.packagePath, zipFileName)
	if os.path.exists(zipFilePath):
		os.unlink(zipFilePath)

	print 'Generating ' + zipFilePath
	zip = zipfile.ZipFile(zipFilePath, 'w')
	AddFile(zip, info.openmsxPdbPath, os.path.basename(info.openmsxPdbPath))
	AddFile(zip, info.catapultPdbPath, os.path.basename(info.catapultPdbPath))
	zip.close()

if __name__ == '__main__':
	if len(sys.argv) != 5:
		print >> sys.stderr, 'Usage: python packagezip.py platform configuration version catapultPath'
		# E.g. build\package-windows\package.cmd x64 Release 0.7.1 ..\wxCatapult
		sys.exit(2)
	else:
		info = packagewindows.PackageInfo(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4])
		PackageZip(info)
