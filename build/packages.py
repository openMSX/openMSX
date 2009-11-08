# $Id$

from urlparse import urljoin

class Package(object):
	'''Abstract base class for packages.
	'''
	niceName = None
	sourceName = None

	@classmethod
	def getMakeName(cls):
		return cls.sourceName.upper()

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

class DirectX(DownloadablePackage):
	downloadURL = 'http://alleg.sourceforge.net/files'
	niceName = 'DirectX'
	sourceName = 'dx'
	version = '70'
	fileLength = 236675
	checksums = {
		'sha256':
			'59f489a7d9f51c70fe37fbb5a6225d4716a97ab774c58138f1dc4661a80356f0',
		}

	@classmethod
	def getMakeName(cls):
		return 'DIRECTX'

	@classmethod
	def getSourceDirName(cls):
		# Note: The tarball does not contain a source dir.
		#       We only redefine this to keep the name of the install
		#       timestamp the same.
		return '%s%s' % (cls.sourceName, cls.version)

	@classmethod
	def getTarballName(cls):
		return '%s%s_mgw.tar.gz' % (cls.sourceName, cls.version)

class Expat(DownloadablePackage):
	downloadURL = 'http://downloads.sourceforge.net/expat'
	niceName = 'Expat'
	sourceName = 'expat'
	version = '2.0.1'
	fileLength = 446456
	checksums = {
		'sha256':
			'847660b4df86e707c9150e33cd8c25bc5cd828f708c7418e765e3e983a2e5e93',
		}

class FreeType(DownloadablePackage):
	downloadURL = 'http://downloads.sourceforge.net/freetype'
	niceName = 'FreeType'
	sourceName = 'freetype'
	version = '2.3.9'
	fileLength = 1809558
	checksums = {
		'sha256':
			'3c3fc489c75af93e9f4367951114b0274bddef60d70ffe419b7e22cda210b9c0',
		}

class GLEW(DownloadablePackage):
	downloadURL = 'http://downloads.sourceforge.net/glew'
	niceName = 'GLEW'
	sourceName = 'glew'
	version = '1.5.1'
	fileLength = 394566
	checksums = {
		'sha256':
			'89e63d085cb563c32a191e3cd4907a192484f10438a6679f1349456db4b9c10a',
		}

	@classmethod
	def getTarballName(cls):
		return '%s-%s-src.tgz' % (cls.sourceName, cls.version)

class JACK(DownloadablePackage):
	downloadURL = 'http://jackaudio.org/downloads/'
	niceName = 'Jack'
	sourceName = 'jack-audio-connection-kit'
	version = '0.116.2'
	fileLength = 944106
	checksums = {
		'sha256':
			'ce6e1f61a3b003137af56b749e5ed4274584167c0877ea9ef2d83f47b11c8d3d',
		}

	@classmethod
	def getMakeName(cls):
		return 'JACK'

class LibPNG(DownloadablePackage):
	downloadURL = 'http://downloads.sourceforge.net/libpng'
	niceName = 'libpng'
	sourceName = 'libpng'
	version = '1.2.35'
	fileLength = 802267
	checksums = {
		'sha256':
			'1da5c80096e8a014911e00fab4661c0f77ce523ae4d41308815f307ee709fc7f',
		}

	@classmethod
	def getMakeName(cls):
		return 'PNG'

class LibXML2(DownloadablePackage):
	downloadURL = 'http://xmlsoft.org/sources'
	niceName = 'libxml2'
	sourceName = 'libxml2'
	version = '2.7.3'
	fileLength = 4789450
	checksums = {
		'sha256':
			'432464d8c9bd8060d9c1fdef1cfa75803c1a363ceac20b21f8c7e34e056e5a98',
		}

	@classmethod
	def getMakeName(cls):
		return 'XML'

class OGG(DownloadablePackage):
	downloadURL = 'http://downloads.xiph.org/releases/ogg'
	niceName = 'libogg'
	sourceName = 'libogg'
	version = '1.1.4'
	fileLength = 439365
	checksums = {
		'sha256':
			'9354c183fd88417c2860778b60b7896c9487d8f6e58b9fec3fdbf971142ce103',
		}

	@classmethod
	def getMakeName(cls):
		return 'OGG'

