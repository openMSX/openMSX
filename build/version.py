# $Id$
# Contains the openMSX version number and versioning related functions.

from executils import captureStdout
from makeutils import filterFile, filterLines

from os import makedirs
from os.path import isdir

# Name used for packaging.
packageName = 'openmsx'

# Version number.
packageVersionNumber = '0.7.0'
packageVersionSuffix = ''
packageVersion = packageVersionNumber + packageVersionSuffix

# Is this a release version ("True") or development version ("False").
releaseFlag = False

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
		log, 'svn info', r'Revision:\s*(\d+)'
		)

def extractSVNGitRevision(log):
	return _extractRevisionFromStdout(
		log, 'git log -n 100', r'\s*git-svn-id:.*@(\d+)'
		)

def extractChangeLogRevision(log):
	for revision, in filterFile('ChangeLog', r'\$Id: ChangeLog (\d+).*\$'):
		print >> log, 'Revision number found in ChangeLog: %s' % revision
		return revision
	else:
		print >> log, 'Revision number not found in ChangeLog'
		return None

def extractRevision():
	if not isdir('derived'):
		makedirs('derived')
	log = open('derived/version.log', 'w')
	print >> log, 'Extracting revision number...'
	try:
		revision = (
			extractSVNRevision(log) or
			extractSVNGitRevision(log) or
			extractChangeLogRevision(log) or
			'unknown'
			)
		print >> log, 'Revision number: %s' % revision
	finally:
		log.close()
	return revision

def getVersionedPackageName():
	if releaseFlag:
		return '%s-%s' % (packageName, packageVersion)
	else:
		return '%s-%s-%s' % (packageName, packageVersion, extractRevision())
