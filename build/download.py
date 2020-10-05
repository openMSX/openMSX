from os import remove, stat
from os.path import basename, isdir, isfile, join as joinpath
from urllib.parse import urlparse
from urllib.request import urlopen

import sys

class StatusLine(object):

	def __init__(self, out):
		self._out = out

	def __call__(self, message, progress = False):
		raise NotImplementedError

class InteractiveStatusLine(StatusLine):
	__length = 0

	def __call__(self, message, progress = False):
		self._out.write(('\r%-' + str(self.__length) + 's') % message)
		self.__length = max(self.__length, len(message))

class NoninteractiveStatusLine(StatusLine):

	def __call__(self, message, progress = False):
		if not progress:
			self._out.write(message + '\n')

def createStatusLine(out):
	if out.isatty():
		return InteractiveStatusLine(out)
	else:
		return NoninteractiveStatusLine(out)

def downloadURL(url, localDir):
	if not isdir(localDir):
		raise OSError('Local directory "%s" does not exist' % localDir)

	fileName = basename(urlparse(url).path)
	localPath = joinpath(localDir, fileName)
	prefix = 'Downloading %s: ' % fileName

	statusLine = createStatusLine(sys.stdout)
	statusLine(prefix + 'contacting server...')

	def reportProgress(doneSize, totalSize):
		statusLine(prefix + (
			'%d/%d bytes (%1.1f%%)...' % (
				doneSize, totalSize, (100.0 * doneSize) / totalSize
				)
			if totalSize > 0 else
			'%d bytes...' % doneSize
			), True)

	try:
		try:
			with urlopen(url) as inp:
				totalSize = int(inp.info().get('Content-Length', '0'))
				with open(localPath, 'wb') as out:
					doneSize = 0
					reportProgress(doneSize, totalSize)
					while True:
						data = inp.read(16384)
						if not data:
							break
						out.write(data)
						doneSize += len(data)
						reportProgress(doneSize, totalSize)
		except OSError:
			statusLine(prefix + 'FAILED.')
			raise
		else:
			statusLine(prefix + 'done.')
		finally:
			print()
	except:
		if isfile(localPath):
			statusLine(prefix + 'removing partial download.')
			remove(localPath)
		raise

if __name__ == '__main__':
	if len(sys.argv) == 3:
		try:
			downloadURL(*sys.argv[1 : ])
		except OSError as ex:
			print(ex, file=sys.stderr)
			sys.exit(1)
	else:
		print('Usage: python3 download.py url localdir', file=sys.stderr)
		sys.exit(2)
