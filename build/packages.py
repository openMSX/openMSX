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

class ALSA(DownloadablePackage):
	downloadURL = 'ftp://ftp.alsa-project.org/pub/lib/'
	niceName = 'ALSA'
	sourceName = 'alsa-lib'
	version = '1.1.0'
	fileLength = 929874
	checksums = {
		'sha256':
			'dfde65d11e82b68f82e562ab6228c1fb7c78854345d3c57e2c68a9dd3dae1f15',
		}

	@classmethod
	def getTarballName(cls):
		return '%s-%s.tar.bz2' % (cls.sourceName, cls.version)

	@classmethod
	def getMakeName(cls):
		return 'ALSA'

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

class FreeType(DownloadablePackage):
	downloadURL = 'http://downloads.sourceforge.net/freetype'
	niceName = 'FreeType'
	sourceName = 'freetype'
	version = '2.4.12'
	fileLength = 2117909
	checksums = {
		'sha256':
			'9755806ff72cba095aad47dce6f0ad66bd60fee2a90323707d2cac5c526066f0',
		}

class GLEW(DownloadablePackage):
	downloadURL = 'http://downloads.sourceforge.net/glew'
	niceName = 'GLEW'
	sourceName = 'glew'
	version = '1.9.0'
	fileLength = 544440
	checksums = {
		'sha256':
			'9b36530e414c95d6624be9d6815a5be1531d1986300ae5903f16977ab8aeb787',
		}

	@classmethod
	def getTarballName(cls):
		return '%s-%s.tgz' % (cls.sourceName, cls.version)

class LibPNG(DownloadablePackage):
	downloadURL = 'http://downloads.sourceforge.net/libpng'
	niceName = 'libpng'
	sourceName = 'libpng'
	version = '1.6.20'
	fileLength = 1417478
	checksums = {
		'sha256':
			'3d3bdc16f973a62fb1d26464fe2fe19f51dde9b883feff3e059d18ec1457b199',
		}

	@classmethod
	def getMakeName(cls):
		return 'PNG'

class OGG(DownloadablePackage):
	downloadURL = 'http://downloads.xiph.org/releases/ogg'
	niceName = 'libogg'
	sourceName = 'libogg'
	version = '1.3.0'
	fileLength = 425144
	checksums = {
		'sha256':
			'a8de807631014615549d2356fd36641833b8288221cea214f8a72750efe93780',
		}

	@classmethod
	def getMakeName(cls):
		return 'OGG'

class OpenGL(Package):
	niceName = 'OpenGL'
	sourceName = 'gl'

class SDL(DownloadablePackage):
	downloadURL = 'http://www.libsdl.org/release'
	niceName = 'SDL'
	sourceName = 'SDL'
	version = '1.2.15'
	fileLength = 3920622
	checksums = {
		'sha256':
			'd6d316a793e5e348155f0dd93b979798933fb98aa1edebcc108829d6474aad00',
		}

class SDL_ttf(DownloadablePackage):
	downloadURL = 'http://www.libsdl.org/projects/SDL_ttf/release'
	niceName = 'SDL_ttf'
	sourceName = 'SDL_ttf'
	version = '2.0.11'
	fileLength = 4053686
	checksums = {
		'sha256':
			'724cd895ecf4da319a3ef164892b72078bd92632a5d812111261cde248ebcdb7',
		}

class TCL_ANDROID(DownloadablePackage):
	downloadURL = 'http://downloads.sourceforge.net/tcl'
	niceName = 'Tcl'
	sourceName = 'tcl'
	version = '8.5.11'
	fileLength = 4484001
	checksums = {
		'sha256':
			'8addc385fa6b5be4605e6d68fbdc4c0e674c5af1dc1c95ec5420390c4b08042a',
		}

	@classmethod
	def getMakeName(cls):
		return 'TCL_ANDROID'

	@classmethod
	def getSourceDirName(cls):
		return '%s%s' % (cls.sourceName, cls.version)

	@classmethod
	def getTarballName(cls):
		return '%s%s-src.tar.gz' % (cls.sourceName, cls.version)

class TCL(DownloadablePackage):
	downloadURL = 'http://downloads.sourceforge.net/tcl'
	niceName = 'Tcl'
	sourceName = 'tcl'
	version = '8.5.18'
	fileLength = 4534628
	checksums = {
		'sha256':
			'032be57a607bdf252135b52fac9e3a7016e526242374ac7637b083ecc4c5d3c9',
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
	version = '1.3.3'
	fileLength = 1592663
	checksums = {
		'sha256':
			'6d747efe7ac4ad249bf711527882cef79fb61d9194c45b5ca5498aa60f290762',
		}

	@classmethod
	def getMakeName(cls):
		return 'VORBIS'

class ZLib(DownloadablePackage):
	downloadURL = 'http://downloads.sourceforge.net/libpng'
	niceName = 'zlib'
	sourceName = 'zlib'
	version = '1.2.8'
	fileLength = 571091
	checksums = {
		'sha256':
			'36658cb768a54c1d4dec43c3116c27ed893e88b02ecfcb44f2166f9c0b7f2a0d',
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
