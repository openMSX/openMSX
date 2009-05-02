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
	dependsOn = ()

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

	@classmethod
	def getTarballName(cls):
		return '%s%s_mgw.tar.gz' % (cls.sourceName, cls.version)

class FreeType(DownloadablePackage):
	downloadURL = 'http://downloads.sourceforge.net/freetype'
	niceName = 'FreeType'
	sourceName = 'freetype'
	version = '2.3.7'
	dependsOn = ('ZLIB', )

class GLEW(DownloadablePackage):
	downloadURL = 'http://downloads.sourceforge.net/glew'
	niceName = 'GLEW'
	sourceName = 'glew'
	version = '1.5.1'
	dependsOn = ('GL', )

	@classmethod
	def getTarballName(cls):
		return '%s-%s-src.tgz' % (cls.sourceName, cls.version)

class JACK(DownloadablePackage):
	downloadURL = 'http://jackaudio.org/downloads/'
	niceName = 'Jack'
	sourceName = 'jack-audio-connection-kit'
	version = '0.116.2'

	@classmethod
	def getMakeName(cls):
		return 'JACK'

class LibPNG(DownloadablePackage):
	downloadURL = 'http://downloads.sourceforge.net/libpng'
	niceName = 'libpng'
	sourceName = 'libpng'
	version = '1.2.34'
	dependsOn = ('ZLIB', )

	@classmethod
	def getMakeName(cls):
		return 'PNG'

class LibXML2(DownloadablePackage):
	downloadURL = 'http://xmlsoft.org/sources'
	niceName = 'libxml2'
	sourceName = 'libxml2'
	version = '2.7.2'
	dependsOn = ('ZLIB', )

	@classmethod
	def getMakeName(cls):
		return 'XML'

class OpenGL(Package):
	niceName = 'OpenGL'
	sourceName = 'gl'

class SDL(DownloadablePackage):
	downloadURL = 'http://www.libsdl.org/release'
	niceName = 'SDL'
	sourceName = 'SDL'
	version = '1.2.13'

class SDL_image(DownloadablePackage):
	downloadURL = 'http://www.libsdl.org/projects/SDL_image/release'
	niceName = 'SDL_image'
	sourceName = 'SDL_image'
	version = '1.2.7'
	dependsOn = ('SDL', 'PNG')

class SDL_ttf(DownloadablePackage):
	downloadURL = 'http://www.libsdl.org/projects/SDL_ttf/release'
	niceName = 'SDL_ttf'
	sourceName = 'SDL_ttf'
	version = '2.0.9'
	dependsOn = ('SDL', 'FREETYPE')

class TCL(DownloadablePackage):
	downloadURL = 'http://downloads.sourceforge.net/tcl'
	niceName = 'Tcl'
	sourceName = 'tcl'
	version = '8.5.6'

	@classmethod
	def getTarballName(cls):
		return '%s%s-src.tar.gz' % (cls.sourceName, cls.version)

class ZLib(DownloadablePackage):
	downloadURL = 'http://downloads.sourceforge.net/libpng'
	niceName = 'zlib'
	sourceName = 'zlib'
	version = '1.2.3'

# Build a dictionary of packages using introspection.
def _discoverPackages(localObjects):
	for obj in localObjects:
		if isinstance(obj, type) and issubclass(obj, Package):
			if not (obj is Package or obj is DownloadablePackage):
				yield obj.getMakeName(), obj
_packagesByName = dict(_discoverPackages(locals().itervalues()))

def getPackage(makeName):
	return _packagesByName[makeName]
