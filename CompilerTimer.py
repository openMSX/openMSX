import msvcrt 
import datetime
import time
import os
import sys
import subprocess
import string
import re
import zipfile
#import MySQLdb
import fileinput
import smtplib

#time.sleep(60)
#add to path >>> C:\Python27;C:\Program Files (x86)\WiX Toolset v3.10\bin;C:\Program Files (x86)\WinSCP;
#add this file to autostart C:\Users\Vampier\AppData\Roaming\Microsoft\Windows\Start Menu\Programs\Startup 
#run upload from a command shell

os.system("net use * /delete /y")
os.system("net use y: \\\\192.168.1.11\\WebServer go /user:root")

def menu():
	os.chdir("c:\\compiler\\openmsx\\")
	os.system('color 9f')
	os.system('mode 64,12')
	os.system('TITLE openMSX Auto Compiler')
	os.system('cls')
	os.system('echo ----------------------------------------------------')
	os.system('echo [ESC] Check For New version')
	os.system('echo [F1] Force Compile')
	os.system('echo [F2] Cleanup Derived folder')
	os.system('echo [F3] Cleanup WebPage')	
	os.system('echo [F4] Reset GIT to source state')	
	os.system('echo [F5] Compile other GIT version')
	os.system('echo [F6] Refresh 3rd party')	
	os.system('echo ----------------------------------------------------')

def kbFunc(): 
	return ord(msvcrt.getch()) if msvcrt.kbhit() else 0

def SendMail(TO,TEXT,SUBJECT):
	gmail_user = "openmsxcompiler@gmail.com"
	gmail_pwd = "xojcjmxjhwkdhrbt" # app password
	
	# Prepare actual message
	message = """\From: %s\nTo: %s\nSubject: %s\n\n%s
	""" % (gmail_user, ", ".join(TO), SUBJECT, TEXT)
	try:
		#server = smtplib.SMTP(SERVER) 
		server = smtplib.SMTP("smtp.gmail.com", 587) #or port 465 doesn't seem to work!
		server.ehlo()
		server.starttls()
		server.login(gmail_user, gmail_pwd)
		server.sendmail(gmail_user, TO, message)
		#server.quit()
		server.close()
		print ("successfully sent the mail")
	except:
		print ("failed to send mail")	



def getGIT():
	version='empty'
	compile=0
	myl = []
	
	os.system("cls")
	print ("checking")
	#os.popen("git reset HEAD --hard")
	cliResult=os.popen("git pull")
	line=''.join(map(str,(cliResult.readlines())))
	version=''.join(map(str,(os.popen("git describe").readlines()))).rstrip('\n')

	newcode = 0

	CommitHash = os.popen("git rev-parse --verify HEAD")
	CommitHash = str(CommitHash.readline()).rstrip('\n')
	print ('New Hash:'+CommitHash)

	file1 = open('LatestCommit.txt', 'r')
	LatestCommit = ''.join(map(str,(file1.readlines())))
	file1.close()

	print ('Old Hash:'+LatestCommit)

	if CommitHash==LatestCommit:
		print ('Found No New Code')
	else:
		print ('Found New Code')
		file1 = open('LatestCommit.txt', 'w')
		file1.writelines(CommitHash)
		file1.close()
		newcode=1
				
		GetCommits =os.popen("git log -p -1")
		TEXT=''.join(map(str,(GetCommits.readlines())))
		SUBJECT = "openMSX commit - "+str(version);
		TO = ['vampiermsx@gmail.com','vermaelen.wouter@gmail.com','manuel.bilderbeek@gmail.com','maarten@treewalker.org','dhran@scarlet.be'] 
		SendMail(TO,TEXT,SUBJECT)

		compile=1
		compilenow(version) 
		
	myl.append(compile)
	myl.append(version)	
	
	return myl

def displayTime():
		sys.stdout.write('openMSX compiler timer: '+time.strftime("%m/%d/%Y @ %H:%M:%S", time.localtime())+'\r')
		sys.stdout.flush() 
		
def update3rdparty():
	os.chdir('C:\\compiler\\openMSX\\');
	os.system('call "python" C:\\compiler\\openMSX\\build\\thirdparty_download.py windows');
	os.system('compile_3rd.bat');
	
		
