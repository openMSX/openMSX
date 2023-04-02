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
	version = '2.1.0'
	fileLength = 764073
	checksums = {
		'sha256':
			'04de91e7e6763039bc11940095cd9c7f880baba82196a7765f727ac05a993c95',
		}

	@classmethod
	def getTarballName(cls):
		return '%s-%s.tgz' % (cls.sourceName, cls.version)

class LibPNG(DownloadablePackage):
	downloadURL = 'http://downloads.sourceforge.net/libpng'
	niceName = 'libpng'
	sourceName = 'libpng'
	version = '1.6.39'
	fileLength = 1507328
	checksums = {
		'sha256':
			'af4fb7f260f839919e5958e5ab01a275d4fe436d45442a36ee62f73e5beb75ba',
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
	version = '2.26.4'
	fileLength = 8084255
	checksums = {
		'sha256':
			'1a0f686498fb768ad9f3f80b39037a7d006eac093aad39cb4ebcc832a8887231',
		}

class SDL2_ttf(DownloadablePackage):
	downloadURL = 'http://www.libsdl.org/projects/SDL_ttf/release'
	niceName = 'SDL2_ttf'
	sourceName = 'SDL2_ttf'
	version = '2.0.15'
	fileLength = 4479718
	checksums = {
		'sha256':
			'a9eceb1ad88c1f1545cd7bd28e7cbc0b2c14191d40238f531a15b01b1b22cd33',
		}

class TCL(DownloadablePackage):
	downloadURL = 'http://downloads.sourceforge.net/tcl'
	niceName = 'Tcl'
	sourceName = 'tcl'
	version = '8.6.13'
	fileLength = 10834396
	checksums = {
		'sha256':
			'43a1fae7412f61ff11de2cfd05d28cfc3a73762f354a417c62370a54e2caf066',
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
