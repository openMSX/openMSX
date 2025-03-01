# Extract files from archives.

from pathlib import Path
from os import O_CREAT, O_WRONLY, fdopen, open as osopen, utime
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
	absDestDir = Path(destDir).resolve()
	if not absDestDir.is_dir():
		raise ValueError(
			f'Destination directory "{absDestDir}" does not exist'
		)

	with TarFile.open(archivePath, errorlevel=2) as tar:
		for member in tar.getmembers():
			absMemberPath = absDestDir / member.name
			if not absMemberPath.is_relative_to(absDestDir):
				raise ValueError(
					f'Refusing to extract tar entry "{member.name}" '
					'outside destination directory'
				)
			if rename:
				absMemberPath = absDestDir / rename(
					absMemberPath.relative_to(absDestDir)
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
				absMemberPath.mkdir(exist_ok=True)
			elif member.issym():
				try:
					symlink(member.linkname, absMemberPath)
				except OSError as ex:
					print(
						'WARNING: Skipping symlink creation: '
						f'{absMemberPath} -> {member.linkname}: {ex}'
					)
			else:
				raise ValueError(
					f'Cannot extract tar entry "{member.name}": '
					'not a regular file, symlink or directory'
				)
			# Set file/directory modification time to match the archive.
			# For example autotools track dependencies between archived files
			# and will attempt to regenerate them if the time stamps indicate
			# one is older than the other.
			if member.isfile():
				utime(absMemberPath, (member.mtime, member.mtime))

class TopLevelDirRenamer:

	def __init__(self, newName):
		self.newName = newName

	def __call__(self, oldPath):
		return Path(self.newName, *oldPath.parts[1:])

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
