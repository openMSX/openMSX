from io import open
from os import environ
from os.path import isfile
from subprocess import PIPE, Popen
import sys

def _determineMounts():
	# The MSYS shell provides a Unix-like file system by translating paths on
	# the command line to Windows paths. Usually this is transparent, but not
	# for us since we call GCC without going through the shell.

	# Figure out the root directory of MSYS.
	proc = Popen(
		[ msysShell(), '-c', '"%s" -c \'import sys ; print sys.argv[1]\' /'
			% sys.executable.replace('\\', '\\\\') ],
		stdin = None,
		stdout = PIPE,
		stderr = PIPE,
		)
	stdoutdata, stderrdata = proc.communicate()
	if stderrdata or proc.returncode:
		if stderrdata:
			print('Error determining MSYS root:', stderrdata, file=sys.stderr)
		if proc.returncode:
			print('Exit code %d' % proc.returncode, file=sys.stderr)
		raise OSError('Error determining MSYS root')
	msysRoot = stdoutdata.strip()

	# Figure out all mount points of MSYS.
	mounts = {}
	fstab = msysRoot + '/etc/fstab'
	if isfile(fstab):
		try:
			with open(fstab, 'r', encoding='utf-8') as inp:
				for line in inp:
					line = line.strip()
					if line and not line.startswith('#'):
						nativePath, mountPoint = (
							path.rstrip('/') + '/' for path in line.split()[:2]
							)
						if nativePath != 'none':
							mounts[mountPoint] = nativePath
		except OSError as ex:
			print('Failed to read MSYS fstab:', ex, file=sys.stderr)
		except ValueError as ex:
			print('Failed to parse MSYS fstab:', ex, file=sys.stderr)
	mounts['/'] = msysRoot + '/'
	return mounts

def msysPathToNative(path):
	if path.startswith('/'):
		if len(path) == 2 or (len(path) > 2 and path[2] == '/'):
			# Support drive letters as top-level dirs.
			return '%s:/%s' % (path[1], path[3 : ])
		longestMatch = ''
		for mountPoint in msysMounts.keys():
			if path.startswith(mountPoint):
				if len(mountPoint) > len(longestMatch):
					longestMatch = mountPoint
		return msysMounts[longestMatch] + path[len(longestMatch) : ]
	else:
		return path

def msysActive():
	return environ.get('OSTYPE') == 'msys' or 'MSYSCON' in environ

def msysShell():
	return environ.get('SHELL') or 'sh.exe'

if msysActive():
	msysMounts = _determineMounts()
else:
	msysMounts = None

if __name__ == '__main__':
	print('MSYS mounts:', msysMounts)
