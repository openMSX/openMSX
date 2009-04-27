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
# Legend of link modes:
# SYS_DYN: link dynamically against system libs
#          this is the default mode; useful for local binaries
# SYS_STA: link statically against system libs
#          this seems pointless to me; not implemented
# 3RD_DYN: link dynamically against libs from non-system dir
#          might be useful; not implemented
# 3RD_STA: link statically against libs from non-system dir
#          this is how we build our redistributable binaries

class Library(object):
	libName = None
	makeName = None
	header = None
	configScriptName = None

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
	def getConfigScript(cls, platform, linkMode): # pylint: disable-msg=W0613
		if cls.configScriptName is None:
			return None
		elif linkMode == 'SYS_DYN':
			return cls.configScriptName
		elif linkMode == '3RD_STA':
			return '$(3RDPARTY_INSTALL_DIR)/bin/%s' % cls.configScriptName
		else:
			raise ValueError(linkMode)

	@classmethod
	def getCompileFlags(cls, platform, linkMode):
		configScript = cls.getConfigScript(platform, linkMode)
		if configScript is None:
			# TODO: We should allow multiple locations where libraries can be
			#       searched for. For example, MacPorts and Fink are neither
			#       systemwide nor our 3rdparty area.
			if cls.isSystemLibrary(platform, linkMode):
				return ''
			else:
				return '-I$(3RDPARTY_INSTALL_DIR)/include'
		else:
			return '`%s --cflags`' % configScript

	@classmethod
	def getLinkFlags(cls, platform, linkMode):
		configScript = cls.getConfigScript(platform, linkMode)
		if configScript is not None:
			libsOption = (
				cls.getDynamicLibsOption(platform)
				if cls.isSystemLibrary(platform, linkMode)
				else cls.getStaticLibsOption(platform)
				)
			if libsOption is not None:
				return '`%s %s`' % (configScript, libsOption)
		if cls.isSystemLibrary(platform, linkMode):
			# TODO: Also include libraries this lib depends on.
			return '-l%s' % cls.libName
		else:
			# TODO: Also include libraries this lib depends on, in the
			#       right order.
			# TODO: "lib" vs "lib64".
			return '$(3RDPARTY_INSTALL_DIR)/lib/lib%s.a' % cls.libName

	@classmethod
	def getResult(cls, platform, linkMode):
		configScript = cls.getConfigScript(platform, linkMode)
		if configScript is None:
			return 'yes'
		else:
			return '`%s --version`' % configScript

class GL(Library):
	libName = 'GL'
	makeName = 'GL'
	# Location of GL headers is not standardised; if one of these matches,
	# we consider the GL headers found.
	header = ( '<gl.h>', '<GL/gl.h>' )

	@classmethod
	def isSystemLibrary(cls, platform, linkMode):
		return True

	@classmethod
	def getCompileFlags(cls, platform, linkMode):
		if platform == 'darwin':
			# TODO: GL_HEADER:=<OpenGL/gl.h> iso GL_CFLAGS is cleaner,
			#       but we have to modify the build before we can use it.
			return '-I/System/Library/Frameworks/OpenGL.framework/Headers'
		else:
			return Library.getCompileFlags(platform, linkMode)

	@classmethod
	def getLinkFlags(cls, platform, linkMode):
		if platform == 'darwin':
			return '-framework OpenGL -lGL ' \
				'-L/System/Library/Frameworks/OpenGL.framework/Libraries'
		elif platform == 'mingw32':
			return '-lopengl32'
		else:
			return Library.getLinkFlags(platform, linkMode)

class GLEW(Library):
	libName = 'GLEW'
	makeName = 'GLEW'
	# The comment for the GL headers applies to GLEW as well.
	header = ( '<glew.h>', '<GL/glew.h>' )

	@classmethod
	def getCompileFlags(cls, platform, linkMode):
		flags = Library.getCompileFlags(platform, linkMode)
		if platform == 'mingw32':
			if cls.isSystemLibrary(platform, linkMode):
				return flags
			else:
				return '%s -DGLEW_STATIC' % flags
		else:
			return flags

	@classmethod
	def getLinkFlags(cls, platform, linkMode):
		flags = Library.getLinkFlags(platform, linkMode)
		if platform == 'mingw32':
			# TODO: An alternative implementation would be to convert libName
			#       into a method.
			if cls.isSystemLibrary(platform, linkMode):
				return '-lglew32'
			else:
				# TODO: Plus GL link flags (see TODO in Library).
				return '$(3RDPARTY_INSTALL_DIR)/lib/libglew32.a'
		else:
			return flags

class JACK(Library):
	libName = 'jack'
	makeName = 'JACK'
	header = '<jack/jack.h>'

