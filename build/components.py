# $Id$
# Defines the building blocks of openMSX and their dependencies.

componentNames = ( 'CORE', 'GL', 'JACK' )

# For static linking, it's important that if lib A depends on B, A is in the
# list before B.
# TODO: It would be better if libraries would declare their dependencies and
#       we would compute the link flags order from that.
coreLibs = ( 'SDL_IMAGE', 'SDL_TTF', 'SDL', 'PNG', 'TCL', 'XML', 'ZLIB' )

def checkComponents(probeVars):
	buildCore = all(
		probeVars['HAVE_%s_LIB' % lib] and probeVars['HAVE_%s_H' % lib]
		for lib in coreLibs
		)
	buildGL = all((
		probeVars['HAVE_GL_LIB'],
		probeVars['HAVE_GL_H'] or probeVars['HAVE_GL_GL_H'],
		probeVars['HAVE_GLEW_LIB'],
		probeVars['HAVE_GLEW_H'] or probeVars['HAVE_GL_GLEW_H'],
		))
	buildJack = bool(probeVars['HAVE_JACK_LIB'] and probeVars['HAVE_JACK_H'])
	return buildCore, buildGL, buildJack
