#!/usr/bin/env python3
# Checks that a release tag matches the version information in the tagged code,
# so that mistakes in preparing a release are found before building it.
# Usage: check_release.py <tag> [<commit>]
# The tag must be like RELEASE_1_2_3 or RELEASE_1_2_3_RC1; other tags (for
# example tags for testing the release workflow) are not checked.
# By default the files of the tag itself are checked. To check before tagging,
# pass the commit you're going to tag, e.g.:
#   check_release.py RELEASE_1_2_3_RC1 HEAD

from subprocess import CalledProcessError, check_output
import ast, re, sys

def showFile(commit, path):
	return check_output(['git', 'show', '%s:%s' % (commit, path)], text=True)

def getAssignment(source, name):
	match = re.search(r'^%s\s*=\s*(.+?)\s*$' % name, source, re.MULTILINE)
	if match is None:
		return None
	try:
		return ast.literal_eval(match.group(1))
	except (ValueError, SyntaxError):
		return None

def checkRelease(tag, commit):
	match = re.fullmatch(r'RELEASE_(\d+(?:_\d+)+)(?:_RC(\d+))?', tag)
	if match is None:
		return None
	number = match.group(1).replace('_', '.')
	rc = match.group(2)
	suffix = '' if rc is None else '-rc%s' % rc
	niceVersion = number + ('' if rc is None else '-RC%s' % rc)

	errors = []
	versionPy = showFile(commit, 'build/version.py')
	if getAssignment(versionPy, 'packageVersionNumber') != number:
		errors.append('build/version.py: packageVersionNumber should be %r' % number)
	if getAssignment(versionPy, 'packageVersionSuffix') != suffix:
		errors.append('build/version.py: packageVersionSuffix should be %r' % suffix)
	if getAssignment(versionPy, 'releaseFlag') is not True:
		errors.append('build/version.py: releaseFlag should be True')

	datePattern = r'\((\d{4}-\d{2}-\d{2})\)'
	notesTitle = showFile(commit, 'doc/release-notes.txt').splitlines()[0]
	notesMatch = re.fullmatch(
		r'Release Notes for openMSX %s %s' % (re.escape(niceVersion), datePattern),
		notesTitle)
	if notesMatch is None:
		errors.append(
			'doc/release-notes.txt: title should be "Release Notes for openMSX '
			'%s (<date>)", but is "%s"' % (niceVersion, notesTitle))

	historyMatch = re.search(
		r'^openMSX %s %s$' % (re.escape(niceVersion), datePattern),
		showFile(commit, 'doc/release-history.txt'), re.MULTILINE)
	if historyMatch is None:
		errors.append(
			'doc/release-history.txt: no entry "openMSX %s (<date>)"' % niceVersion)

	if notesMatch and historyMatch and notesMatch.group(1) != historyMatch.group(1):
		errors.append(
			'release date differs: %s in doc/release-notes.txt, %s in '
			'doc/release-history.txt' % (notesMatch.group(1), historyMatch.group(1)))
	return errors

if __name__ == '__main__':
	if len(sys.argv) not in (2, 3):
		print('Usage: %s <tag> [<commit>]' % sys.argv[0], file=sys.stderr)
		sys.exit(2)
	tag = sys.argv[1]
	commit = sys.argv[2] if len(sys.argv) == 3 else tag
	try:
		errors = checkRelease(tag, commit)
	except CalledProcessError as ex:
		print('Failed to read the files of "%s": %s' % (commit, ex), file=sys.stderr)
		sys.exit(1)
	if errors is None:
		print('Tag "%s" is not a release tag, skipping the checks.' % tag)
		sys.exit(0)
	for error in errors:
		print('ERROR: %s' % error, file=sys.stderr)
	if errors:
		sys.exit(1)
	print('Tag "%s" matches the version information in "%s".' % (tag, commit))
