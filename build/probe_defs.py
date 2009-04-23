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
	header = None
	configScriptName = None
	dynamicLibsOption = '--libs'
	staticLibsOption = None

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
				cls.dynamicLibsOption
				if cls.isSystemLibrary(platform, linkMode)
				else cls.staticLibsOption
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
	# Location of GL headers is not standardised; if one of these matches,
	# we consider the GL headers found.
	header = ( '<gl.h>', '<GL/gl.h>' )

	@classmethod
	def isSystemLibrary(cls, platform, linkMode):
		return True

class GLEW(Library):
	libName = 'GLEW'
	# The comment for the GL headers applies to GLEW as well.
	header = ( '<glew.h>', '<GL/glew.h>' )

class JACK(Library):
	libName = 'jack'
	header = '<jack/jack.h>'

class LibPNG(Library):
	libName = 'png12'
	header = '<png.h>'
	configScript = 'libpng-config'
	dynamicLibsOption = '--ldflags'

	@classmethod
	def getCompileFlags(cls, platform, linkMode):
		flags = Library.getCompileFlags(cls, platform, linkMode)
		if cls.isSystemLibrary(platform, linkMode):
			return flags
		else:
			# Note: The additional -I is to pick up the zlib headers when zlib
			#       is not installed systemwide.
			return flags + ' -I$(3RDPARTY_INSTALL_DIR)/include'

class SDL(Library):
	libName = 'SDL'
	header = '<SDL.h>'
	configScript = 'sdl-config'

class SDL_image(Library):
	libName = 'SDL_image'
	header = '<SDL_image.h>'

	@classmethod
	def getCompileFlags(cls, platform, linkMode):
		return SDL.getCompileFlags(platform, linkMode)

class SDL_ttf(Library):
	libName = 'SDL_ttf'
	header = '<SDL_ttf.h>'

	@classmethod
	def getCompileFlags(cls, platform, linkMode):
		return SDL.getCompileFlags(platform, linkMode)

class TCL(Library):
	libName = 'tcl'
	header = '<tcl.h>'

	@classmethod
	def getConfigScript(cls, platform, linkMode):
		# TODO: Convert (part of) tcl-search.sh to Python as well?
		if linkMode == 'SYS_DYN':
			return 'build/tcl-search.sh'
		elif linkMode == '3RD_STA':
			return 'TCL_CONFIG_DIR=$(3RDPARTY_INSTALL_DIR)/lib ' \
				'build/tcl-search.sh'
		else:
			raise ValueError(linkMode)

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
	header = '<libxml/parser.h>'
	configScript = 'xml2-config'

	@classmethod
	def getCompileFlags(cls, platform, linkMode):
		flags = Library.getCompileFlags(cls, platform, linkMode)
		if cls.isSystemLibrary(platform, linkMode):
			return flags
		else:
			return flags + ' -DLIBXML_STATIC'

class ZLib(Library):
	libName = 'z'
	header = '<zlib.h>'
