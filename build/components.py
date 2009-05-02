# $Id$
# Defines the building blocks of openMSX and their dependencies.

from packages import getPackage

class Component(object):
	niceName = None
	makeName = None
	dependsOn = None

	@classmethod
	def canBuild(cls, probeVars):
		return all(
			probeVars['HAVE_%s_H' % makeName] and
			probeVars['HAVE_%s_LIB' % makeName]
			for makeName in (
				getPackage(packageName).getMakeName()
				for packageName in cls.dependsOn
				)
			)

class EmulationCore(Component):
	niceName = 'Emulation core'
	makeName = 'CORE'
	dependsOn = (
		'SDL', 'SDL_image', 'SDL_ttf', 'libpng', 'tcl', 'libxml2', 'zlib'
		)

class GLRenderer(Component):
	niceName = 'GL renderer'
	makeName = 'GL'
	dependsOn = ('gl', 'glew')

class CassetteJack(Component):
	niceName = 'CassetteJack'
	makeName = 'JACK'
	dependsOn = ('jack-audio-connection-kit', )

def iterComponents():
	yield EmulationCore
	yield GLRenderer
	yield CassetteJack

def _computeCoreLibs():
	'''Returns the Make names of the libraries needed by the emulation core
	component, in the proper order for static linking.
	For static linking, if lib A depends on B, A must be in the list before B.
	TODO: Actually the link flags of lib A should already reflect that.
	      Are we compensating for bad link flags?
	'''
	coreDependsOn = set(EmulationCore.dependsOn)
	dependencies = dict(
		(name, set(getPackage(name).dependsOn) & coreDependsOn)
		for name in coreDependsOn
		)

	# Flatten dependency graph.
	# The algorithm is quadratic, but that doesn't matter for the small data
	# sizes we're feeding it.
	orderedDependencies = []
	while dependencies:
		# Find a package that does not depend on anything is not in
		# orderedDependencies yet.
		freePackages = set(
			name
			for name, dependsOn in dependencies.iteritems()
			if not dependsOn
			)
		if not freePackages:
			raise ValueError('Circular dependency between packages')
		orderedDependencies += reversed(sorted(freePackages))
		# Remove dependency just moved to orderedDependencies.
		for name in freePackages:
			del dependencies[name]
		for dependsOn in dependencies.itervalues():
			dependsOn -= freePackages

	# Reverse order and output Make names.
	for name in reversed(orderedDependencies):
		yield getPackage(name).getMakeName()

coreLibs = tuple(_computeCoreLibs())
