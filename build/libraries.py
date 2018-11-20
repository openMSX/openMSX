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

from executils import captureStdout, shjoin
from msysutils import msysActive, msysPathToNative

from os import listdir
from os.path import isdir, isfile
from os import environ

class Library(object):
	libName = None
	makeName = None
	header = None
	configScriptName = None
	dynamicLibsOption = '--libs'
	staticLibsOption = None
	function = None
	# TODO: A library can give an application compile time and run time
	#       dependencies on other libraries. For example SDL_ttf depends on
	#       FreeType only at run time, but depends on SDL both compile time
	#       and run time, since SDL is part of its interface and FreeType is
	#       only used by the implementation. As a result, it is possible to
	#       compile against SDL_ttf without having the FreeType headers
	#       installed. But our getCompileFlags() does not support this.
	#       In pkg-config these are called private dependencies.
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
		elif platform == 'dingux' and cls.isSystemLibrary(platform):
			# TODO: A generic mechanism for locating config scripts in SDKs.
			#       Note that distroRoot is for non-system libs only.
			#       Trying a path relative to the compiler location would
			#       probably work well.
			return '/opt/gcw0-toolchain/usr/mipsel-gcw0-linux-uclibc/sysroot/usr/bin/%s' % scriptName
		elif distroRoot is None:
			return scriptName
		else:
			return '%s/bin/%s' % (distroRoot, scriptName)

	@classmethod
	def getHeaders(cls, platform): # pylint: disable-msg=W0613
		header = cls.header
		return header if hasattr(header, '__iter__') else (header, )

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
	def getVersion(cls, platform, linkStatic, distroRoot):
		'''Returns the version of this library, "unknown" if there is no
		mechanism to retrieve the version, None if there is a mechanism
		to retrieve the version but it failed, or a callable that takes a
		CompileCommand and a log stream as its arguments and returns the
		version or None if retrieval failed.
		'''
		configScript = cls.getConfigScript(platform, linkStatic, distroRoot)
		if configScript is None:
			return 'unknown'
		else:
			return '`%s --version`' % configScript

class ALSA(Library):
	libName = 'asound'
	makeName = 'ALSA'
	header = '<alsa/asoundlib.h>'
	function = 'snd_seq_open'

	@classmethod
	def isSystemLibrary(cls, platform):
		return platform in ('dingux',)

	@classmethod
	def getLinkFlags(cls, platform, linkStatic, distroRoot):
		flags = super(ALSA, cls).getLinkFlags(platform, linkStatic, distroRoot)
		if linkStatic:
			flags += ' -lpthread'
		return flags

	@classmethod
	def getVersion(cls, platform, linkStatic, distroRoot):
		def execute(cmd, log):
			version = cmd.expand(log, cls.getHeaders(platform),
				'SND_LIB_VERSION_STR'
				)
			return None if version is None else version.strip('"')
		return execute

class FreeType(Library):
	libName = 'freetype'
	makeName = 'FREETYPE'
	header = ('<ft2build.h>', 'FT_FREETYPE_H')
	configScriptName = 'freetype-config'
	function = 'FT_Open_Face'

	@classmethod
	def isSystemLibrary(cls, platform):
		return platform in ('dingux',)

	@classmethod
	def getConfigScript(cls, platform, linkStatic, distroRoot):
		if platform in ('netbsd', 'openbsd'):
			if distroRoot == '/usr/local':
				# FreeType is located in the X11 tree, not the ports tree.
				distroRoot = '/usr/X11R6'
		script = super(FreeType, cls).getConfigScript(
			platform, linkStatic, distroRoot
			)
		if isfile(script):
			return script
		else:
			# FreeType 2.9.1 no longer installs the freetype-config script
			# by default and expects pkg-config to be used instead.
			return 'pkg-config freetype2'

	@classmethod
	def getVersion(cls, platform, linkStatic, distroRoot):
		configScript = cls.getConfigScript(platform, linkStatic, distroRoot)
		return '`%s --ftversion`' % configScript

