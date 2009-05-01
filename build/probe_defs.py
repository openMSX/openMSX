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
#
# TODO: How to probe for a static library?
#       Currently we link an empty test program to it. Since the test program
#       does not use any symbols from the library, the test can only fail if
#       the ".a" file does not exist.
#       We could test the existence of the ".a" file from Python, making the
#       test more efficient.
#       Or we could try to trigger a representative subset of the symbols in
#       the library, so missing dependencies of this library would be caught.
#
# Legend of link modes:
# SYS_DYN: link dynamically against system libs
#          this is the default mode; useful for local binaries
# SYS_STA: link statically against system libs
#          this seems pointless to me; not implemented
# 3RD_DYN: link dynamically against libs from non-system dir
#          might be useful; not implemented
# 3RD_STA: link statically against libs from non-system dir
#          this is how we build our redistributable binaries

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
def iterSystemFunctions(localObjects):
	for obj in localObjects:
		if isinstance(obj, type) and issubclass(obj, SystemFunction):
			if obj is not SystemFunction:
				yield obj
systemFunctions = list(iterSystemFunctions(locals().itervalues()))

class Library(object):
	libName = None
	makeName = None
	header = None
	configScriptName = None
	function = None

	@classmethod
	def getDynamicLibsOption(cls, platform): # pylint: disable-msg=W0613
		return '--libs'

	@classmethod
	def getStaticLibsOption(cls, platform): # pylint: disable-msg=W0613
		return None

	@classmethod
	def isSystemLibrary(cls, platform, linkMode): # pylint: disable-msg=W0613
		if linkMode == 'SYS_DYN':
			return True
		elif linkMode == '3RD_STA':
			return False
		else:
			raise ValueError(linkMode)

	@classmethod
	def getConfigScript( # pylint: disable-msg=W0613
		cls, platform, linkMode, distroRoot
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
	def getCompileFlags(cls, platform, linkMode, distroRoot):
		configScript = cls.getConfigScript(platform, linkMode, distroRoot)
		if configScript is None:
			# TODO: We should allow multiple locations where libraries can be
			#       searched for. For example, MacPorts and Fink are neither
			#       systemwide nor our 3rdparty area.
			if cls.isSystemLibrary(platform, linkMode):
				return ''
			elif distroRoot is None:
				raise ValueError(
					# TODO: Use nice name instead of lib name.
					'Library "%s" is not a system library and no alternative '
					'location is available.' % cls.libName
					)
			else:
				return '-I%s/include' % distroRoot
		else:
			return '`%s --cflags`' % configScript

	@classmethod
	def getLinkFlags(cls, platform, linkMode, distroRoot):
		configScript = cls.getConfigScript(platform, linkMode, distroRoot)
		if configScript is not None:
			libsOption = (
				cls.getDynamicLibsOption(platform)
				if cls.isSystemLibrary(platform, linkMode)
				else cls.getStaticLibsOption(platform)
				)
			if libsOption is not None:
				return '`%s %s`' % (configScript, libsOption)
		if cls.isSystemLibrary(platform, linkMode):
			return '-l%s' % cls.libName
		elif distroRoot is None:
			raise ValueError(
				# TODO: Use nice name instead of lib name.
				'Library "%s" is not a system library and no alternative '
				'location is available.' % cls.libName
				)
		else:
			# TODO: "lib" vs "lib64".
			if linkMode == 'SYS_DYN':
				return '-L%s/lib -l%s' % (distroRoot, cls.libName)
			elif linkMode == '3RD_STA':
				# TODO: Also include libraries this lib depends on, in the
				#       right order.
				return '%s/lib/lib%s.a' % (distroRoot, cls.libName)
			else:
				raise ValueError('Invalid link mode "%s"' % linkMode)

	@classmethod
	def getResult(cls, platform, linkMode, distroRoot):
		configScript = cls.getConfigScript(platform, linkMode, distroRoot)
		if configScript is None:
			return 'yes'
		else:
			return '`%s --version`' % configScript

class GL(Library):
	libName = 'GL'
	makeName = 'GL'
	function = 'glGenTextures'

	@classmethod
	def isSystemLibrary(cls, platform, linkMode):
		return True

	@classmethod
	def getHeader(cls, platform): # pylint: disable-msg=W0613
		if platform == 'darwin':
			return '<OpenGL/gl.h>'
		else:
			return '<GL/gl.h>'

	@classmethod
	def getCompileFlags(cls, platform, linkMode, distroRoot):
		if platform == 'darwin':
			return '-framework OpenGL'
		else:
			return super(GL, cls).getCompileFlags(
				platform, linkMode, distroRoot
				)

	@classmethod
	def getLinkFlags(cls, platform, linkMode, distroRoot):
		if platform == 'darwin':
			return '-framework OpenGL -lGL ' \
				'-L/System/Library/Frameworks/OpenGL.framework/Libraries'
		elif platform == 'mingw32':
			return '-lopengl32'
		else:
			return super(GL, cls).getLinkFlags(platform, linkMode, distroRoot)

class GLEW(Library):
	libName = 'GLEW'
	makeName = 'GLEW'
	header = '<GL/glew.h>'
	function = 'glewInit'

	@classmethod
	def getCompileFlags(cls, platform, linkMode, distroRoot):
		flags = super(GLEW, cls).getCompileFlags(platform, linkMode, distroRoot)
		if platform == 'mingw32':
			if cls.isSystemLibrary(platform, linkMode):
				return flags
			else:
				return '%s -DGLEW_STATIC' % flags
		else:
			return flags

	@classmethod
	def getLinkFlags(cls, platform, linkMode, distroRoot):
		flags = super(GLEW, cls).getLinkFlags(platform, linkMode, distroRoot)
		if platform == 'mingw32':
			# TODO: An alternative implementation would be to convert libName
			#       into a method.
			if cls.isSystemLibrary(platform, linkMode):
				return '-lglew32'
			else:
				# TODO: Plus GL link flags (see TODO in Library).
				return '%s/lib/libglew32.a' % distroRoot
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
	function = 'png_write_image'

	@classmethod
	def getDynamicLibsOption(cls, platform):
		return '--ldflags'

	@classmethod
	def getCompileFlags(cls, platform, linkMode, distroRoot):
		flags = super(LibPNG, cls).getCompileFlags(
			platform, linkMode, distroRoot
			)
		if cls.isSystemLibrary(platform, linkMode):
			return flags
		else:
			# Note: The additional -I is to pick up the zlib headers when zlib
			#       is not installed systemwide.
			return flags + ' -I%s/include' % distroRoot

class SDL(Library):
	libName = 'SDL'
	makeName = 'SDL'
	header = '<SDL.h>'
	configScriptName = 'sdl-config'
	function = 'SDL_Init'

	@classmethod
	def getLinkFlags(cls, platform, linkMode, distroRoot):
		flags = super(SDL, cls).getLinkFlags(platform, linkMode, distroRoot)
		if platform in ('linux', 'gnu'):
			if cls.isSystemLibrary(platform, linkMode):
				return flags
			else:
				# TODO: Fix sdl-config instead.
				return '%s -ldl' % flags
		elif platform == 'mingw32':
			if cls.isSystemLibrary(platform, linkMode):
				return flags.replace('-mwindows', '-mconsole')
			else:
				return ' '.join((
					'%s/lib/libSDL.a' % distroRoot,
					'/mingw/lib/libmingw32.a',
					'%s/lib/libSDLmain.a' % distroRoot,
					'-lwinmm',
					'-lgdi32'
					))
		else:
			return flags

class SDL_image(Library):
	libName = 'SDL_image'
	makeName = 'SDL_IMAGE'
	header = '<SDL_image.h>'
	function = 'IMG_LoadPNG_RW'

	@classmethod
	def getStaticLibsOption(cls, platform):
		if platform == 'darwin':
			return '--static-libs'
		else:
			return super(SDL_image, cls).getStaticLibsOption(platform)

	@classmethod
	def getCompileFlags(cls, platform, linkMode, distroRoot):
		return SDL.getCompileFlags(platform, linkMode, distroRoot)

class SDL_ttf(Library):
	libName = 'SDL_ttf'
	makeName = 'SDL_TTF'
	header = '<SDL_ttf.h>'
	function = 'TTF_OpenFont'

	@classmethod
	def getCompileFlags(cls, platform, linkMode, distroRoot):
		return SDL.getCompileFlags(platform, linkMode, distroRoot)

class TCL(Library):
	libName = 'tcl'
	makeName = 'TCL'
	header = '<tcl.h>'
	function = 'Tcl_CreateInterp'

	@classmethod
	def getConfigScript(cls, platform, linkMode, distroRoot):
		# TODO: Convert (part of) tcl-search.sh to Python as well?
		if cls.isSystemLibrary(platform, linkMode):
			return 'build/tcl-search.sh'
		else:
			return 'TCL_CONFIG_DIR=%s/lib build/tcl-search.sh' % distroRoot

	@classmethod
	def getCompileFlags(cls, platform, linkMode, distroRoot):
		configScript = cls.getConfigScript(platform, linkMode, distroRoot)
		return '`%s --cflags`' % configScript

	@classmethod
	def getLinkFlags(cls, platform, linkMode, distroRoot):
		# Note: Tcl can be compiled with a static or a dynamic library, not
		#       both. So whether this returns static or dynamic link flags
		#       depends on how this copy of Tcl was built.
		# TODO: If we are trying to link statically against a a dynamic
		#       build of Tcl (or vice versa), consider it an error.
		return '`%s %s`' % (
			cls.getConfigScript(platform, linkMode, distroRoot),
			'--ldflags' if cls.isSystemLibrary(platform, linkMode)
				else '--static-libs'
			)

class LibXML2(Library):
	libName = 'xml2'
	makeName = 'XML'
	header = '<libxml/parser.h>'
	configScriptName = 'xml2-config'
	function = 'xmlSAXUserParseFile'

	@classmethod
	def getConfigScript(cls, platform, linkMode, distroRoot):
		if platform == 'darwin':
			# Use xml2-config from /usr: ideally we would use xml2-config from
			# the SDK, but the SDK doesn't contain that file. The -isysroot
			# compiler argument makes sure the headers are taken from the SDK
			# though.
			return '/usr/bin/%s' % cls.configScriptName
		else:
			return super(LibXML2, cls).getConfigScript(
				platform, linkMode, distroRoot
				)

	@classmethod
	def getCompileFlags(cls, platform, linkMode, distroRoot):
		flags = super(LibXML2, cls).getCompileFlags(
			platform, linkMode, distroRoot
			)
		if cls.isSystemLibrary(platform, linkMode):
			return flags
		else:
			return flags + ' -DLIBXML_STATIC'

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
