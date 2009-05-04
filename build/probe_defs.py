# $Id$
#
# Some notes about static linking:
# There are two ways of linking to static library: using the -l command line
# option or specifying the full path to the library file as one of the inputs.
# When using the -l option, the library search paths will be searched for a
# dynamic version of the library, if that is not found, the search paths will
# be searched for a static version of the library. This means we cannot force
# static linking of a library this way. It is possible to force static linking
# of all libraries, but we want to control it per library.
# Conclusion: We have to specify the full path to each library that should be
#             linked statically.

class SystemFunction(object):
	name = None

	@classmethod
	def getFunctionName(cls):
		return cls.name

	@classmethod
	def getMakeName(cls):
		return cls.name.upper()

	@classmethod
	def iterHeaders(cls, targetPlatform):
		raise NotImplementedError

class FTruncateFunction(SystemFunction):
	name = 'ftruncate'

	@classmethod
	def iterHeaders(cls, targetPlatform):
		yield '<unistd.h>'

class GetTimeOfDayFunction(SystemFunction):
	name = 'gettimeofday'

	@classmethod
	def iterHeaders(cls, targetPlatform):
		yield '<sys/time.h>'

class MMapFunction(SystemFunction):
	name = 'mmap'

	@classmethod
	def iterHeaders(cls, targetPlatform):
		if targetPlatform in ('darwin', 'openbsd'):
			yield '<sys/types.h>'
		yield '<sys/mman.h>'

class PosixMemAlignFunction(SystemFunction):
	name = 'posix_memalign'

	@classmethod
	def iterHeaders(cls, targetPlatform):
		yield '<stdlib.h>'

class USleepFunction(SystemFunction):
	name = 'usleep'

	@classmethod
	def iterHeaders(cls, targetPlatform):
		yield '<unistd.h>'

# Build a list of system functions using introspection.
def _discoverSystemFunctions(localObjects):
	for obj in localObjects:
		if isinstance(obj, type) and issubclass(obj, SystemFunction):
			if obj is not SystemFunction:
				yield obj
systemFunctions = list(_discoverSystemFunctions(locals().itervalues()))

class Library(object):
	libName = None
	makeName = None
	header = None
	configScriptName = None
	dynamicLibsOption = '--libs'
	staticLibsOption = None
	function = None
	dependsOn = ()

	@classmethod
	def isSystemLibrary(cls, platform): # pylint: disable-msg=W0613
		'''Returns True iff this library is a system library on the given
		platform.
		A system library is a library that is available systemwide in the
		minimal installation of the OS.
		The default implementation returns False.
		'''
		return False

	@classmethod
	def getConfigScript( # pylint: disable-msg=W0613
		cls, platform, linkStatic, distroRoot
		):
		scriptName = cls.configScriptName
		if scriptName is None:
			return None
		elif distroRoot is None:
			return scriptName
		else:
			return '%s/bin/%s' % (distroRoot, scriptName)

	@classmethod
	def getHeader(cls, platform): # pylint: disable-msg=W0613
		return cls.header

	@classmethod
	def getLibName(cls, platform): # pylint: disable-msg=W0613
		return cls.libName

	@classmethod
	def getCompileFlags(cls, platform, linkStatic, distroRoot):
		configScript = cls.getConfigScript(platform, linkStatic, distroRoot)
		if configScript is not None:
			flags = [ '`%s --cflags`' % configScript ]
		elif distroRoot is None or cls.isSystemLibrary(platform):
			flags = []
		else:
			flags = [ '-I%s/include' % distroRoot ]
		dependentFlags = [
			librariesByName[name].getCompileFlags(
				platform, linkStatic, distroRoot
				)
			for name in cls.dependsOn
			]
		return ' '.join(flags + dependentFlags)

	@classmethod
	def getLinkFlags(cls, platform, linkStatic, distroRoot):
		configScript = cls.getConfigScript(platform, linkStatic, distroRoot)
		if configScript is not None:
			libsOption = (
				cls.dynamicLibsOption
				if not linkStatic or cls.isSystemLibrary(platform)
				else cls.staticLibsOption
				)
			if libsOption is not None:
				return '`%s %s`' % (configScript, libsOption)
		if distroRoot is None or cls.isSystemLibrary(platform):
			return '-l%s' % cls.getLibName(platform)
		else:
			flags = [
				'%s/lib/lib%s.a' % (distroRoot, cls.getLibName(platform))
				] if linkStatic else [
				'-L%s/lib -l%s' % (distroRoot, cls.getLibName(platform))
				]
			dependentFlags = [
				librariesByName[name].getLinkFlags(
					platform, linkStatic, distroRoot
					)
				for name in cls.dependsOn
				]
			return ' '.join(flags + dependentFlags)

	@classmethod
	def getResult(cls, platform, linkStatic, distroRoot):
		configScript = cls.getConfigScript(platform, linkStatic, distroRoot)
		if configScript is None:
			return 'yes'
		else:
			return '`%s --version`' % configScript

