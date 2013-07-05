import msvcrt 
import os

#set standard settings

#XP support 0=off/1=on
compileXP=0 

#xp 32=win32/64=x64 version
version=64

#Loop variable keep at 0
quit=0

def menu():
	os.system('color 9f')
	os.system('mode 80,24')
	os.system('TITLE openMSX Auto Compiler')
	os.system('cls')

	print('------------------------------------------------------')
	print(' openMSX build menu for Visual Studio 2012')
	print('------------------------------------------------------')
	print
	print(' [1] Update from Repository')
	print(' [2] Get 3rd Party Files')
	print
	print(' [3] Compile 3rd-party files')
	print(' [4] Compile Release')
	print(' [5] Generate Installer and Packages')
	print
	print(' [6] Toggle Win32/Win64 Version')
	print(' [7] Toggle XP Support')
	print
	print(' [Q] Quit')
	print
	print('----------------------------------------------------')	
	if version==32:
		print " Resulting Binary: Windows 32bit Version"
	else:
		print " Resulting Binary: Windows 64bit Version"	
	if compileXP==1:
		print ' Resulting Binary: XP Support ON'
	else:
		print ' Resulting Binary: XP Support Off'
	print('----------------------------------------------------')
	print
	print('The DirectX June 2010 SDK is needed for XP support')

def updateSourceCode():
	os.system('cls')
	os.system('git pull')
	os.system('pause')
	menu()
	
def update3rdparty():
	os.system('cls')
	#get to the root dir so we can execute the 3rd party download script
	os.chdir('..\\..\\')
	#execute the 3rd party script
	os.system('call "python" build\\thirdparty_download.py windows')
	print os.getcwd()
	#get back to the current dir
	os.chdir('build\\package-windows')
	os.system('pause')
	menu()
	
def compile(whattocompile):
	os.chdir('..\\..\\')
	fo = open("foo.bat", "w")
	fo.write('cls'+chr(10))
	fo.write('Echo Compiling openMSX'+chr(10))
	
	
	if version==64:
		compiler='x86_amd64'
	else:
		compiler='x86'
	
	fo.write ('call "C:\\Program Files (x86)\\Microsoft Visual Studio 11.0\\VC\\vcvarsall.bat" '+str(compiler)+chr(10))
		
	if compileXP==1:
		support='v110_xp'
	else:
		support='v110'
		
	fo.write ('set PlatformToolset='+str(support)+chr(10))
	
	if version==64:	compilefor='x64'
	if version==32:	compilefor='Win32'
	
	if whattocompile=='OpenMSX': fo.write ('msbuild -p:Configuration=Release;Platform='+str(compilefor)+' build\\msvc\\openmsx.sln /m'+chr(10))
	if whattocompile=='3rdParty': fo.write ('msbuild -p:Configuration=Release;Platform='+str(compilefor)+' build\\3rdparty\\3rdparty.sln /m'+chr(10))

	fo.close()
	os.system('foo.bat')
	os.chdir('build\\package-windows')
	os.system('pause')
	menu()
	
def package():
	os.chdir('..\\..\\')
	if version==64:
		os.system('build\\package-windows\\package.cmd x64 release ..\\wxcatapult')
	else:
		os.system('build\\package-windows\\package.cmd win32 release ..\\wxcatapult')
	os.chdir('build\\package-windows')
	os.system('pause')
	menu()

# Main Loop	
menu()

while quit==0:
	x = ord(msvcrt.getch())
	
	#print "Got key: %d" % x
	if x==49:updateSourceCode()
	
	if x==50:update3rdparty()
	
	if x==51:compile('3rdParty')
	
	if x==52:compile('OpenMSX')
	
	if x==53:package()
	
	if x==54:
		if version==64: 
			version=32 
		else:
			version=64
		menu()

	if x==55:
		if compileXP==1: 
			compileXP=0 
		else:
			compileXP=1
		menu()
	
	if x==113:
		os.system('cls')
		quit=1
		
