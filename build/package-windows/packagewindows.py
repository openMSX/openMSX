import os, sys
import install

def DeleteDirectoryIfExists(top):
	if os.path.exists(top):
		DeleteDirectory(top)
		
def DeleteDirectory(top):
	for root, dirs, files in os.walk(top, topdown=False):
		for name in files:
			os.remove(os.path.join(root, name))
		for name in dirs:
			os.rmdir(os.path.join(root, name))

def GenerateInstallFiles(info):
	DeleteDirectoryIfExists(info.makeInstallPath)
	install.installAll(info.makeInstallPath + os.sep, 'bin', 'share', 'doc', info.openmsxExePath, 'mingw32', True, True)

def WalkPath(sourcePath):
    if os.path.isfile(sourcePath):
        filenames = list()
        filenames.append(os.path.basename(sourcePath))
        yield os.path.dirname(sourcePath), list(), filenames
    else:
        for dirpath, dirnames, filenames in os.walk(sourcePath):
            yield dirpath, dirnames, filenames

class PackageInfo:

	def __init__(self, platform, configuration, version, catapultPath):
		
		self.platform = platform.lower()
		if self.platform == 'win32':
			self.architecture = 'x86'
			self.platform = 'Win32'
			self.win64 = False
		elif self.platform == 'x64':
			self.architecture = 'x64'
			self.platform = 'x64'
			self.win64 = True
		else:
			raise ValueError, 'Wrong platform: ' + platform
			
		self.configuration = configuration.lower()
		if self.configuration == 'release':
			self.configuration = 'Release'
			self.catapultConfiguration = 'Unicode Release'
		elif self.configuration == 'Developer':
			self.configuration = 'Developer'
			self.catapultConfiguration = 'Unicode Debug'
		elif self.configuration == 'Debug':
			self.configuration = 'Debug'
			self.catapultConfiguration = 'Unicode Debug'
		else:
			raise ValueError, 'Wrong configuration: ' + architecture

		self.version = version
		self.catapultPath = catapultPath
		
		# Useful variables
		self.buildFlavor = self.platform + '-VC-' + self.configuration
		self.buildPath = os.path.join('derived', self.buildFlavor)
		self.sourcePath = 'src'
		self.codecPath = 'Contrib\\codec\\Win32'
		self.packageWindowsPath = 'build\\package-windows'
		
		self.catapultSourcePath = os.path.join(self.catapultPath, 'src')
		self.catapultBuildFlavor = self.platform + '-VC-' + self.catapultConfiguration
		self.catapultBuildPath = os.path.join(self.catapultPath, os.path.join('derived', self.catapultBuildFlavor))
		self.catapultExePath = os.path.join(self.catapultBuildPath, 'install\\Catapult.exe')
		self.catapultPdbPath = os.path.join(self.catapultBuildPath, 'install\\Catapult.pdb')
		
		self.openmsxExePath = os.path.join(self.buildPath, 'install\\openmsx.exe')
		self.openmsxPdbPath = os.path.join(self.buildPath, 'install\\openmsx.pdb')
		
		self.packagePath = os.path.join(self.buildPath, 'package-windows')
		self.makeInstallPath = os.path.join(self.packagePath, 'install')
		
		self.installerFileName = 'openmsx-' + version + '-VC-' + self.architecture
		
if __name__ == '__main__':
	if len(sys.argv) == 5:
		info = PackageInfo(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4])
	else:
		print >> sys.stderr, \
			'Usage: python packagewindowsinfo.py architecture, configuration, version, catapultPath'
		sys.exit(2)
