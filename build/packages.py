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
	version = '2.3.12'
	fileLength = 1885477
	checksums = {
		'sha256':
			'74e4ddbd3a48e8b8701cf3581a13e680b26e717c4f86563018433b18301881a9',
		}

class GLEW(DownloadablePackage):
	downloadURL = 'http://downloads.sourceforge.net/glew'
	niceName = 'GLEW'
	sourceName = 'glew'
	version = '1.5.4'
	fileLength = 459341
	checksums = {
		'sha256':
			'ddb8402453d48b63fd5d230a8b26885fbe29bc887cf8f7d12459fc8ab9ea8e32',
		}

	@classmethod
	def getTarballName(cls):
		return '%s-%s.tgz' % (cls.sourceName, cls.version)

class LibPNG(DownloadablePackage):
	downloadURL = 'http://downloads.sourceforge.net/libpng'
	niceName = 'libpng'
	sourceName = 'libpng'
	version = '1.2.43'
	fileLength = 835683
	checksums = {
		'sha256':
			'4d46ea6ea3f68918b13ef1fd54e99a06f7778c6be3221d25ef038b7c39d0744f',
		}

	@classmethod
	def getMakeName(cls):
		return 'PNG'

class LibXML2(DownloadablePackage):
	downloadURL = 'http://xmlsoft.org/sources'
	niceName = 'libxml2'
	sourceName = 'libxml2'
	version = '2.7.7'
	fileLength = 4868502
	checksums = {
		'sha256':
			'af5b781418ba4fff556fa43c50086658ea8a2f31909c2b625c2ce913a1d9eb68',
		}

	@classmethod
	def getMakeName(cls):
		return 'XML'

class OGG(DownloadablePackage):
	downloadURL = 'http://downloads.xiph.org/releases/ogg'
	niceName = 'libogg'
	sourceName = 'libogg'
	version = '1.2.0'
	fileLength = 400293
	checksums = {
		'sha256':
			'f30d983e238acd94e80ae551327ea2f83cdc330470b4188564bef28fec59eb69',
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
	version = '1.2.14'
	fileLength = 4014154
	checksums = {
		'sha256':
			'5d927e287034cb6bb0ebccfa382cb1d185cb113c8ab5115a0759798642eed9b6',
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
	version = '8.5.8'
	fileLength = 4452127
	checksums = {
		'sha256':
			'6b090c1024038d0381e1ccfbd6d5c0f0e6ef205269ceb9d28bd7bd7ac5bbf4a7',
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
	version = '1.3.1'
	fileLength = 1467164
	checksums = {
		'sha256':
			'951f462ba732a76f51002590d56437ee45362f4353d8bdbeb01231a54a2da0f1',
		}

	@classmethod
	def getMakeName(cls):
		return 'VORBIS'

class ZLib(DownloadablePackage):
	downloadURL = 'http://downloads.sourceforge.net/libpng'
	niceName = 'zlib'
	sourceName = 'zlib'
	version = '1.2.5'
	fileLength = 544640
	checksums = {
		'sha256':
			'6064e52e513facb0fbb7998c6413406cf253cfb986063d68f4771c2bf7a3f958',
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
