# $Id$
# Contains the openMSX version number and versioning related functions.

from makeutils import filterFile

# Name used for packaging.
packageName = 'openmsx'

# Version number.
packageVersion = '0.7.0'

# Is this a release version ("True") or development version ("False").
releaseFlag = False

# TODO: Before extraction of SVN or git-SVN revision number can be done, we
#       should figure out a way to avoid rewriting Version.ii on every build.
#       Option 1: Read Version.ii and do not write if new contents are the same.
#       Option 2: Persist the extracted revision number.
#       I prefer option 2, since it is more modular (separate extraction and
#       include generation steps) but option 1 might be easier to imlement at
#       first (no need to persist anything).

def extractSVNRevision():
	return None
	# `svn info`, re = 'Revision:\s*(\d+)'

def extractSVNGitRevision():
	return None
	# `git-log`, re = 'git-svn-id:.*@(\d+)'

def extractChangeLogRevision():
	for revision, in filterFile('ChangeLog', r'\$Id: ChangeLog (\d+).*\$'):
		return revision
	else:
		return None

def extractRevision():
	return (
		extractSVNRevision() or
		extractSVNGitRevision() or
		extractChangeLogRevision() or
		'unknown'
		)

def getVersionedPackageName():
	if releaseFlag:
		return '%s-%s' % (packageName, packageVersion)
	else:
		return '%s-%s-%s' % (packageName, packageVersion, extractRevision())
