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
	dependsOn = ('SDL3', 'SDL3_TTF', 'FREETYPE', 'PNG', 'TCL', 'ZLIB')

class GLRenderer(Component):
	niceName = 'GL renderer'
	makeName = 'GL'
	dependsOn = ('GL', 'GLEW')

class Laserdisc(Component):
	niceName = 'Laserdisc'
	makeName = 'LASERDISC'
	dependsOn = ('OGG', 'VORBIS', 'THEORA')

class ALSAMIDI(Component):
	niceName = 'ALSA MIDI'
	makeName = 'ALSAMIDI'
	dependsOn = ('ALSA', )

def iterComponents():
	'''Iterates through all components of openMSX.
	'''
	yield EmulationCore
	yield GLRenderer
	yield Laserdisc
	yield ALSAMIDI

def iterBuildableComponents(probeVars):
	'''Iterates through those components of openMSX that can be built
	on the probed system.
	'''
	for component in iterComponents():
		if component.canBuild(probeVars):
			yield component

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
