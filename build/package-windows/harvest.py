# Generates Windows resource header.

from optparse import OptionParser
from os import walk
from os.path import (
	basename, dirname, isfile, join as joinpath, relpath, split as splitpath
	)
from uuid import uuid4

indentSize = 2

excludedDirectories = ['.svn', 'CVS']
excludedFiles = ['node.mk']

# Bit of a hack, but it works
def isParentDir(child, parent):
	return '..' not in relpath(child, parent)

def makeFileId(guid):
	return 'file_' + guid

def makeDirectoryId(guid):
	return 'directory_' + guid

def makeComponentId(guid):
	return 'component_' + guid

def newGuid():
	return str(uuid4()).replace('-', '')

def walkPath(sourcePath):
	if isfile(sourcePath):
		yield dirname(sourcePath), [], [ basename(sourcePath) ]
	else:
		for dirpath, dirnames, filenames in walk(sourcePath):
			yield dirpath, dirnames, filenames

class WixFragment(object):

	def __init__(self, fileGenerator, componentGroup, directoryRef, virtualDir,
			excludedFile, win64):
		self.fileGenerator = fileGenerator
		self.componentGroup = componentGroup
		self.directoryRef = directoryRef
		self.virtualDir = virtualDir
		self.indentLevel = 0
		self.win64 = 'yes' if win64 else 'no'

		if excludedFile:
			# TODO: Modifying this global variable is an unexpected side effect.
			excludedFiles.append(excludedFile)

	def incrementIndent(self):
		self.indentLevel += indentSize

	def decrementIndent(self):
		self.indentLevel -= indentSize

	def indent(self, line):
		return ' ' * self.indentLevel + line

	def startElement(self, elementName, **args):
		line = self.indent(
			'<%s %s>' % (
				elementName,
				' '.join('%s="%s"' % item for item in args.items())
				)
			)
		self.incrementIndent()
		return line

	def endElement(self, elementName):
		self.decrementIndent()
		return self.indent('</%s>' % elementName)

	def yieldFragment(self):
		# List that stores the components we've added
		components = []

		# Stack that stores directories we've already visited
		stack = []

		# List that stores the virtual directories we added
		virtualstack = []

		yield self.indent(
			'<?xml version="1.0" encoding="utf-8"?>'
			)
		yield self.startElement(
			'Wix', xmlns = 'http://schemas.microsoft.com/wix/2006/wi'
			)
		yield self.startElement('Fragment')
		yield self.startElement('DirectoryRef', Id = self.directoryRef)

		# Add virtual directories
		if self.virtualDir:
			head = self.virtualDir
			while head:
				joinedPath = head
				head, tail_ = splitpath(head)
				virtualstack.insert(0, joinedPath)
			for path in virtualstack:
				componentId = 'directory_' + str(uuid4()).replace('-', '_')
				yield self.startElement(
					'Directory', Id = componentId, Name = basename(path)
					)

		# Walk the provided file list
		firstDir = True
		for dirpath, dirnames, filenames in self.fileGenerator:

			# Remove excluded sub-directories
			for exclusion in excludedDirectories:
				if exclusion in dirnames:
					dirnames.remove(exclusion)

			if firstDir:
				firstDir = False
			else:
				# Handle directory hierarchy appropriately
				while stack:
					popped = stack.pop()
					if isParentDir(dirpath, popped):
						stack.append(popped)
						break
					else:
						yield self.endElement('Directory')

				# Enter new directory
				stack.append(dirpath)
				yield self.startElement(
					'Directory', Id = makeDirectoryId(newGuid()),
					Name = basename(dirpath)
					)

			# Remove excluded files
			for exclusion in excludedFiles:
				if exclusion in filenames:
					filenames.remove(exclusion)

			# Process files
			for filename in filenames:
				guid = newGuid()
				componentId = makeComponentId(guid)
				sourcePath = joinpath(dirpath, filename)

				yield self.startElement(
					'Component', Id = componentId, Guid = guid,
					DiskId = '1', Win64 = self.win64
					)
				yield self.startElement(
					'File', Id = makeFileId(guid), Name = filename,
					Source = sourcePath
					)
				yield self.endElement('File')

				yield self.endElement('Component')
				components.append(componentId)

		# Drain pushed physical directories
		while stack:
			popped = stack.pop()
			yield self.endElement('Directory')

		# Drain pushed virtual directories
		while virtualstack:
			popped = virtualstack.pop()
			yield self.endElement('Directory')

		yield self.endElement('DirectoryRef')
		yield self.endElement('Fragment')

		# Emit ComponentGroup
		yield self.startElement('Fragment')
		yield self.startElement('ComponentGroup', Id = self.componentGroup)
		for component in components:
			yield self.startElement('ComponentRef', Id = component)
			yield self.endElement('ComponentRef')
		yield self.endElement('ComponentGroup')
		yield self.endElement('Fragment')

		yield self.endElement('Wix')

def generateWixFragment(
	sourcePath, componentGroup, directoryRef, virtualDir, excludedFile, win64
	):
	fileGenerator = walkPath(sourcePath)
	wf = WixFragment(
		fileGenerator, componentGroup, directoryRef, virtualDir, excludedFile,
		win64
		)
	return wf.yieldFragment()

def run():
	parser = OptionParser()
	parser.add_option(
		'-c', '--componentGroup', type = 'string', dest = 'componentGroup'
		)
	parser.add_option(
		'-r', '--directoryRef', type = 'string', dest = 'directoryRef'
		)
	parser.add_option(
		'-s', '--sourcePath', type = 'string', dest = 'sourcePath'
		)
	parser.add_option(
		'-v', '--virtualDir', type = 'string', dest = 'virtualDir'
		)
	parser.add_option(
		'-x', '--excludedFile', type = 'string', dest = 'excludedFile'
		)
	parser.add_option(
		'-w', '--win64', type = 'string', dest = 'win64'
		)
	options, args_ = parser.parse_args()

	for line in generateWixFragment(
		options.sourcePath, options.componentGroup, options.directoryRef,
		options.virtualDir, options.excludedFile, options.win64
		):
		print(line)

if __name__ == '__main__':
	run()
