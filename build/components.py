# $Id$
# Defines the building blocks of openMSX and their dependencies.

class Component(object):
	niceName = None
	makeName = None
	dependsOn = None

	@classmethod
	def canBuild(cls, probeVars):
		return all(
			probeVars['HAVE_%s_H' % makeName] and
			probeVars['HAVE_%s_LIB' % makeName]
			for makeName in cls.dependsOn
			)

class EmulationCore(Component):
	niceName = 'Emulation core'
	makeName = 'CORE'
	dependsOn = ('SDL', 'SDL_IMAGE', 'SDL_TTF', 'PNG', 'TCL', 'XML', 'ZLIB')

class GLRenderer(Component):
	niceName = 'GL renderer'
	makeName = 'GL'
	dependsOn = ('GL', 'GLEW')

class CassetteJack(Component):
	niceName = 'CassetteJack'
	makeName = 'JACK'
	dependsOn = ('JACK', )

def iterComponents():
	yield EmulationCore
	yield GLRenderer
	yield CassetteJack
