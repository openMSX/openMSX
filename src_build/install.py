from fileutils import (
	installDirs, installFile, installSymlink, installTree, scanTree
	)
from makeutils import extractMakeVariables, parseBool

from os import listdir
from os.path import basename, expanduser, isdir, splitext
import os
import sys

def installAll(
	installPrefix, binaryDestDir, shareDestDir, docDestDir,
	binaryBuildPath, targetPlatform, cbios, symlinkForBinary
	):
	platformVars = extractMakeVariables(
		'src_build/platform-%s.mk' % targetPlatform,
		dict.fromkeys(
			('COMPILE_FLAGS', 'LINK_FLAGS', 'TARGET_FLAGS',
			 'OPENMSX_TARGET_CPU'),
			''
			)
		)
	binaryFileName = 'openmsx' + platformVars.get('EXEEXT', '')

	docNodeVars = extractMakeVariables('doc/node.mk')
	docsToInstall = [
		'doc/' + fileName for fileName in docNodeVars['INSTALL_DOCS'].split()
		]

	print('  Executable...')
	installDirs(installPrefix + binaryDestDir)
	installFile(
		binaryBuildPath,
		installPrefix + binaryDestDir + '/' + binaryFileName
		)

	print('  Data files...')
	installDirs(installPrefix + shareDestDir)
	installTree('share', installPrefix + shareDestDir, scanTree('share'))

	print('  Documentation...')
	installDirs(installPrefix + docDestDir)
	for path in docsToInstall:
		installFile(path, installPrefix + docDestDir + '/' + basename(path))
	installDirs(installPrefix + docDestDir + '/manual')
	for fileName in listdir('doc/manual'):
		if splitext(fileName)[1] in ('.html', '.css', '.png'):
			installFile(
				'doc/manual/' + fileName,
				installPrefix + docDestDir + '/manual/' + fileName
				)

	if cbios:
		print('  C-BIOS...')
		installFile(
			'Contrib/README.cbios', installPrefix + docDestDir + '/cbios.txt'
			)
		installTree(
			'Contrib/cbios', installPrefix + shareDestDir + '/machines',
			scanTree('Contrib/cbios')
			)
		installDirs(installPrefix + shareDestDir + '/systemroms/cbios-old')
		installTree(
			'Contrib/cbios-old',
			installPrefix + shareDestDir + '/systemroms/cbios-old',
			scanTree('Contrib/cbios-old')
			)

	if hasattr(os, 'symlink') and os.name != 'nt':
		print('  Creating symlinks...')
		for machine, alias in (
			('National_CF-3300', 'msx1'),
			('Toshiba_HX-10', 'msx1_eu'),
			('Sony_HB-F900', 'msx2'),
			('Philips_NMS_8250', 'msx2_eu'),
			('Panasonic_FS-A1WSX', 'msx2plus'),
			('Panasonic_FS-A1GT', 'turbor'),
			):
			installSymlink(
				machine + ".xml",
				installPrefix + shareDestDir + '/machines/' + alias + ".xml"
				)
		if symlinkForBinary and installPrefix == '':
			def createSymlinkToBinary(linkDir):
				if linkDir != binaryDestDir and isdir(linkDir):
					try:
						installSymlink(
							binaryDestDir + '/' + binaryFileName,
							linkDir + '/' + binaryFileName
							)
					except OSError:
						return False
					else:
						return True
				else:
					return False
			success = createSymlinkToBinary('/usr/local/bin')
			if not success:
				createSymlinkToBinary(expanduser('~/bin'))

def main(
	installPrefix, binaryDestDir, shareDestDir, docDestDir,
	binaryBuildPath, targetPlatform, verboseStr, cbiosStr
	):
	customVars = extractMakeVariables('src_build/custom.mk')

	try:
		verbose = parseBool(verboseStr)
		cbios = parseBool(cbiosStr)
		symlinkForBinary = parseBool(customVars['SYMLINK_FOR_BINARY'])
	except ValueError as ex:
		print('Invalid argument:', ex, file=sys.stderr)
		sys.exit(2)

	if installPrefix and not installPrefix.endswith('/'):
		# Just in case the destination directories are not absolute.
		installPrefix += '/'

	if verbose:
		print('Installing openMSX:')

	try:
		installAll(
			installPrefix, binaryDestDir, shareDestDir, docDestDir,
			binaryBuildPath, targetPlatform, cbios, symlinkForBinary
			)
	except OSError as ex:
		print('Installation failed:', ex, file=sys.stderr)
		sys.exit(1)

	if verbose:
		print('Installation complete... have fun!')
		print((
			'Notice: if you want to emulate real MSX systems and not only the '
			'free C-BIOS machines, put the system ROMs in one of the following '
			'directories: %s/systemroms or '
			'~/.openMSX/share/systemroms\n'
			'If you want openMSX to find MSX software referred to from replays '
			'or savestates you get from your friends, copy that MSX software to '
			'%s/software or '
			'~/.openMSX/share/software'
			) % (shareDestDir, shareDestDir))

if __name__ == '__main__':
	if len(sys.argv) == 9:
		main(*sys.argv[1 : ])
	else:
		print(
			'Usage: python3 install.py '
			'DESTDIR INSTALL_BINARY_DIR INSTALL_SHARE_DIR INSTALL_DOC_DIR '
			'BINARY_FULL OPENMSX_TARGET_OS INSTALL_VERBOSE INSTALL_CONTRIB',
			file=sys.stderr
			)
		sys.exit(2)
