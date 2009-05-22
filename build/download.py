# $Id$

from os.path import basename, isdir
from urllib import urlretrieve
from urlparse import urlparse

import sys

class InteractiveStatusLine(object):
	length = 0

	def __call__(self, message, progress = False):
		sys.stdout.write(('\r%-' + str(self.length) + 's') % message)
		self.length = max(self.length, len(message))

class NoninteractiveStatusLine(object):

	def __call__(self, message, progress = False):
		if not progress:
			sys.stdout.write(message + '\n')

def downloadURL(url, localDir):
	if not isdir(localDir):
		raise IOError('Local directory "%s" does not exist' % localDir)

	fileName = basename(urlparse(url).path)
	prefix = 'Downloading %s: ' % fileName

	if sys.stdout.isatty():
		statusLine = InteractiveStatusLine()
	else:
		statusLine = NoninteractiveStatusLine()
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
		urlretrieve(url, localDir + '/' + fileName, reportProgress)
	except IOError:
		statusLine(prefix + 'FAILED.')
		raise
	else:
		statusLine(prefix + 'done.')
	finally:
		print

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