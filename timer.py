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

newcode = 0
cliResult=os.popen("git pull")

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

print (newcode)
	