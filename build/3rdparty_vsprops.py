# $Id$

from packages import getPackage
from vsprops import writeVSPropertyFile

props = {
	'DerivedDir': r'..\..\derived',
	'3rdPartySrcDir': r'$(DerivedDir)\3rdparty\src',
	'OpenMSXSrcDir': r'..\..\src',
	'BuildFlavor': '$(PlatformName)-VC-$(ConfigurationName)',
	'BuildDir': r'$(DerivedDir)\$(BuildFlavor)',
	'3rdPartyBuildDir': r'$(BuildDir)\3rdparty',
	'3rdpartyIntDir': r'$(3rdpartyBuildDir)\build',
	'3rdpartyOutDir': r'$(3rdPartyBuildDir)\install\lib',
	'OpenMSXIntDir': r'$(BuildDir)\build',
	'OpenMSXOutDir': r'$(BuildDir)\install',
	'OpenMSXConfigDir': r'$(BuildDir)\config',
	'LibNameFreeType': getPackage('FREETYPE').getSourceDirName(),
	'LibNameGlew': getPackage('GLEW').getSourceDirName(),
	'LibNameLibPng': getPackage('PNG').getSourceDirName(),
	'LibNameLibXml': getPackage('XML').getSourceDirName(),
	'LibNameSDL': getPackage('SDL').getSourceDirName(),
	'LibNameSDLmain': 'SDLmain',
	'LibNameSDL_ttf': getPackage('SDL_TTF').getSourceDirName(),
	'LibNameTcl': getPackage('TCL').getSourceDirName(),
	'LibNameZlib': getPackage('ZLIB').getSourceDirName(),
	}

if __name__ == '__main__':
	writeVSPropertyFile('build/3rdparty/3rdparty.vsprops', props)
