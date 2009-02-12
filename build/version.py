# $Id$

from makeutils import extractMakeVariables, filterFile, parseBool

_versionVars = extractMakeVariables('build/version.mk')
packageVersion = _versionVars['PACKAGE_VERSION']
releaseFlag = parseBool(_versionVars['RELEASE_FLAG'])

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