class FreeType(Library):
	libName = 'freetype'
	makeName = 'FREETYPE'
	dependsOn = ('ZLIB', )

class GL(Library):
	libName = 'GL'
	makeName = 'GL'
	function = 'glGenTextures'

	@classmethod
	def isSystemLibrary(cls, platform):
		return True

	@classmethod
	def getHeader(cls, platform):
		if platform == 'darwin':
			return '<OpenGL/gl.h>'
		else:
			return '<GL/gl.h>'

	@classmethod
	def getCompileFlags(cls, platform, linkStatic, distroRoot):
		if platform in ('netbsd', 'openbsd'):
			return '-I/usr/X11R6/include -I/usr/X11R7/include'
		else:
			return super(GL, cls).getCompileFlags(
				platform, linkStatic, distroRoot
				)

	@classmethod
	def getLinkFlags(cls, platform, linkStatic, distroRoot):
		if platform == 'darwin':
			return '-framework OpenGL'
		elif platform == 'mingw32':
			return '-lopengl32'
		elif platform in ('netbsd', 'openbsd'):
			return '-L/usr/X11R6/lib -L/usr/X11R7/lib -lGL'
		else:
			return super(GL, cls).getLinkFlags(platform, linkStatic, distroRoot)

class GLEW(Library):
	makeName = 'GLEW'
	header = '<GL/glew.h>'
	function = 'glewInit'
	dependsOn = ('GL', )

	@classmethod
	def getLibName(cls, platform):
		if platform == 'mingw32':
			return 'glew32'
		else:
			return 'GLEW'

	@classmethod
	def getCompileFlags(cls, platform, linkStatic, distroRoot):
		flags = super(GLEW, cls).getCompileFlags(
			platform, linkStatic, distroRoot
			)
		if platform == 'mingw32' and linkStatic:
			return '%s -DGLEW_STATIC' % flags
		else:
			return flags

class JACK(Library):
	libName = 'jack'
	makeName = 'JACK'
	header = '<jack/jack.h>'
	function = 'jack_client_new'

class LibPNG(Library):
	libName = 'png12'
	makeName = 'PNG'
	header = '<png.h>'
	configScriptName = 'libpng-config'
	dynamicLibsOption = '--ldflags'
	function = 'png_write_image'
	dependsOn = ('ZLIB', )

class LibXML2(Library):
	libName = 'xml2'
	makeName = 'XML'
	header = '<libxml/parser.h>'
	configScriptName = 'xml2-config'
	function = 'xmlSAXUserParseFile'
	dependsOn = ('ZLIB', )

	@classmethod
	def getConfigScript(cls, platform, linkStatic, distroRoot):
		if platform == 'darwin':
			# Use xml2-config from /usr: ideally we would use xml2-config from
			# the SDK, but the SDK doesn't contain that file. The -isysroot
			# compiler argument makes sure the headers are taken from the SDK
			# though.
			return '/usr/bin/%s' % cls.configScriptName
		else:
			return super(LibXML2, cls).getConfigScript(
				platform, linkStatic, distroRoot
				)

	@classmethod
	def getCompileFlags(cls, platform, linkStatic, distroRoot):
		flags = super(LibXML2, cls).getCompileFlags(
			platform, linkStatic, distroRoot
			)
		if not linkStatic or cls.isSystemLibrary(platform):
			return flags
		else:
			return flags + ' -DLIBXML_STATIC'

class SDL(Library):
	libName = 'SDL'
	makeName = 'SDL'
	header = '<SDL.h>'
	configScriptName = 'sdl-config'
	staticLibsOption = '--static-libs'
	function = 'SDL_Init'

class SDL_image(Library):
	libName = 'SDL_image'
	makeName = 'SDL_IMAGE'
	header = '<SDL_image.h>'
	function = 'IMG_LoadPNG_RW'
	dependsOn = ('SDL', 'PNG')

class SDL_ttf(Library):
	libName = 'SDL_ttf'
	makeName = 'SDL_TTF'
	header = '<SDL_ttf.h>'
	function = 'TTF_OpenFont'
	dependsOn = ('SDL', 'FREETYPE')

class TCL(Library):
	libName = 'tcl'
	makeName = 'TCL'
	header = '<tcl.h>'
	function = 'Tcl_CreateInterp'
	staticLibsOption = '--static-libs'

	@classmethod
	def getConfigScript(cls, platform, linkStatic, distroRoot):
		if distroRoot is None or cls.isSystemLibrary(platform):
			return 'build/tcl-search.sh'
		else:
			return 'TCL_CONFIG_DIR=%s/lib build/tcl-search.sh' % distroRoot

class ZLib(Library):
	libName = 'z'
	makeName = 'ZLIB'
	header = '<zlib.h>'
	function = 'inflate'

# Build a dictionary of libraries using introspection.
def _discoverLibraries(localObjects):
	for obj in localObjects:
		if isinstance(obj, type) and issubclass(obj, Library):
			if not (obj is Library):
				yield obj.makeName, obj
librariesByName = dict(_discoverLibraries(locals().itervalues()))
