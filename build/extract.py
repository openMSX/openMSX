# Extract files from archives.

from os import O_CREAT, O_WRONLY, fdopen, mkdir, open as osopen, utime
try:
	from os import O_BINARY
except ImportError:
	# Platforms that do not define O_BINARY do not need it either.
	O_BINARY = 0
try:
	from os import symlink
except ImportError:
	def symlink(source, link_name):
		raise OSError('OS does not support symlink creation')
from os.path import abspath, isdir, join as joinpath, sep, split as splitpath
from stat import S_IRWXU, S_IRWXG, S_IRWXO, S_IXUSR, S_IXGRP, S_IXOTH
from tarfile import TarFile
import sys

from detectsys import detectOS

hostOS = detectOS()

# Note: Larger buffers might make extraction slower.
bufSize = 16384

def extract(archivePath, destDir, rename = None):
	'''Extract the given archive to the given directory.
	If a rename function is given, it is called with the output path relative
	to the destination directory; the value returned by the rename function is
	used as the actual relative destination file path.
	This function sets file ownership and permissions like is done in newly
	created files and ignores the ownership and permissions from the archive,
	since we are not restoring a backup.
	'''
	absDestDir = abspath(destDir) + sep
	if not isdir(absDestDir):
		raise ValueError(
			'Destination directory "%s" does not exist' % absDestDir
			)

	with TarFile.open(archivePath, errorlevel=2) as tar:
		for member in tar.getmembers():
			absMemberPath = abspath(joinpath(absDestDir, member.name))
			if member.isdir():
				absMemberPath += sep
			if not absMemberPath.startswith(absDestDir):
				raise ValueError(
					'Refusing to extract tar entry "%s" '
					'outside destination directory'
					% member.name
					)
			if rename:
				absMemberPath = absDestDir + rename(
					absMemberPath[len(absDestDir) : ]
					)

			if member.isfile():
				mode = S_IRWXU | S_IRWXG | S_IRWXO
				if not (member.mode & S_IXUSR):
					mode &= ~(S_IXUSR | S_IXGRP | S_IXOTH)
				fd = osopen(absMemberPath, O_CREAT | O_WRONLY | O_BINARY, mode)
				with fdopen(fd, 'wb') as out:
					inp = tar.extractfile(member)
					bytesLeft = member.size
					while bytesLeft > 0:
						buf = inp.read(bufSize)
						out.write(buf)
						bytesLeft -= len(buf)
			elif member.isdir():
				if not isdir(absMemberPath):
					mkdir(absMemberPath)
			elif member.issym():
				try:
					symlink(member.linkname, absMemberPath)
				except OSError as ex:
					print(
						'WARNING: Skipping symlink creation: %s -> %s: %s'
						% (absMemberPath, member.linkname, ex)
						)
			else:
				raise ValueError(
					'Cannot extract tar entry "%s": '
					'not a regular file, symlink or directory'
					% member.name
					)
			# Set file/directory modification time to match the archive.
			# For example autotools track dependencies between archived files
			# and will attempt to regenerate them if the time stamps indicate
			# one is older than the other.
			# Note: Apparently Python 2.5's utime() cannot set timestamps on
			#       directories in Windows.
			if member.isfile() or (
				member.isdir() and not hostOS.startswith('mingw')
				):
				utime(absMemberPath, (member.mtime, member.mtime))

class TopLevelDirRenamer(object):

	def __init__(self, newName):
		self.newName = newName

	def __call__(self, oldPath):
		head, tail = splitpath(oldPath)
		headParts = head.split(sep)
		if not headParts:
			raise ValueError(
				'Directory part is empty for entry "%s"' % oldPath
				)
		headParts[0] = self.newName
		return sep.join(headParts + [ tail ])

if __name__ == '__main__':
	if 3 <= len(sys.argv) <= 4:
		if len(sys.argv) == 4:
			renameTopLevelDir = TopLevelDirRenamer(sys.argv[3])
		else:
			renameTopLevelDir = None
		extract(sys.argv[1], sys.argv[2], renameTopLevelDir)
	else:
		print('Usage: python3 extract.py archive destination [new-top-level-dir]', file=sys.stderr)
		sys.exit(2)
