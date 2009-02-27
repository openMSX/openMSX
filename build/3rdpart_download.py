# $Id: 3rdparty-download.py 9252 2009-02-26 10:27:25Z vampier $

from download import downloadURL
import os.path
from urlparse import urljoin

#TODO:
#	Specify downloads for:
#	- MINGW (DX7)
#	- VC++
#	- OTHER
#
# required components -> required packages -> minus system packages -> 3rdparty packages

TARBALLS_DIR="../derived/3rdparty/download"
#SOURCE_DIR="derived/3rdparty/src"
#PATCHES_DIR="build/3rdparty"
#TIMESTAMP_DIR="$(BUILD_PATH)/timestamps"
#BUILD_DIR="$(BUILD_PATH)/build"
#INSTALL_DIR="$(BUILD_PATH)/install"



#Create Generic download class
class Package(object):
	downloadURL = None
	name = None
	version = None
	
	@classmethod
	def checkFile(cls):
		check= TARBALLS_DIR+'/%s' % (cls.getTarballName())
		return os.path.isfile(check)
		
	@classmethod
	def getTarballName(cls):
		return '%s-%s.tar.gz' % (cls.name, cls.version)

	@classmethod
	def getURL(cls):
		return urljoin(cls.downloadURL + '/', cls.getTarballName())

#Download Packages
class GLEW(Package):
	downloadURL = 'http://downloads.sourceforge.net/glew'
	name = 'glew'
	version = '1.5.1'

	@classmethod
	def getTarballName(cls):
		return '%s-%s-src.tgz' % (cls.name, cls.version)

class ZLib(Package):
	downloadURL = 'http://downloads.sourceforge.net/libpng'
	name = 'zlib'
	version = '1.2.3'

class libPNG(Package):
	downloadURL = 'http://downloads.sourceforge.net/libpng'
	name = 'libpng'
	version = '1.2.34'

class TCL(Package):
	downloadURL = 'http://downloads.sourceforge.net/tcl'
	name = 'tcl'
	version = '8.5.6'
	
	@classmethod
	def getTarballName(cls):
		return '%s%s-src.tar.gz' % (cls.name, cls.version)

class SDL(Package):
	downloadURL = 'http://www.libsdl.org/release'
	name = 'SDL'
	version = '1.2.13'
	
class SDL_image(Package):
	downloadURL = 'http://www.libsdl.org/projects/SDL_image/release'
	name = 'SDL_image'
	version = '1.2.7'
	
class SDL_ttf(Package):
	downloadURL = 'http://www.libsdl.org/projects/SDL_ttf/release'
	name = 'SDL_ttf'
	version = '2.0.9'

class Freetype(Package):
	downloadURL = 'http://nongnu.askapache.com/freetype'
	name = 'freetype'
	version = '2.3.7'
	
class libXML2(Package):
	downloadURL = 'http://xmlsoft.org/sources'
	name = 'libxml2'
	version = '2.7.2'

class DIRECTX(Package):
	downloadURL = 'http://alleg.sourceforge.net/files'
	name = 'dx'
	version = '70'
	
	@classmethod
	def getTarballName(cls):
		return '%s%s_mgw.tar.gz' % (cls.name, cls.version)


#download the packages
for package in (GLEW, ZLib,libPNG,TCL,SDL,SDL_image,SDL_ttf,Freetype,libXML2,DIRECTX):
	if not(package.checkFile()):
		downloadURL(package.getURL(),TARBALLS_DIR)
	else:
		print "%s version %s - already downloaded" % (package.name,package.version)
