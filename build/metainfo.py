# Generates an AppStream MetaInfo file from a template, filling in the list of
# releases from the release history and the copyright message.

from outpututils import rewriteIfChanged
from version import getCopyright

from io import open
import re
import sys

# The first release that has a GitHub release page with release notes; for
# older releases, only the Git tag exists.
FIRST_RELEASE_WITH_NOTES = (0, 11, 0)

def iterReleases(releaseHistoryPath):
	'''Yields (version, date) for every release in the release history,
	newest first.
	'''
	releasePattern = re.compile(r'^openMSX (\S+) \((\d{4}-\d{2}-\d{2})\)$')
	with open(releaseHistoryPath, 'r', encoding='utf-8') as inp:
		for line in inp:
			match = releasePattern.match(line.rstrip('\n'))
			if match:
				yield match.group(1), match.group(2)

def iterReleaseElements(releaseHistoryPath, indent):
	for version, date in iterReleases(releaseHistoryPath):
		versionTuple = tuple(int(part) for part in version.split('.'))
		if versionTuple >= FIRST_RELEASE_WITH_NOTES:
			tag = 'RELEASE_' + version.replace('.', '_')
			yield '%s<release version="%s" date="%s">' % (indent, version, date)
			yield '%s\t<url type="details">' \
				'https://github.com/openMSX/openMSX/releases/tag/%s</url>' \
				% (indent, tag)
			yield '%s</release>' % indent
		else:
			yield '%s<release version="%s" date="%s"/>' % (indent, version, date)

def iterMetaInfo(templatePath, releaseHistoryPath):
	with open(templatePath, 'r', encoding='utf-8') as inp:
		for line in inp:
			line = line.rstrip('\n')
			if line.strip() == '%RELEASES%':
				indent = line[:len(line) - len(line.lstrip())]
				yield from iterReleaseElements(releaseHistoryPath, indent)
			else:
				yield line.replace('%COPYRIGHT%', getCopyright())

if __name__ == '__main__':
	if len(sys.argv) == 4:
		rewriteIfChanged(sys.argv[3], iterMetaInfo(sys.argv[1], sys.argv[2]))
	else:
		print(
			'Usage: python3 metainfo.py TEMPLATE RELEASE_HISTORY OUTPUT',
			file=sys.stderr
			)
		sys.exit(2)
