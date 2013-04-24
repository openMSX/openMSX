from os import remove, stat
from os.path import basename, isdir, isfile, join as joinpath
from urllib import FancyURLopener
from urlparse import urlparse

import sys

# FancyURLOpener, which is also used in urlretrieve(), does not raise
# an exception on status codes like 404 and 500. However, for downloading it is
# critical to write either what we requested or nothing at all.
class DownloadURLOpener(FancyURLopener):

	def http_error_default(self, url, fp, errcode, errmsg, headers):
		raise IOError('%s: http:%s' % (errmsg, url))

_urlOpener = DownloadURLOpener()

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
		raise IOError('Local directory "%s" does not exist' % localDir)

	fileName = basename(urlparse(url).path)
	localPath = joinpath(localDir, fileName)
	prefix = 'Downloading %s: ' % fileName

	statusLine = createStatusLine(sys.stdout)
	statusLine(prefix + 'contacting server...')

	def reportProgress(blocksDone, blockSize, totalSize):
		doneSize = blocksDone * blockSize
		statusLine(prefix + (
			'%d/%d bytes (%1.1f%%)...' % (
				doneSize, totalSize, (100.0 * doneSize) / totalSize
				)
			if totalSize > 0 else
			'%d bytes...' % doneSize
			), True)

	try:
		try:
			_urlOpener.retrieve(url, localPath, reportProgress)
		except IOError:
			statusLine(prefix + 'FAILED.')
			raise
		else:
			statusLine(prefix + 'done.')
		finally:
			print
	except:
		if isfile(localPath):
			statusLine(prefix + 'removing partial download.')
			remove(localPath)
		raise

if __name__ == '__main__':
	if len(sys.argv) == 3:
		try:
			downloadURL(*sys.argv[1 : ])
		except IOError, ex:
			print >> sys.stderr, ex
			sys.exit(1)
	else:
		print >> sys.stderr, \
			'Usage: python download.py url localdir'
		sys.exit(2)
