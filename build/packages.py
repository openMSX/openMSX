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
	version = '2.15.0'
	fileLength = 4262480
	checksums = {
		'sha256':
			'9186627467a616d9a9e3e574748ac0c45fa77398e6364edef3041a8ba8d92f64',
		}

class GLEW(DownloadablePackage):
	downloadURL = 'http://downloads.sourceforge.net/glew'
	niceName = 'GLEW'
	sourceName = 'glew'
	version = '2.3.1'
	fileLength = 866060
	checksums = {
		'sha256':
			'b64790f94b926acd7e8f84c5d6000a86cb43967bd1e688b03089079799c9e889',
		}

	@classmethod
	def getTarballName(cls):
		return '%s-%s.tgz' % (cls.sourceName, cls.version)

class LibPNG(DownloadablePackage):
	downloadURL = 'http://downloads.sourceforge.net/libpng'
	niceName = 'libpng'
	sourceName = 'libpng'
	version = '1.6.59'
	fileLength = 1589315
	checksums = {
		'sha256':
			'86a3e4b501f7f50e392c4e456ea158893e9a595b1c63eb88c7bb9f6cf8772dad',
		}

	@classmethod
	def getMakeName(cls):
		return 'PNG'

class OGG(DownloadablePackage):
	downloadURL = 'http://downloads.xiph.org/releases/ogg'
	niceName = 'libogg'
	sourceName = 'libogg'
	version = '1.3.6'
	fileLength = 604469
	checksums = {
		'sha256':
			'83e6704730683d004d20e21b8f7f55dcb3383cdf84c0daedf30bde175f774638',
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
	version = '2.32.10'
	fileLength = 7630262
	checksums = {
		'sha256':
			'5f5993c530f084535c65a6879e9b26ad441169b3e25d789d83287040a9ca5165',
		}

class SDL2_ttf(DownloadablePackage):
	downloadURL = 'http://www.libsdl.org/projects/SDL_ttf/release'
	niceName = 'SDL2_ttf'
	sourceName = 'SDL2_ttf'
	version = '2.24.0'
	fileLength = 13335900
	checksums = {
		'sha256':
			'0b2bf1e7b6568adbdbc9bb924643f79d9dedafe061fa1ed687d1d9ac4e453bfd',
		}

class TCL(DownloadablePackage):
	downloadURL = 'http://downloads.sourceforge.net/tcl'
	niceName = 'Tcl'
	sourceName = 'tcl'
	version = '8.6.18'
	fileLength = 11816279
	checksums = {
		'sha256':
			'14f9af32b1767ff718477a8f974ad03c34341097e6b43f4ce54644ee974e268e',
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
	downloadURL = 'https://zlib.net/fossils/'
	niceName = 'zlib'
	sourceName = 'zlib'
	version = '1.3.2'
	fileLength = 1502830
	checksums = {
		'sha256':
			'bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16',
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
