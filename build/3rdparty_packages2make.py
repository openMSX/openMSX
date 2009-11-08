# $Id$

from packages import getPackage

makeNames = (
	'ZLIB',
	'PNG',
	'FREETYPE',
	'SDL',
	'SDL_IMAGE',
	'SDL_TTF',
	'GLEW',
	'TCL',
	'XML',
	'DIRECTX',
	'OGG',
	'OGGZ',
	'VORBIS',
	'THEORA',
	)

def genDownloadURLs():
	yield '# Download locations for package sources.'
	for makeName in makeNames:
		package = getPackage(makeName)
		yield 'DOWNLOAD_%s:=%s' % (makeName, package.downloadURL)

def genVersions():
	yield '# These were the most recent versions at the moment of writing this Makefile.'
	yield '# You can use other versions if you like; adjust the names accordingly.'
	yield '# Note: Do not put comments behind the definition, since this will include'
	yield '#       a space in the value and spaces are separators for Make so the whole'
	yield '#       build process will break.'
	for makeName in makeNames:
		package = getPackage(makeName)
		yield 'PACKAGE_%s:=%s' % (makeName, package.getSourceDirName())

def genTarballs():
	yield '# Source tar file names for non-standard packages.'
	yield 'TARBALL_GLEW:=$(PACKAGE_GLEW)-src.tgz'
	yield 'TARBALL_TCL:=$(PACKAGE_TCL)-src.tar.gz'
	yield 'TARBALL_DIRECTX:=$(PACKAGE_DIRECTX)_mgw.tar.gz'
	yield '# Source tar file names for standard packages.'
	yield 'TARBALL_ZLIB:=$(PACKAGE_ZLIB).tar.gz'
	yield 'TARBALL_PNG:=$(PACKAGE_PNG).tar.gz'
	yield 'TARBALL_FREETYPE:=$(PACKAGE_FREETYPE).tar.gz'
	yield 'TARBALL_SDL:=$(PACKAGE_SDL).tar.gz'
	yield 'TARBALL_SDL_IMAGE:=$(PACKAGE_SDL_IMAGE).tar.gz'
	yield 'TARBALL_SDL_TTF:=$(PACKAGE_SDL_TTF).tar.gz'
	yield 'TARBALL_XML:=$(PACKAGE_XML).tar.gz'
	yield 'TARBALL_OGG:=$(PACKAGE_OGG).tar.gz'
	yield 'TARBALL_OGGZ:=$(PACKAGE_OGGZ).tar.gz'
	yield 'TARBALL_VORBIS:=$(PACKAGE_VORBIS).tar.gz'
	yield 'TARBALL_THEORA:=$(PACKAGE_THEORA).tar.gz'

for line in genDownloadURLs():
	print line
print
for line in genVersions():
	print line
print
for line in genTarballs():
	print line
