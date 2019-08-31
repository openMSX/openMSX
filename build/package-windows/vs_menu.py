import msvcrt
import os

#set standard settings (No XP support/64Bit version)

compileXP = 0 	#XP support 0 = off/1 = on
version = 64 	#32 = win32/64 = x64 version
quit = 0 		#Loop variable keep at 0

#Some standard strings
location='C:\\Program Files (x86)\\Microsoft Visual Studio 14.0\\VC\\' #location of Visual Studio 2015 (Community) VC files
title=' openMSX build menu for Visual Studio 2015'

menuval = {
	 '[1] Update from Repository'
	,'[2] Get 3rd Party Files\n'
	,'[3] Compile 3rd-party files'
	,'[4] Compile openMSX Release'
	,'[5] Generate Installer and Packages\n'
	,'[6] Toggle Win32/Win64 Version'
	,'[7] Toggle Toggle XP Support\n'
	,'[8] Quit\n'
}

def menu():

	os.system('color 9f')
	os.system('mode 80,24')
	os.system('TITLE'+title)
	os.system('cls')

	print('-'*50)
	print(title)
	print('-'*50+'\n')

	for item in sorted(menuval): print(item)

	print('-'*50)
	print('  Resulting Binary settings:')

	if version == 32:	print('     - Windows 32bit Version')
	else: print('     - Windows 64bit Version')

	if compileXP == 1:print('     - XP Support ON')
	else: print('     - XP Support Off')

	print('-'*50)
	print()
	if compileXP == 1: print('The DirectX June 2010 SDK is needed for XP support')
	if quit == 1: os.system('cls')


def execCommand(cmd):
	os.system('cls')
	os.chdir('..\\..\\')
	os.system(cmd)
	os.chdir('build\\package-windows')
	os.system('pause')


def updateSourceCode():
	execCommand('git pull')


def update3rdparty():
	execCommand('call "python" build\\thirdparty_download.py windows')


def compile(whattocompile):

	compiler = 'x86' if version == 32 else 'amd64'
	support = 'v140' if compileXP == 0 else  'v140_xp'
	compilefor = 'Win32' if version == 32 else 'x64'
	compilewhat = 'build\\msvc\\openmsx.sln' if  whattocompile == 'OpenMSX' else 'build\\3rdparty\\3rdparty.sln'

	cmd = 'call "'+location+'vcvarsall.bat" '+str(compiler)+'&&'
	cmd += 'set PlatformToolset='+str(support)+'&&'
	cmd += 'msbuild -p:Configuration=Release;Platform='+str(compilefor)+' '+compilewhat+' /m'

	execCommand(cmd)


def package():
	packagefor = 'Win32' if version == 32 else 'x64'
	execCommand('build\\package-windows\\package.cmd '+packagefor+' release ..\\wxcatapult')


# Main Loop
menu()

while quit == 0:

	x  =  ord(msvcrt.getch())

	if x == 49: updateSourceCode()
	if x == 50: update3rdparty()
	if x == 51: compile('3rdParty')
	if x == 52: compile('OpenMSX')
	if x == 53: package()
	if x == 54:	version = 32 if version == 64 else 64
	if x == 55: compileXP = 0 if compileXP == 1 else 1
	if x == 56: quit = 1

	menu()