class GL(Library):
	libName = 'GL'
	makeName = 'GL'
	function = 'glGenTextures'

	@classmethod
	def isSystemLibrary(cls, platform):
		# On *BSD, OpenGL is in ports, not in the base system.
		return not platform.endswith('bsd')

	@classmethod
	def getHeaders(cls, platform):
		if platform == 'darwin':
			return ('<OpenGL/gl.h>', )
		else:
			return ('<GL/gl.h>', )

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
		elif platform.startswith('mingw'):
			return '-lopengl32'
		elif platform in ('netbsd', 'openbsd'):
			return '-L/usr/X11R6/lib -L/usr/X11R7/lib -lGL'
		else:
			return super(GL, cls).getLinkFlags(platform, linkStatic, distroRoot)

	@classmethod
	def getVersion(cls, platform, linkStatic, distroRoot):
		def execute(cmd, log):
			versionPairs = tuple(
				( major, minor )
				for major in range(1, 10)
				for minor in range(0, 10)
				)
			version = cmd.expand(log, cls.getHeaders(platform), *(
				'GL_VERSION_%d_%d' % pair for pair in versionPairs
				))
			try:
				return '%d.%d' % max(
					ver
					for ver, exp in zip(versionPairs, version)
					if exp is not None
					)
			except ValueError:
				return None
		return execute

class GLEW(Library):
	makeName = 'GLEW'
	header = '<GL/glew.h>'
	function = 'glewInit'
	dependsOn = ('GL', )

	@classmethod
	def getLibName(cls, platform):
		if platform.startswith('mingw'):
			return 'glew32'
		else:
			return 'GLEW'

	@classmethod
	def getCompileFlags(cls, platform, linkStatic, distroRoot):
		flags = super(GLEW, cls).getCompileFlags(
			platform, linkStatic, distroRoot
			)
		if platform.startswith('mingw') and linkStatic:
			return '%s -DGLEW_STATIC' % flags
		else:
			return flags

class LibPNG(Library):
	libName = 'png16'
	makeName = 'PNG'
	header = '<png.h>'
	configScriptName = 'libpng-config'
	dynamicLibsOption = '--ldflags'
	function = 'png_write_image'
	dependsOn = ('ZLIB', )

	@classmethod
	def isSystemLibrary(cls, platform):
		return platform in ('dingux',)

class OGG(Library):
	libName = 'ogg'
	makeName = 'OGG'
	header = '<ogg/ogg.h>'
	function = 'ogg_stream_init'

	@classmethod
	def isSystemLibrary(cls, platform):
		return platform in ('dingux',)

class SDL(Library):
	libName = 'SDL'
	makeName = 'SDL'
	header = '<SDL.h>'
	configScriptName = 'sdl-config'
	staticLibsOption = '--static-libs'
	function = 'SDL_Init'

	@classmethod
	def isSystemLibrary(cls, platform):
		return platform in ('android', 'dingux')

	@classmethod
	def getConfigScript(cls, platform, linkStatic, distroRoot):
		if platform == 'android':
			return environ['SDL_ANDROID_PORT_PATH'] \
				+ '/project/jni/application/sdl-config'
		else:
			return super(SDL, cls).getConfigScript(
				platform, linkStatic, distroRoot
				)

	@classmethod
	def getLinkFlags(cls, platform, linkStatic, distroRoot):
		flags = super(SDL, cls).getLinkFlags(
			platform, linkStatic, distroRoot
			)
		if platform == 'android':
			# On Android, we explicitly add the commander
			# genius lib folder for SDL
			flags = '-L%s/project/obj/local/%s %s' % (
				environ['SDL_ANDROID_PORT_PATH'],
				environ['TARGET_ABI'],
				flags
				)
		return flags