class LibPNG(Library):
	libName = 'png12'
	makeName = 'PNG'
	header = '<png.h>'
	configScript = 'libpng-config'

	@classmethod
	def getDynamicLibsOption(cls, platform):
		return '--ldflags'

	@classmethod
	def getCompileFlags(cls, platform, linkMode):
		flags = Library.getCompileFlags(platform, linkMode)
		if cls.isSystemLibrary(platform, linkMode):
			return flags
		else:
			# Note: The additional -I is to pick up the zlib headers when zlib
			#       is not installed systemwide.
			return flags + ' -I$(3RDPARTY_INSTALL_DIR)/include'

class SDL(Library):
	libName = 'SDL'
	makeName = 'SDL'
	header = '<SDL.h>'
	configScript = 'sdl-config'

	@classmethod
	def getLinkFlags(cls, platform, linkMode):
		flags = Library.getLinkFlags(platform, linkMode)
		if platform in ('linux', 'gnu'):
			if cls.isSystemLibrary(platform, linkMode):
				# TODO: Fix sdl-config instead.
				return '%s -ldl' % flags
			else:
				return flags
		elif platform == 'mingw32':
			if cls.isSystemLibrary(platform, linkMode):
				return flags.replace('-mwindows', '-mconsole')
			else:
				return ' '.join(
					'$(3RDPARTY_INSTALL_DIR)/lib/libSDL.a',
					'/mingw/lib/libmingw32.a',
					'$(3RDPARTY_INSTALL_DIR)/lib/libSDLmain.a'
					'-lwinmm',
					'-lgdi32'
					)
		else:
			return flags

class SDL_image(Library):
	libName = 'SDL_image'
	makeName = 'SDL_IMAGE'
	header = '<SDL_image.h>'

	@classmethod
	def getStaticLibsOption(cls, platform):
		if platform == 'darwin':
			return '--static-libs'
		else:
			return Library.getStaticLibsOption(platform)

	@classmethod
	def getCompileFlags(cls, platform, linkMode):
		return SDL.getCompileFlags(platform, linkMode)

class SDL_ttf(Library):
	libName = 'SDL_ttf'
	makeName = 'SDL_TTF'
	header = '<SDL_ttf.h>'

	@classmethod
	def getCompileFlags(cls, platform, linkMode):
		return SDL.getCompileFlags(platform, linkMode)

class TCL(Library):
	libName = 'tcl'
	makeName = 'TCL'
	header = '<tcl.h>'

	@classmethod
	def getConfigScript(cls, platform, linkMode):
		# TODO: Convert (part of) tcl-search.sh to Python as well?
		if cls.isSystemLibrary(platform, linkMode):
			return 'build/tcl-search.sh'
		else:
			return 'TCL_CONFIG_DIR=$(3RDPARTY_INSTALL_DIR)/lib ' \
				'build/tcl-search.sh'

	@classmethod
	def getCompileFlags(cls, platform, linkMode):
		configScript = cls.getConfigScript(platform, linkMode)
		return '`%s --cflags`' % configScript

	@classmethod
	def getLinkFlags(cls, platform, linkMode):
		# Note: Tcl can be compiled with a static or a dynamic library, not
		#       both. So whether this returns static or dynamic link flags
		#       depends on how this copy of Tcl was built.
		# TODO: If we are trying to link statically against a a dynamic
		#       build of Tcl (or vice versa), consider it an error.
		return '`%s %s`' % (
			cls.getConfigScript(platform, linkMode),
			'--ldflags' if cls.isSystemLibrary(platform, linkMode)
				else '--static-libs'
			)

	@classmethod
	def getResult(cls, platform, linkMode):
		configScript = cls.getConfigScript(platform, linkMode)
		return '`%s --version`' % configScript

class LibXML2(Library):
	libName = 'xml2'
	makeName = 'XML'
	header = '<libxml/parser.h>'
	configScript = 'xml2-config'

	@classmethod
	def getConfigScript(cls, platform, linkMode):
		if platform == 'darwin':
			# Use xml2-config from /usr: ideally we would use xml2-config from
			# the SDK, but the SDK doesn't contain that file. The -isysroot
			# compiler argument makes sure the headers are taken from the SDK
			# though.
			return '/usr/bin/%s' % cls.configScript
		else:
			return Library.getConfigScript(platform, linkMode)

	@classmethod
	def getCompileFlags(cls, platform, linkMode):
		flags = Library.getCompileFlags(platform, linkMode)
		if cls.isSystemLibrary(platform, linkMode):
			return flags
		else:
			return flags + ' -DLIBXML_STATIC'

class ZLib(Library):
	libName = 'z'
	makeName = 'ZLIB'
	header = '<zlib.h>'

# Build a dictionary of libraries using introspection.
def _discoverLibraries(localObjects):
	for obj in localObjects:
		if isinstance(obj, type) and issubclass(obj, Library):
			if not (obj is Library):
				yield obj.makeName, obj
librariesByName = dict(_discoverLibraries(locals().itervalues()))
