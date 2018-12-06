# Contains the openMSX version number and versioning related functions.

from executils import captureStdout
from makeutils import filterLines

from os import makedirs
from os.path import isdir

import re

# Name used for packaging.
packageName = 'openmsx'

# Version number.
packageVersionNumber = '0.15.0'

# Version code for Android must be an incremental number
# Increase this number for each release build. For a dev build, the
# version number is based on the git commit count but for a release
# build, it must be hardcoded
androidReleaseVersionCode=10

# Note: suffix should be empty or with dash, like "-rc1" or "-test1"
packageVersionSuffix = ''
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

def extractGitRevision(log):
	return _extractRevisionFromStdout(
		log, 'git describe --dirty', r'\S+?-(\S+)$'
		)

def extractNumberFromGitRevision(revisionStr):
	if revisionStr is None:
		return None
	if revisionStr == 'dirty':
		return None
	return re.match(r'(\d+)+', revisionStr).group(0)

_cachedRevision = False # because None is a valid result

def extractRevision():
	global _cachedRevision
	if _cachedRevision is not False:
		return _cachedRevision
	if releaseFlag:
		# Not necessary, we do not append revision for a release build.
		return None
	if not isdir('derived'):
		makedirs('derived')
	log = open('derived/version.log', 'w')
	print >> log, 'Extracting revision info...'
	try:
		revision = extractGitRevision(log)
		print >> log, 'Revision string: %s' % revision
		print >> log, 'Revision number: %s' % extractNumberFromGitRevision(revision)
	finally:
		log.close()
	_cachedRevision = revision
	return revision

def extractRevisionNumber():
	return int(extractNumberFromGitRevision(extractRevision()) or 1)

def extractRevisionString():
	return extractRevision() or 'unknown'

def getVersionedPackageName():
	if releaseFlag:
		return '%s-%s' % (packageName, packageVersion)
	else:
		return '%s-%s-%s' % (
			packageName, packageVersion, extractRevisionString()
			)

def countGitCommits():
	if not isdir('derived'):
		makedirs('derived')
	log = open('derived/commitCountVersion.log', 'w')
	print >> log, 'Extracting commit count...'
	try:
		commitCount = captureStdout(log, 'git rev-list HEAD --count')
		print >> log, 'Commit count: %s' % commitCount
	finally:
		log.close()
	return commitCount

def getAndroidVersionCode():
	if releaseFlag:
		return '%s' % (androidReleaseVersionCode)
	else:
		return '%s' % ( countGitCommits() )

if __name__ == '__main__':
	print packageVersionNumber
