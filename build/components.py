# $Id$
# Defines the building blocks of openMSX and their dependencies.

class Component(object):
	niceName = None
	makeName = None
	dependsOn = None

	@classmethod
	def canBuild(cls, probeVars):
		return all(
			probeVars.get('HAVE_%s_H' % makeName) and
			probeVars.get('HAVE_%s_LIB' % makeName)
			for makeName in cls.dependsOn
			)

class EmulationCore(Component):
	niceName = 'Emulation core'
	makeName = 'CORE'
	dependsOn = ('SDL', 'SDL_TTF', 'PNG', 'TCL', 'XML', 'ZLIB')

class GLRenderer(Component):
	niceName = 'GL renderer'
	makeName = 'GL'
	dependsOn = ('GL', 'GLEW')

class CassetteJack(Component):
	niceName = 'CassetteJack'
	makeName = 'JACK'
	dependsOn = ('JACK', )

class Laserdisc(Component):
	niceName = 'Laserdisc'
	makeName = 'LASERDISC'
	dependsOn = ('OGGZ', 'VORBIS', 'THEORA')

def iterComponents():
	yield EmulationCore
	yield GLRenderer
	yield CassetteJack
	yield Laserdisc

def requiredLibrariesFor(components):
	'''Compute the library packages required to build the given components.
	Only the direct dependencies from openMSX are included, not dependencies
	between libraries.
	Returns a set of Make names.
	'''
	return set(
		makeName
		for comp in components
		for makeName in comp.dependsOn
		)
