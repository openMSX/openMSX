# $Id$

from urlparse import urljoin

class Package(object):
	'''Abstract base class for packages.
	'''
	name = None

	@classmethod
	def getMakeName(cls):
		return cls.name.upper()

class DownloadablePackage(Package):
	'''Abstract base class for packages that can be downloaded.
	'''
	downloadURL = None
	version = None
	dependsOn = ()

	@classmethod
	def getTarballName(cls):
		return '%s-%s.tar.gz' % (cls.name, cls.version)

	@classmethod
	def getURL(cls):
		return urljoin(cls.downloadURL + '/', cls.getTarballName())

# The class names reflect package names, which do not necessarily follow our
# naming convention.
# pylint: disable-msg=C0103

class DirectX(DownloadablePackage):
	downloadURL = 'http://alleg.sourceforge.net/files'
	name = 'dx'
	version = '70'

	@classmethod
	def getTarballName(cls):
		return '%s%s_mgw.tar.gz' % (cls.name, cls.version)

class FreeType(DownloadablePackage):
	downloadURL = 'http://nongnu.askapache.com/freetype'
	name = 'freetype'
	version = '2.3.7'
	dependsOn = ('zlib', )

class GLEW(DownloadablePackage):
	downloadURL = 'http://downloads.sourceforge.net/glew'
	name = 'glew'
	version = '1.5.1'
	dependsOn = ('gl', )

	@classmethod
	def getTarballName(cls):
		return '%s-%s-src.tgz' % (cls.name, cls.version)

class LibPNG(DownloadablePackage):
	downloadURL = 'http://downloads.sourceforge.net/libpng'
	name = 'libpng'
	version = '1.2.34'
	dependsOn = ('zlib', )

	@classmethod
	def getMakeName(cls):
		return 'PNG'

class LibXML2(DownloadablePackage):
	downloadURL = 'http://xmlsoft.org/sources'
	name = 'libxml2'
	version = '2.7.2'
	dependsOn = ('zlib', )

	@classmethod
	def getMakeName(cls):
		return 'XML'

class OpenGL(Package):
	name = 'gl'

class SDL(DownloadablePackage):
	downloadURL = 'http://www.libsdl.org/release'
	name = 'SDL'
	version = '1.2.13'

class SDL_image(DownloadablePackage):
	downloadURL = 'http://www.libsdl.org/projects/SDL_image/release'
	name = 'SDL_image'
	version = '1.2.7'
	dependsOn = ('SDL', 'libpng')

class SDL_ttf(DownloadablePackage):
	downloadURL = 'http://www.libsdl.org/projects/SDL_ttf/release'
	name = 'SDL_ttf'
	version = '2.0.9'
	dependsOn = ('SDL', 'freetype')

class TCL(DownloadablePackage):
	downloadURL = 'http://downloads.sourceforge.net/tcl'
	name = 'tcl'
	version = '8.5.6'

	@classmethod
	def getTarballName(cls):
		return '%s%s-src.tar.gz' % (cls.name, cls.version)

class ZLib(DownloadablePackage):
	downloadURL = 'http://downloads.sourceforge.net/libpng'
	name = 'zlib'
	version = '1.2.3'

# Build a dictionary of packages using introspection.
def _discoverPackages(localObjects):
	for obj in localObjects:
		if isinstance(obj, type) and issubclass(obj, Package):
			if not (obj is Package or obj is DownloadablePackage):
				yield obj.name, obj
_packagesByName = dict(_discoverPackages(locals().itervalues()))

def getPackage(name):
	return _packagesByName[name]
