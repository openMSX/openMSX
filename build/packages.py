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

class LibAO(DownloadablePackage):
	downloadURL = 'http://downloads.xiph.org/releases/ao'
	niceName = 'libao'
	sourceName = 'libao'
	version = '1.1.0'
	fileLength = 397102
	checksums = {
		'sha256':
			'29de5bb9b1726ba890455ef7e562d877df87811febb0d99ee69164b88c171bd4',
		}

	@classmethod
	def getMakeName(cls):
		return 'AO'

class LibPNG(DownloadablePackage):
	downloadURL = 'http://downloads.sourceforge.net/libpng'
	niceName = 'libpng'
	sourceName = 'libpng'
	version = '1.2.50'
	fileLength = 826893
	checksums = {
		'sha256':
			'19f17cd49782fcec8df0f7d1b348448cc3f69ed7e2a59de24bc0907b907f1abc',
		}

	@classmethod
	def getMakeName(cls):
		return 'PNG'

class LibXML2(DownloadablePackage):
	downloadURL = 'http://xmlsoft.org/sources'
	niceName = 'libxml2'
	sourceName = 'libxml2'
	version = '2.8.0' # 2.9.0 and 2.9.1 do not seem to compile...
	fileLength = 4915203
	checksums = {
		'sha256':
			'f2e2d0e322685193d1affec83b21dc05d599e17a7306d7b90de95bb5b9ac622a',
		}

	@classmethod
	def getMakeName(cls):
		return 'XML'

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
	version = '8.5.15'
	fileLength = 4536117
	checksums = {
		'sha256':
			'f24eaae461795e6b09bf54c7e9f38def025892da55f26008c16413cfdda2884e',
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
