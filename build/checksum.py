from __future__ import print_function
from hashlib import new as newhash
from os import stat
from os.path import isfile
import sys

def verifyFile(filePath, fileLength, checksums):
	actualLength = stat(filePath).st_size
	if actualLength != fileLength:
		raise IOError(
			'Expected length %d, actual length %d' % (fileLength, actualLength)
			)
	hashers = {}
	for algo in checksums.iterkeys():
		try:
			hashers[algo] = newhash(algo)
		except ValueError as ex:
			raise IOError('Failed to create "%s" hasher: %s' % (algo, ex))
	bufSize = 16384
	with open(filePath, 'rb') as inp:
		while True:
			buf = inp.read(bufSize)
			if not buf:
				break
			for hasher in hashers.itervalues():
				hasher.update(buf)
	for algo, hasher in sorted(hashers.iteritems()):
		if checksums[algo] != hasher.hexdigest():
			raise IOError('%s checksum mismatch' % algo)

def main(filePath, fileLengthStr, checksumStrs):
	if not isfile(filePath):
		print('No such file: %s' % filePath, file=sys.stderr)
		sys.exit(2)
	try:
		fileLength = int(fileLengthStr)
	except ValueError:
		print('Length should be an integer', file=sys.stderr)
		sys.exit(2)
	checksums = {}
	for checksumStr in checksumStrs:
		try:
			algo, hashval = checksumStr.split('=')
		except ValueError:
			print('Invalid checksum format: %s' % checksumStr, file=sys.stderr)
			sys.exit(2)
		else:
			checksums[algo] = hashval
	print('Validating: %s' % filePath)
	try:
		verifyFile(filePath, fileLength, checksums)
	except IOError as ex:
		print('Validation FAILED: %s' % ex, file=sys.stderr)
		sys.exit(1)
	else:
		print('Validation passed')
		sys.exit(0)

if __name__ == '__main__':
	if len(sys.argv) >= 3:
		main(
			sys.argv[1],
			sys.argv[2],
			sys.argv[3 : ]
			)
	else:
		print(
			'Usage: python checksum.py FILE LENGTH (ALGO=HASH)*',
			file=sys.stderr
			)
		sys.exit(2)
