# $Id$
# Contains the openMSX version number and versioning related functions.

from executils import captureStdout
from makeutils import filterLines

from os import makedirs
from os.path import isdir

# Name used for packaging.
packageName = 'openmsx'

# Version number.
packageVersionNumber = '0.9.1'
# Note: suffix should be empty or with dash, like "-rc1" or "-test1"
packageVersionSuffix = '-rc3'
packageVersion = packageVersionNumber + packageVersionSuffix

# Is this a release version ("True") or development version ("False").
releaseFlag = True

def _extractRevisionFromStdout(log, command, regex):
	text = captureStdout(log, command)
	if text is None:
		# Error logging already done by captureStdout().
		return None
	# pylint 0.18.0 somehow thinks captureStdout() returns a list, not a string.
	lines = text.split('\n') # pylint: disable-msg=E1103
	for revision, in filterLines(lines, regex):
		print >> log, 'Revision number found by "%s": %s' % (command, revision)
		return revision
	else:
		print >> log, 'Revision number not found in "%s" output:' % command
		print >> log, text
		return None

def extractSVNRevision(log):
	return _extractRevisionFromStdout(
		log, 'svn info', r'Last Changed Rev:\s*(\d+)'
		)

def extractSVNGitRevision(log):
	return _extractRevisionFromStdout(
		log, 'git log -n 100', r'\s*git-svn-id:.*@(\d+)'
		)

_cachedRevision = False # because None is a valid result

def extractRevision():
	global _cachedRevision
	if _cachedRevision is not False:
		return _cachedRevision
	if releaseFlag:
		# Running "svn info" creates a ~/.subversion directory, which is
		# undesired on automated build machines and pkgsrc complains about it.
		return None
	if not isdir('derived'):
		makedirs('derived')
	log = open('derived/version.log', 'w')
	print >> log, 'Extracting revision number...'
	try:
		revision = (
			extractSVNRevision(log) or
			extractSVNGitRevision(log)
			)
		print >> log, 'Revision number: %s' % revision
	finally:
		log.close()
	_cachedRevision = revision
	return revision

def extractRevisionNumber():
	return int(extractRevision() or 1)

def extractRevisionString():
	return extractRevision() or 'unknown'

def getVersionedPackageName():
	if releaseFlag:
		return '%s-%s' % (packageName, packageVersion)
	else:
		return '%s-%s-%s' % (
			packageName, packageVersion, extractRevisionString()
			)
