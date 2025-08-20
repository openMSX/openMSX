from install import installAll
from version import (
	extractRevisionNumber, getVersionedPackageName, packageVersionNumber,
	releaseFlag
	)

from os import makedirs, remove, rmdir, sep, walk
from os.path import exists, join as joinpath
import sys

def emptyOrCreateDirectory(top):
	if exists(top):
		_emptyDirectory(top)
	else:
		makedirs(top)

def _emptyDirectory(top):
	for root, dirs, files in walk(top, topdown = False):
		for name in files:
			remove(joinpath(root, name))
		for name in dirs:
			rmdir(joinpath(root, name))

def generateInstallFiles(info):
	emptyOrCreateDirectory(info.makeInstallPath)
	installAll(
		info.makeInstallPath + sep, 'bin', 'share', 'doc',
		info.openmsxExePath, 'mingw32', True, True
		)

class PackageInfo(object):

	def __init__(self, platform, configuration, catapultPath):

		self.platform = platform.lower()
		if self.platform == 'win32':
			self.cpu = 'x86'
			self.platform = 'Win32'
			self.win64 = False
		elif self.platform == 'x64':
			self.cpu = 'x64'
			self.platform = 'x64'
			self.win64 = True
		else:
			raise ValueError('Wrong platform: ' + platform)

		self.configuration = configuration.lower()
		if self.configuration == 'release':
			self.configuration = 'Release'
			self.catapultConfiguration = 'Unicode Release'
		elif self.configuration == 'developer':
			self.configuration = 'Developer'
			self.catapultConfiguration = 'Unicode Debug'
		elif self.configuration == 'debug':
			self.configuration = 'Debug'
			self.catapultConfiguration = 'Unicode Debug'
		else:
			raise ValueError('Wrong configuration: ' + configuration)

		self.packageCatapult = (catapultPath != "NOCATAPULT")
		self.catapultPath = catapultPath

		# Useful variables
		self.buildFlavor = self.platform + '-VC-' + self.configuration
		self.buildPath = joinpath('derived', self.buildFlavor)
		self.sourcePath = 'src'
		self.codecPath = 'Contrib\\codec\\Win32'
		self.packageWindowsPath = 'build\\package-windows'

		self.catapultSourcePath = joinpath(self.catapultPath, 'src')
		self.catapultBuildFlavor = '%s-VC-%s' % (
			self.platform, self.catapultConfiguration
			)
		self.catapultBuildPath = joinpath(
			self.catapultPath, joinpath('derived', self.catapultBuildFlavor)
			)
		self.catapultExePath = joinpath(
			self.catapultBuildPath, 'install\\catapult.exe'
			)
		self.catapultPdbPath = joinpath(
			self.catapultBuildPath, 'install\\catapult.pdb'
			)

		self.openmsxExePath = joinpath(self.buildPath, 'install\\openmsx.exe')
		self.openmsxPdbPath = joinpath(self.buildPath, 'install\\openmsx.pdb')

		self.packagePath = joinpath(self.buildPath, 'package-windows')
		self.makeInstallPath = joinpath(self.packagePath, 'install')

		self.version = packageVersionNumber
		if releaseFlag:
			self.version += '.0'
		else:
			self.version += '.%d' % extractRevisionNumber()

		# <product>-<version>-<os>-<compiler>-<cpu>-<filetype>.ext
		self.os = 'windows'
		self.compiler = 'vc'

		self.packageFileName = '-'.join((
			getVersionedPackageName(),
			self.os,
			self.compiler,
			self.cpu
			))

if __name__ == '__main__':
	if len(sys.argv) == 4:
		PackageInfo(*sys.argv[1 : ])
	else:
		print(
			'Usage: py packagewindows.py '
			'platform configuration catapultPath',
			file=sys.stderr
			)
		sys.exit(2)