class OGGZ(DownloadablePackage):
	downloadURL = 'http://downloads.xiph.org/releases/liboggz'
	niceName = 'liboggz'
	sourceName = 'liboggz'
	version = '0.9.9'
	fileLength = 637538
	checksums = {
		'sha256':
			'8d8a05752f739d0b377040e36c9f6cfa4dac35b9d55deac2de30f377972fcf75',
		}

	@classmethod
	def getMakeName(cls):
		return 'OGGZ'

class OpenGL(Package):
	niceName = 'OpenGL'
	sourceName = 'gl'

class SDL(DownloadablePackage):
	downloadURL = 'http://www.libsdl.org/release'
	niceName = 'SDL'
	sourceName = 'SDL'
	version = '1.2.13'
	fileLength = 3373673
	checksums = {
		'sha256':
			'94f99df1d60f296b57f4740650a71b6425da654044ca30f8f0ce34934429e132',
		}

class SDL_image(DownloadablePackage):
	downloadURL = 'http://www.libsdl.org/projects/SDL_image/release'
	niceName = 'SDL_image'
	sourceName = 'SDL_image'
	version = '1.2.7'
	fileLength = 1315517
	checksums = {
		'sha256':
			'14e4d9932ae2af03d814cca9e56ab9ba0091ffe06c9387dde74dfb03a4dde3b3',
		}

class SDL_ttf(DownloadablePackage):
	downloadURL = 'http://www.libsdl.org/projects/SDL_ttf/release'
	niceName = 'SDL_ttf'
	sourceName = 'SDL_ttf'
	version = '2.0.9'
	fileLength = 3143838
	checksums = {
		'sha256':
			'b4248876798b43d0fae1931cf8ae249f4f67a87736f97183f035f34aab554653',
		}

class SQLite(DownloadablePackage):
	downloadURL = 'http://www.sqlite.org/'
	niceName = 'SQLite'
	sourceName = 'sqlite'
	version = '3.6.16'
	fileLength = 1353155
	checksums = {
		'sha256':
			'f576c1be29726c03c079ac466095776b2c5b1ac8f996af1422b251855a0619a9',
		}

	@classmethod
	def getTarballName(cls):
		return 'sqlite-amalgamation-%s.tar.gz' % cls.version

class TCL(DownloadablePackage):
	downloadURL = 'http://downloads.sourceforge.net/tcl'
	niceName = 'Tcl'
	sourceName = 'tcl'
	version = '8.5.7'
	fileLength = 4421720
	checksums = {
		'sha256':
			'67d28d51a8d04c37114030276503bc8859a4b291bc33133556ab2d11303e66f2',
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
	version = '1.0'
	fileLength = 1919343
	checksums = {
		'sha256':
			'd5ac6867143b295da41aac1fb24357b6c7f388bf87985630168a47ed2ed8b048',
		}

	@classmethod
	def getMakeName(cls):
		return 'THEORA'

class Vorbis(DownloadablePackage):
	downloadURL = 'http://downloads.xiph.org/releases/vorbis'
	niceName = 'libvorbis'
	sourceName = 'libvorbis'
	version = '1.2.3'
	fileLength = 1474492
	checksums = {
		'sha256':
			'c679d1e5e45a3ec8aceb5e71de8e3712630b7a6dec6952886c17435a65955947',
		}

	@classmethod
	def getMakeName(cls):
		return 'VORBIS'

class ZLib(DownloadablePackage):
	downloadURL = 'http://downloads.sourceforge.net/libpng'
	niceName = 'zlib'
	sourceName = 'zlib'
	version = '1.2.3'
	fileLength = 496597
	checksums = {
		'sha256':
			'1795c7d067a43174113fdf03447532f373e1c6c57c08d61d9e4e9be5e244b05e',
		}

# Build a dictionary of packages using introspection.
def _discoverPackages(localObjects):
	for obj in localObjects:
		if isinstance(obj, type) and issubclass(obj, Package):
			if not (obj is Package or obj is DownloadablePackage):
				yield obj.getMakeName(), obj
_packagesByName = dict(_discoverPackages(locals().itervalues()))

def getPackage(makeName):
	return _packagesByName[makeName]

def iterDownloadablePackages():
	for package in _packagesByName.itervalues():
		if issubclass(package, DownloadablePackage):
			yield package