class SDL_ttf(Library):
	libName = 'SDL_ttf'
	makeName = 'SDL_TTF'
	header = '<SDL_ttf.h>'
	function = 'TTF_OpenFont'
	dependsOn = ('SDL', 'FREETYPE')

	@classmethod
	def isSystemLibrary(cls, platform):
		return platform in ('dingux',)

	@classmethod
	def getLinkFlags(cls, platform, linkStatic, distroRoot):
		flags = super(SDL_ttf, cls).getLinkFlags(
			platform, linkStatic, distroRoot
			)
		if not linkStatic:
			# Because of the SDLmain trickery, we need SDL's link flags too
			# on some platforms even though we're linking dynamically.
			flags += ' ' + SDL.getLinkFlags(platform, linkStatic, distroRoot)
		return flags

	@classmethod
	def getCompileFlags(cls, platform, linkStatic, distroRoot):
		flags = super(SDL_ttf, cls).getCompileFlags(
				platform, linkStatic, distroRoot
				)
		if platform == 'android':
			# On Android, we don't share an install location with SDL,
			# so SDL's compile flags are insufficient.
			flags += ' -I%s/include/SDL' % distroRoot
		return flags

	@classmethod
	def getVersion(cls, platform, linkStatic, distroRoot):
		def execute(cmd, log):
			version = cmd.expand(log, cls.getHeaders(platform),
				'SDL_TTF_MAJOR_VERSION',
				'SDL_TTF_MINOR_VERSION',
				'SDL_TTF_PATCHLEVEL',
				)
			return None if None in version else '%s.%s.%s' % version
		return execute

class TCL(Library):
	libName = 'tcl'
	makeName = 'TCL'
	header = '<tcl.h>'
	function = 'Tcl_CreateInterp'

	@classmethod
	def getTclConfig(cls, platform, distroRoot):
		'''Tcl has a config script that is unlike the typical lib-config script.
		Information is gathered by sourcing the config script, instead of
		executing it and capturing the queried value from stdout. This script
		is located in a library directory, not in a directory in the PATH.
		Also, it does not have the executable bit set.
		This method returns the location of the Tcl config script, or None if
		it could not be found.
		'''
		if hasattr(cls, 'tclConfig'):
			# Return cached value.
			return cls.tclConfig

		def iterLocations():
			# Allow the user to specify the location we should search first,
			# by setting an environment variable.
			tclpath = environ.get('TCL_CONFIG')
			if tclpath is not None:
				yield tclpath

			if distroRoot is None or cls.isSystemLibrary(platform):
				if msysActive():
					roots = (msysPathToNative('/mingw32'), )
				else:
					roots = ('/usr/local', '/usr')
			else:
				roots = (distroRoot, )
			for root in roots:
				if isdir(root):
					for libdir in ('lib', 'lib64', 'lib/tcl'):
						libpath = root + '/' + libdir
						if isdir(libpath):
							yield libpath
							for entry in listdir(libpath):
								if entry.startswith('tcl8.'):
									tclpath = libpath + '/' + entry
									if isdir(tclpath):
										yield tclpath

		tclConfigs = {}
		log = open('derived/tcl-search.log', 'w')
		print >> log, 'Looking for Tcl...'
		try:
			for location in iterLocations():
				path = location + '/tclConfig.sh'
				if isfile(path):
					print >> log, 'Config script:', path
					text = captureStdout(
						log,
						"sh -c '. %s && echo %s'" % (
							path, '$TCL_MAJOR_VERSION $TCL_MINOR_VERSION'
							)
						)
					if text is not None:
						try:
							# pylint: disable-msg=E1103
							major, minor = text.split()
							version = int(major), int(minor)
						except ValueError:
							pass
						else:
							print >> log, 'Found: version %d.%d' % version
							tclConfigs[path] = version
			try:
				# Minimum required version is 8.5.
				# Pick the oldest possible version to minimize the risk of
				# running into incompatible changes.
				tclConfig = min(
					( version, path )
					for path, version in tclConfigs.iteritems()
					if version >= (8, 5)
					)[1]
			except ValueError:
				tclConfig = None
				print >> log, 'No suitable versions found.'
			else:
				print >> log, 'Selected:', tclConfig
		finally:
			log.close()

		cls.tclConfig = tclConfig
		return tclConfig

	@classmethod
	def evalTclConfigExpr(cls, platform, distroRoot, expr, description):
		tclConfig = cls.getTclConfig(platform, distroRoot)
		if tclConfig is None:
			return None
		log = open('derived/tcl-search.log', 'a')
		try:
			print >> log, 'Getting Tcl %s...' % description
			text = captureStdout(
				log,
				shjoin([
					'sh', '-c',
						'. %s && eval "echo \\"%s\\""' % (tclConfig, expr)
					])
				)
			if text is not None:
				print >> log, 'Result: %s' % text.strip()
		finally:
			log.close()
		return None if text is None else text.strip()

	@classmethod
	def getCompileFlags(cls, platform, linkStatic, distroRoot):
		wantShared = not linkStatic or cls.isSystemLibrary(platform)
		# The -DSTATIC_BUILD is a hack to avoid including the complete
		# TCL_DEFS (see 9f1dbddda2) but still being able to link on
		# MinGW (tcl.h depends on this being defined properly).
		return cls.evalTclConfigExpr(
			platform,
			distroRoot,
			'${TCL_INCLUDE_SPEC}' + ('' if wantShared else ' -DSTATIC_BUILD'),
			'compile flags'
			)

	@classmethod
	def getLinkFlags(cls, platform, linkStatic, distroRoot):
		# Tcl can be built as a shared or as a static library, but not both.
		# Check whether the library type of Tcl matches the one we want.
		wantShared = not linkStatic or cls.isSystemLibrary(platform)
		tclShared = cls.evalTclConfigExpr(
			platform,
			distroRoot,
			'${TCL_SHARED_BUILD}',
			'library type (shared/static)'
			)
		log = open('derived/tcl-search.log', 'a')
		try:
			if tclShared == '0':
				if wantShared:
					print >> log, (
						'Dynamic linking requested, but Tcl installation has '
						'static library.'
						)
					return None
			elif tclShared == '1':
				if not wantShared:
					print >> log, (
						'Static linking requested, but Tcl installation has '
						'dynamic library.'
						)
					return None
			else:
				print >> log, (
					'Unable to determine whether Tcl installation has '
					'shared or static library.'
					)
				return None
		finally:
			log.close()

		# Now get the link flags.
		if wantShared:
			return cls.evalTclConfigExpr(
				platform,
				distroRoot,
				'${TCL_LIB_SPEC}',
				'dynamic link flags'
				)
		else:
			return cls.evalTclConfigExpr(
				platform,
				distroRoot,
				'${TCL_EXEC_PREFIX}/lib/${TCL_LIB_FILE} ${TCL_LIBS}',
				'static link flags'
				)

	@classmethod
	def getVersion(cls, platform, linkStatic, distroRoot):
		return cls.evalTclConfigExpr(
			platform,
			distroRoot,
			'${TCL_MAJOR_VERSION}.${TCL_MINOR_VERSION}${TCL_PATCH_LEVEL}',
			'version'
			)