def compilenow(version):
	cleanup()
	os.system('mode 120,10')
	os.system('cls')
	os.system('color cf')
	
	print ("Compiling revision: "+version)
	
	f = open('y:\\status\\'+str(version)+'.txt','w')
	f.write('busy')
	f.close()
	
	FileName='Y:\\logs\\'+str(version)+'.html'
	print ("build started loging to: "+FileName)
	os.chdir('C:\\compiler\\openMSX\\');
	os.system('Compile.bat >'+FileName)
	
	#begin syntax highlight
	#replacements = {'warning':'<span style="background:#ff0">warning</span>', 'error':'<span style="background:#ff0">error</span>','c:\\compiler\\openmsx':'..','c:\\compiler\\openMSX':'..'}

	#lines = []
	#with open(FileName) as infile:
	#	for line in infile:
	#		for src, target in replacements.iteritems():
	#			line = line.replace(src, target)
	#		lines.append(line)
	#with open(FileName, 'w') as outfile:
	#	for line in lines:
	#		outfile.write(line)
	#end syntax highlight
	
	linestring = open(FileName, 'r').read()
	if linestring.count('Build FAILED.')!=0:
	
		errormail(version)
	
		f = open('y:\\error.txt','w')
		f.write(str(version))
		f.close()
		
		f = open('y:\\status\\'+str(version)+'.txt','w')
		f.write('Error')
		f.close()

	else:
	
		# zip files 
		zf = zipfile.ZipFile('y:\\zips\openmsx'+str(version)+'.zip', mode='w',compression=zipfile.ZIP_DEFLATED)
		zf.write('C:\\compiler\\openMSX\\derived\\x64-VC-Release\\install\\openmsx.exe',arcname='openMSX64.exe')
		zf.close();
					
		f = open('y:\\status\\'+str(version)+'.txt','w')
		f.write('complete')
		f.close()
		
		print ("Upload")
		os.system('call "upload.bat"')
	
		#refresh openMSX installation for testing purposes
		os.system('copy derived\\x64-VC-Release\\install\\openmsx.exe c:\\openMSX\\	/Y')
		os.system('xcopy share\\*.* c:\\openmsx\\share\\ /s /y')
	
	os.system('cls')
	os.system('mode 64,10')
	os.system('color 9f')

def cleanup():
	os.system('cls')

	print ("x64 Cleanup")
	os.system('rmdir C:\\compiler\\openMSX\\derived\\x64-VC-Release\\build  /s /q')
	os.system('rmdir C:\\Compiler\\openMSX\\derived\\x64-VC-Release\\config  /s /q')
	os.system('rmdir C:\\Compiler\\openMSX\\derived\\x64-VC-Release\\install /s /q')
	os.system('rmdir C:\\Compiler\\openMSX\\derived\\x64-VC-Release\\package-windows /s /q')
	time.sleep(2)

def SweepWebPage():	
	os.system('cls')
	os.system('del y:\\logs\\*.*  /s /q')
	os.system('del y:\\status\\*.*  /s /q')
	time.sleep(2)

def GitReset():
	os.system('cls')
	os.system('git reset --hard HEAD')
	time.sleep(2)
	
def errormail(version):
	TO = ['vampiermsx@gmail.com','vermaelen.wouter@gmail.com','manuel.bilderbeek@gmail.com','maarten@treewalker.org'] #must be a list
	SUBJECT = "openMSX Windows Build Error for revision "+str(version)
	TEXT = 'This is a Windows Autobuild message to inform you that the compilation failed for openMSX: http://openmsx.vampier.net/logs/'+str(version)+'.html'
	SendMail(TO,TEXT,SUBJECT)
		
#main loop
menu()

while 1:
	x = kbFunc()

	if x != 0:
		#print "Got key: %d" % x
		if x==27:
			print (getGIT())
			print ("done")
			time.sleep(2)
		if x==59:
			os.popen("git pull")
			version=''.join(map(str,(os.popen("git describe").readlines()))).rstrip('\n')
			compilenow(version) 
			time.sleep(2)			
		if x==60:	
			cleanup()
		if x==61:
			SweepWebPage()
		if x==62:
			GitReset()
		if x==63:
			GitReset()
			gitversion = input("git version to compile:")
			cliResult=os.system ('git checkout '+str(gitversion))
			version=''.join(map(str,(os.popen("git describe").readlines()))).rstrip('\n')
			compilenow(version) 
			os.system("git checkout master")
		if x==64:
			GitReset()
			update3rdparty()
		menu()
			
			
	else:
		now = datetime.datetime.now()
		if now.minute in range(0, 60, 5) and now.second==0:
			print (getGIT())
			#time.sleep(61)
			menu()
		displayTime()
		time.sleep(1)
