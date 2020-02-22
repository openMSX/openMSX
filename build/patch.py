# Applies a unified diff to a directory tree.

from io import open
from os.path import abspath, isdir, join as joinpath, sep
import re
import sys

class LineScanner(object):

	def __init__(self, stream, lineFilter = None):
		'''Scans the given stream.
		Any object providing readline() can be passed as stream, such as file
		objects. The line scanner does not close the stream.
		The optional line filter is a function that will be called for every
		line read from the stream; iff the filter returns False, the line is
		skipped. This can be useful for example for skipping comment lines.
		'''
		if not hasattr(stream, 'readline'):
			raise TypeError(
				'Invalid type for stream: %s' % type(stream).__name__
				)
		self.__stream = stream
		self.__filter = lineFilter
		self.__currLine = None
		self.__lineNo = 0
		self.nextLine()

	def end(self):
		'''Returns True iff the end of the stream has been reached.
		'''
		return self.__currLine is None

	def peek(self):
		'''Returns the current line.
		Like readline(), the returned string includes the newline characteter
		if it is present in the stream.
		'''
		return self.__currLine

	def nextLine(self):
		'''Moves on to the next line.
		Raises OSError if there is a problem reading from the stream.
		'''
		stream = self.__stream
		lineFilter = self.__filter

		while True:
			line = stream.readline()
			self.__lineNo += 1
			if line:
				if lineFilter is None or lineFilter(line):
					break
			else:
				line = None
				break
		self.__currLine = line

	def getLineNumber(self):
		'''Returns the line number of the current line.
		The first line read from the stream is considered line 1.
		'''
		return self.__lineNo

	def parseError(self, msg, lineNo = None):
		'''Returns a ParseError object with a descriptive message.
		Raising the exception is left to the caller, to make sure the backtrace
		is meaningful.
		If a line number is given, that line number is used in the message,
		otherwise the current line number is used.
		'''
		stream = self.__stream
		if lineNo is None:
			lineNo = self.getLineNumber()
		return ParseError(
			lineNo,
			'Error parsing %s at line %d: %s' % (
				'"%s"' % stream.name if hasattr(stream, 'name') else 'stream',
				lineNo,
				msg
				)
			)

class ParseError(Exception):

	def __init__(self, lineNo, msg):
		Exception.__init__(self, msg)
		self.lineNo = lineNo

class _Change(object):
	oldInc = None
	newInc = None
	action = None

	def __init__(self, content):
		self.content = content

	def __str__(self):
		return self.action + self.content.rstrip()

class _Context(_Change):
	oldInc = 1
	newInc = 1
	action = ' '

class _Add(_Change):
	oldInc = 0
	newInc = 1
	action = '+'

class _Remove(_Change):
	oldInc = 1
	newInc = 0
	action = '-'

class Hunk(object):
	'''Contains the differences between two versions of a single section within
	a file.
	'''
	reDeltaLine = re.compile(r'@@ -(\d+),(\d+) \+(\d+),(\d+) @@')
	changeClasses = dict( (cc.action, cc) for cc in (_Context, _Add, _Remove) )

	@classmethod
	def parse(cls, scanner):
		delta = cls.reDeltaLine.match(scanner.peek())
		if delta is None:
			raise scanner.parseError('invalid delta line')
		oldLine, oldLen, newLine, newLen = ( int(n) for n in delta.groups() )
		deltaLineNo = scanner.getLineNumber()
		scanner.nextLine()
		def lineCountMismatch(oldOrNew, declared, actual):
			return scanner.parseError(
				'hunk %s size mismatch: %d declared, %d actual' % (
					oldOrNew, declared, actual
					),
				deltaLineNo
				)
		def iterChanges():
			oldCount = 0
			newCount = 0
			while not scanner.end():
				line = scanner.peek()
				if (line.startswith('---') or line.startswith('+++')) \
						and oldCount == oldLen and newCount == newLen:
					# Hunks for a new file start here.
					# Since a change line could start with "---" or "+++"
					# as well we also have to check whether we are at the
					# declared end of this hunk.
					break
				if len(line) == 1:
					# Assume this is an empty context line that had its single
					# space character removed.
					line = ' '
				changeClass = cls.changeClasses.get(line[0])
				if changeClass is None:
					break
				content = line[1 : ]
				scanner.nextLine()
				if not scanner.end() and scanner.peek().startswith('\\'):
					# No newline at end of file.
					assert content[-1] == '\n'
					content = content[ : -1]
					scanner.nextLine()
				change = changeClass(content)
				yield change
				oldCount += change.oldInc
				newCount += change.newInc
			if oldCount != oldLen:
				raise lineCountMismatch('old', oldLen, oldCount)
			if newCount != newLen:
				raise lineCountMismatch('new', newLen, newCount)
		return cls(oldLine, newLine, iterChanges())

	def __init__(self, oldLine, newLine, changes):
		self.__oldLine = oldLine
		self.__newLine = newLine
		self.__changes = tuple(changes)

	def __str__(self):
		return 'hunk(old=%d,new=%d)' % (self.__oldLine, self.__newLine)

	def getOldLine(self):
		return self.__oldLine

	def getNewLine(self):
		return self.__newLine

	def iterChanges(self):
		return iter(self.__changes)

