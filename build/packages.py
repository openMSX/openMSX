from urllib.parse import urljoin

class Package(object):
	'''Abstract base class for packages.
	'''
	niceName = None
	sourceName = None

	@classmethod
	def getMakeName(cls):
		return cls.sourceName.upper().replace('-', '_')

class DownloadablePackage(Package):
	'''Abstract base class for packages that can be downloaded.
	'''
	downloadURL = None
	version = None
	fileLength = None
	checksums = None

	@classmethod
	def getSourceDirName(cls):
		'''Returns the desired name of the top-level source directory.
		This might not match the actual name inside the downloaded archive,
		but we can perform a rename on extraction to fix that.
		'''
		return '%s-%s' % (cls.sourceName, cls.version)

	@classmethod
	def getTarballName(cls):
		return '%s-%s.tar.gz' % (cls.sourceName, cls.version)

	@classmethod
	def getURL(cls):
		return urljoin(cls.downloadURL + '/', cls.getTarballName())

class ALSA(Package):
	niceName = 'ALSA'
	sourceName = 'alsa-lib'

	@classmethod
	def getMakeName(cls):
		return 'ALSA'

class FreeType(DownloadablePackage):
	downloadURL = 'http://downloads.sourceforge.net/freetype'
	niceName = 'FreeType'
	sourceName = 'freetype'
	version = '2.11.1'
	fileLength = 3451359
	checksums = {
		'sha256':
			'f8db94d307e9c54961b39a1cc799a67d46681480696ed72ecf78d4473770f09b',
		}

class GLEW(DownloadablePackage):
	downloadURL = 'http://downloads.sourceforge.net/glew'
	niceName = 'GLEW'
	sourceName = 'glew'
	version = '2.2.0'
	fileLength = 835861
	checksums = {
		'sha256':
			'd4fc82893cfb00109578d0a1a2337fb8ca335b3ceccf97b97e5cc7f08e4353e1',
		}

	@classmethod
	def getTarballName(cls):
		return '%s-%s.tgz' % (cls.sourceName, cls.version)

class LibPNG(DownloadablePackage):
	downloadURL = 'http://downloads.sourceforge.net/libpng'
	niceName = 'libpng'
	sourceName = 'libpng'
	version = '1.6.44'
	fileLength = 1545850
	checksums = {
		'sha256':
			'8c25a7792099a0089fa1cc76c94260d0bb3f1ec52b93671b572f8bb61577b732',
		}

	@classmethod
	def getMakeName(cls):
		return 'PNG'

class OGG(DownloadablePackage):
	downloadURL = 'http://downloads.xiph.org/releases/ogg'
	niceName = 'libogg'
	sourceName = 'libogg'
	version = '1.3.5'
	fileLength = 593071
	checksums = {
		'sha256':
			'0eb4b4b9420a0f51db142ba3f9c64b333f826532dc0f48c6410ae51f4799b664',
		}

	@classmethod
	def getMakeName(cls):
		return 'OGG'

class OpenGL(Package):
	niceName = 'OpenGL'
	sourceName = 'gl'

class PkgConfig(DownloadablePackage):
	downloadURL = 'https://pkg-config.freedesktop.org/releases'
	niceName = 'pkg-config'
	sourceName = 'pkg-config'
	version = '0.29.2'
	fileLength = 2016830
	checksums = {
		'sha256':
			'6fc69c01688c9458a57eb9a1664c9aba372ccda420a02bf4429fe610e7e7d591',
		}

class SDL2(DownloadablePackage):
	downloadURL = 'https://www.libsdl.org/release'
	niceName = 'SDL2'
	sourceName = 'SDL2'
	version = '2.30.7'
	fileLength = 7525092
	checksums = {
		'sha256':
			'2508c80438cd5ff3bbeb8fe36b8f3ce7805018ff30303010b61b03bb83ab9694',
		}

class SDL2_ttf(DownloadablePackage):
	downloadURL = 'http://www.libsdl.org/projects/SDL_ttf/release'
	niceName = 'SDL2_ttf'
	sourceName = 'SDL2_ttf'
	version = '2.22.0'
	fileLength = 14314901
	checksums = {
		'sha256':
			'd48cbd1ce475b9e178206bf3b72d56b66d84d44f64ac05803328396234d67723',
		}

class TCL(DownloadablePackage):
	downloadURL = 'http://downloads.sourceforge.net/tcl'
	niceName = 'Tcl'
	sourceName = 'tcl'
	version = '8.6.15'
	fileLength = 11765231
	checksums = {
		'sha256':
			'861e159753f2e2fbd6ec1484103715b0be56be3357522b858d3cbb5f893ffef1',
		}

	@classmethod
	def getSourceDirName(cls):
		return '%s%s' % (cls.sourceName, cls.version)

	@classmethod
	def getTarballName(cls):
		return '%s%s-src.tar.gz' % (cls.sourceName, cls.version)

class Theora(DownloadablePackage):
	downloadURL = 'http://downloads.xiph.org/releases/theora'
	niceName = 'libtheora'
	sourceName = 'libtheora'
	version = '1.1.1'
	fileLength = 2111877
	checksums = {
		'sha256':
			'40952956c47811928d1e7922cda3bc1f427eb75680c3c37249c91e949054916b',
		}

	@classmethod
	def getMakeName(cls):
		return 'THEORA'

class Vorbis(DownloadablePackage):
	downloadURL = 'http://downloads.xiph.org/releases/vorbis'
	niceName = 'libvorbis'
	sourceName = 'libvorbis'
	version = '1.3.7'
	fileLength = 1658963
	checksums = {
		'sha256':
			'0e982409a9c3fc82ee06e08205b1355e5c6aa4c36bca58146ef399621b0ce5ab',
		}

	@classmethod
	def getMakeName(cls):
		return 'VORBIS'

class ZLib(DownloadablePackage):
	downloadURL = 'http://downloads.sourceforge.net/libpng'
	niceName = 'zlib'
	sourceName = 'zlib'
	version = '1.2.11'
	fileLength = 607698
	checksums = {
		'sha256':
			'c3e5e9fdd5004dcb542feda5ee4f0ff0744628baf8ed2dd5d66f8ca1197cb1a1',
		}

# Build a dictionary of packages using introspection.
_packagesByName = {
	obj.getMakeName(): obj
	for obj in locals().values()
	if isinstance(obj, type)
		and issubclass(obj, Package)
		and obj is not Package
		and obj is not DownloadablePackage
	}

def getPackage(makeName):
	return _packagesByName[makeName]

def iterDownloadablePackages():
	for package in _packagesByName.values():
		if issubclass(package, DownloadablePackage):
			yield package