class Theora(Library):
	libName = 'theoradec'
	makeName = 'THEORA'
	header = '<theora/theoradec.h>'
	function = 'th_decode_ycbcr_out'
	dependsOn = ('OGG', )

	@classmethod
	def isSystemLibrary(cls, platform):
		return platform in ('dingux',)

class Vorbis(Library):
	libName = 'vorbis'
	makeName = 'VORBIS'
	header = '<vorbis/codec.h>'
	function = 'vorbis_synthesis_pcmout'
	dependsOn = ('OGG', )

	@classmethod
	def isSystemLibrary(cls, platform):
		return platform in ('dingux',)

class ZLib(Library):
	libName = 'z'
	makeName = 'ZLIB'
	header = '<zlib.h>'
	function = 'inflate'

	@classmethod
	def isSystemLibrary(cls, platform):
		return platform in ('dingux',)

	@classmethod
	def getVersion(cls, platform, linkStatic, distroRoot):
		def execute(cmd, log):
			version = cmd.expand(log, cls.getHeaders(platform), 'ZLIB_VERSION')
			return None if version is None else version.strip('"')
		return execute

# Build a dictionary of libraries using introspection.
def _discoverLibraries(localObjects):
	for obj in localObjects:
		if isinstance(obj, type) and issubclass(obj, Library):
			if not (obj is Library):
				yield obj.makeName, obj
librariesByName = dict(_discoverLibraries(locals().itervalues()))

def allDependencies(makeNames):
	'''Compute the set of all directly and indirectly required libraries to
	build and use the given set of libraries.
	Returns the make names of the required libraries.
	'''
	# Compute the reflexive-transitive closure.
	transLibs = set()
	newLibs = set(makeNames)
	while newLibs:
		transLibs.update(newLibs)
		newLibs = set(
			depMakeName
			for makeName in newLibs
			for depMakeName in librariesByName[makeName].dependsOn
			if depMakeName not in transLibs
			)
	return transLibs
