#!/usr/bin/env python3
# Creates a source distribution package.

from os import makedirs, remove
from os.path import isdir
from subprocess import CalledProcessError, PIPE, Popen, check_output
from tarfile import TarError, TarFile
import stat, sys

verbose = False

def archiveFromGit(versionedPackageName, committish):
	prefix = '%s/' % versionedPackageName
	distBase = 'derived/dist/'
	umask = (
			stat.S_IRUSR | stat.S_IWUSR | stat.S_IXUSR |
			stat.S_IRGRP |                stat.S_IXGRP |
			stat.S_IROTH |                stat.S_IXOTH
			)

	def exclude(info):
		'''Returns True iff the given tar entry should be excluded.
		'''
		return any((
				info.name.endswith('/.gitignore'),
				info.name.startswith(prefix + 'doc/internal'),
				info.name.startswith(prefix + '.'),
				info.name[info.name.rfind('/') + 1:].startswith('meson'),
				))

	proc = Popen(
			('git', 'archive', '--prefix=' + prefix, '--format=tar', committish),
			bufsize = -1,
			stdin = None, stdout = PIPE, stderr = sys.stderr,
			)
	try:
		outTarPath = distBase + versionedPackageName + '.tar.gz'
		print('archive:', outTarPath)
		if not isdir(distBase):
			makedirs(distBase)
		try:
			with TarFile.open(outTarPath, 'w:gz') as outTar:
				# Copy entries from "git archive" into output tarball,
				# except for excluded entries.
				numIncluded = numExcluded = 0
				with TarFile.open(mode='r|', fileobj=proc.stdout) as inTar:
					for info in inTar:
						if exclude(info):
							if verbose:
								print('EX', info.name)
							numExcluded += 1
						else:
							if verbose:
								print('IN', info.name)
							numIncluded += 1
							info.uid = info.gid = 1000
							info.uname = info.gname = 'openmsx'
							info.mode = info.mode & umask
							outTar.addfile(info, inTar.extractfile(info))
				print('entries: %d included, %d excluded' % (
						numIncluded, numExcluded))
		except:
			# Clean up partial output file.
			remove(outTarPath)
			raise
	except:
		proc.terminate()
		raise
	else:
		data, _ = proc.communicate()
		if len(data) != 0:
			print(
				'WARNING: %d more bytes of data from "git archive" after '
				'tar stream ended' % len(data),
				file=sys.stderr
				)

def niceVersionFromGitDescription(description):
	'''Our release tag names are still based on naming limitations from CVS;
	convert them to something more pleasing to the eyes.
	'''
	parts = description.split('-')
	tag = parts[0]
	if tag.startswith('RELEASE_'):
		tagParts = tag.split('_')[1 : ]
		i = 0
		while i < len(tagParts) and tagParts[i].isdigit():
			i += 1
		version = '-'.join(
				['.'.join(tagParts[ : i])] +
				[s.lower() for s in tagParts[i : ]]
				)
	else:
		version = tag

	if len(parts) >= 2:
		parts[1] = 'dev' + parts[1]

	return '-'.join([version] + parts[1 : ])

def getDescription(committish):
	args = ['git', 'describe', '--abbrev=9']
	if committish is not None:
		args.append(committish)
	try:
		data = check_output(args)
	except CalledProcessError as ex:
		print(
			'"%s" returned %d' % (' '.join(args), ex.returncode),
			file=sys.stderr
			)
		raise
	else:
		return data.decode('utf-8', 'replace').rstrip('\n')

def main(committish = None):
	try:
		description = getDescription(committish)
	except CalledProcessError:
		sys.exit(1)
	version = niceVersionFromGitDescription(description)
	try:
		archiveFromGit('openmsx-%s' % version, description)
	except (OSError, TarError) as ex:
		print('ERROR: %s' % ex, file=sys.stderr)
		sys.exit(1)

if __name__ == '__main__':
	if len(sys.argv) == 1:
		main()
	elif len(sys.argv) == 2:
		main(sys.argv[1])
	else:
		print('Usage: gitdist.py [branch | tag]', file=sys.stderr)
		sys.exit(2)