class Diff(object):
	'''Contains the differences between two versions of a single file.
	'''

	@classmethod
	def load(cls, path):
		'''Iterates through the differences contained in the given diff file.
		Only the unified diff format is supported.
		Each element returned is a Diff object containing the differences to
		a single file.
		'''
		with open(path, 'r', encoding='utf-8') as inp:
			scanner = LineScanner(inp, lambda line: not line.startswith('#'))
			error = scanner.parseError
			def parseHunks():
				while not scanner.end():
					if scanner.peek().startswith('@@'):
						yield Hunk.parse(scanner)
					else:
						break
			while not scanner.end():
				if scanner.peek().startswith('diff '):
					scanner.nextLine()
				diffLineNo = scanner.getLineNumber()
				if not scanner.peek().startswith('--- '):
					raise error('"---" expected (not a unified diff?)')
				scanner.nextLine()
				newFileLine = scanner.peek()
				if not newFileLine.startswith('+++ '):
					raise error('"+++" expected (not a unified diff?)')
				index = 4
				length = len(newFileLine)
				while index < length and not newFileLine[index].isspace():
					index += 1
				filePath = newFileLine[4 : index]
				scanner.nextLine()
				try:
					yield cls(filePath, parseHunks())
				except ValueError as ex:
					raise error('inconsistent hunks: %s' % ex, diffLineNo)

	def __init__(self, path, hunks):
		self.__path = path
		self.__hunks = sorted(hunks, key = lambda hunk: hunk.getOldLine())

		# Sanity check on line numbers.
		offset = 0
		for hunk in self.__hunks:
			declaredOffset = hunk.getNewLine() - hunk.getOldLine()
			if offset != declaredOffset:
				raise ValueError(
					'hunk offset mismatch: %d declared, %d actual' % (
						declaredOffset, offset
						)
					)
			offset += sum(
				change.newInc - change.oldInc for change in hunk.iterChanges()
				)

	def __str__(self):
		return 'diff of %d hunks to "%s"' % (len(self.__hunks), self.__path)

	def getPath(self):
		return self.__path

	def iterHunks(self):
		'''Iterates through the hunks in this diff in order of increasing
		old line numbers.
		'''
		return iter(self.__hunks)

def patch(diff, targetDir):
	'''Applies the given Diff to the given target directory.
	No fuzzy matching is done: if the diff does not apply exactly, ValueError
	is raised.
	'''
	absTargetDir = abspath(targetDir) + sep
	absFilePath = abspath(joinpath(absTargetDir, diff.getPath()))
	if not absFilePath.startswith(absTargetDir):
		raise ValueError(
			'Refusing to patch file "%s" outside destination directory'
			% diff.getPath()
			)

	# Read entire file into memory.
	# The largest file we expect to patch is the "configure" script, which is
	# typically about 1MB.
	with open(absFilePath, 'r', encoding='utf-8') as inp:
		lines = inp.readlines()

	for hunk in diff.iterHunks():
		# We will be modifying "lines" at index "newLine", while "oldLine" is
		# only used to produce error messages if there is a context or remove
		# mismatch. As a result, "newLine" is 0-based and "oldLine" is 1-based.
		oldLine = hunk.getOldLine()
		newLine = hunk.getNewLine() - 1
		for change in hunk.iterChanges():
			if change.oldInc == 0:
				lines.insert(newLine, change.content)
			else:
				assert change.oldInc == 1
				if change.content.rstrip() != lines[newLine].rstrip():
					raise ValueError('mismatch at line %d' % oldLine)
				if change.newInc == 0:
					del lines[newLine]
			oldLine += change.oldInc
			newLine += change.newInc

	with open(absFilePath, 'w', encoding='utf-8') as out:
		out.writelines(lines)

def main(diffPath, targetDir):
	try:
		differences = list(Diff.load(diffPath))
	except OSError as ex:
		print('Error reading diff:', ex, file=sys.stderr)
		sys.exit(1)
	except ParseError as ex:
		print(ex, file=sys.stderr)
		sys.exit(1)

	if not isdir(targetDir):
		print('Destination directory "%s" does not exist' % targetDir, file=sys.stderr)
		sys.exit(1)
	for diff in differences:
		targetPath = joinpath(targetDir, diff.getPath())
		try:
			patch(diff, targetDir)
		except OSError as ex:
			print('I/O error patching "%s": %s' % (
				targetPath, ex
				), file=sys.stderr)
			sys.exit(1)
		except ValueError as ex:
			print('Patch could not be applied to "%s": %s' % (
				targetPath, ex
				), file=sys.stderr)
			sys.exit(1)
		else:
			print('Patched:', targetPath)

if __name__ == '__main__':
	if len(sys.argv) == 3:
		main(*sys.argv[1 : ])
	else:
		print('Usage: python3 patch.py diff target', file=sys.stderr)
		sys.exit(2)
