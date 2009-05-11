# Generates Windows resource header.

import os, sys
import uuid
from hashlib import md5
from optparse import OptionParser
import packagewindows

indentSize = 2

excludedDirectories = [".svn", "CVS"]
excludedFiles = ["node.mk"]

# Bit of a hack, but it works
def isParentDir(child, parent):
    return not ".." in os.path.relpath(child, parent)

def makeFileId(guid):
    return "file_" + guid

def makeDirectoryId(guid):
    return "directory_" + guid

def makeComponentId(guid):
    return "component_" + guid
    
def newGuid():
    return (str)(uuid.uuid4()).replace("-","")

class WixFragment:

    def __init__(self, fileGenerator, componentGroup, directoryRef, virtualDir, excludedFile, win64):
        self.fileGenerator = fileGenerator
        self.componentGroup = componentGroup
        self.directoryRef = directoryRef
        self.virtualDir = virtualDir
        self.indent = 0
        if win64:
			self.win64 = 'yes'
        else:
			self.win64 = 'no'

        if excludedFile:
            excludedFiles.append(excludedFile)

    def incrementIndent(self):
        self.indent += indentSize
        
    def decrementIndent(self):
        self.indent -= indentSize

    def getIndent(self):
        for i in range(0, self.indent):
            yield " "
            
    def printIndented(self, line):
        for space in self.getIndent():
            line = space + line
        yield line

    def startWix(self):
        for line in self.printIndented("<?xml version=\"1.0\" encoding=\"utf-8\"?>"): yield line
        for line in self.printIndented("<Wix xmlns=\"http://schemas.microsoft.com/wix/2006/wi\">"): yield line
        self.incrementIndent()

    def endWix(self):
        self.decrementIndent()
        for line in self.printIndented("</Wix>"): yield line
        
    def startFragment(self):
        for line in self.printIndented("<Fragment>"): yield line
        self.incrementIndent()

    def endFragment(self):
        self.decrementIndent()
        for line in self.printIndented("</Fragment>"): yield line

    def startDirectoryRef(self, id):
        for line in self.printIndented("<DirectoryRef Id=\"" + id + "\">"): yield line
        self.incrementIndent()

    def endDirectoryRef(self):
        self.decrementIndent()
        for line in self.printIndented("</DirectoryRef>"): yield line
        
    def startComponentGroup(self, id):
        for line in self.printIndented("<ComponentGroup Id=\"" + id + "\">"): yield line
        self.incrementIndent()

    def endComponentGroup(self):
        self.decrementIndent()
        for line in self.printIndented("</ComponentGroup>"): yield line
        
    def startComponentRef(self, id):
        for line in self.printIndented("<ComponentRef Id=\"" + id + "\">"): yield line
        self.incrementIndent()

    def endComponentRef(self):
        self.decrementIndent()
        for line in self.printIndented("</ComponentRef>"): yield line

    def startDirectory(self, directoryId, directory):
        for line in self.printIndented("<Directory Id=\"" + directoryId + "\" Name=\"" + directory + "\">"): yield line
        self.incrementIndent()

    def endDirectory(self):
        self.decrementIndent()
        for line in self.printIndented("</Directory>"): yield line

    def startComponent(self, componentId, guid):
		component = "<Component Id=\"" + componentId + "\" Guid=\"" + guid + "\" DiskId=\"1\""
		component += " Win64=\"" + self.win64 + "\""
		component += ">"
		for line in self.printIndented(component): yield line
		self.incrementIndent()
        
    def endComponent(self):
        self.decrementIndent()
        for line in self.printIndented("</Component>"): yield line

    def startFile(self, fileId, fileName, sourcePath):
        for line in self.printIndented("<File Id=\"" + fileId + "\" Name=\"" + fileName + "\" Source=\"" + sourcePath + "\">"): yield line
        self.incrementIndent()
        
    def endFile(self):
        self.decrementIndent()
        for line in self.printIndented("</File>"): yield line

    def yieldFragment(self):
    
        # List that stores the components we've added
        components = list()
        
        # Stack that stores directories we've already visited
        stack = list()
        
        # List that stores the virtual directories we added
        virtualstack = list()

        for line in self.startWix(): yield line
        for line in self.startFragment(): yield line
        for line in self.startDirectoryRef(self.directoryRef): yield line

        # Add virtual directories
        if self.virtualDir:
            head = self.virtualDir
            while head:
                joinedPath = head
                head, tail = os.path.split(head)
                virtualstack.insert(0, joinedPath)
            for dir in virtualstack:
                componentId = "directory_" + (str)(uuid.uuid4()).replace("-", "_")
                for line in self.startDirectory(componentId, os.path.basename(dir)): yield line

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
                        for line in self.endDirectory(): yield line

                # Enter new directory
                stack.append(dirpath)
                for line in self.startDirectory(makeDirectoryId(newGuid()), os.path.basename(dirpath)): yield line
            
            if filenames:
                # Remove excluded files
                for exclusion in excludedFiles:
                    if exclusion in filenames:
                        filenames.remove(exclusion)
 
                # Process files
                for file in filenames:
                    guid = newGuid()
                    componentId = makeComponentId(guid)
                    sourcePath = os.path.join(dirpath, file)

                    for line in self.startComponent(componentId, guid): yield line
                    for line in self.startFile(makeFileId(guid), file, sourcePath): yield line
                    for line in self.endFile(): yield line
                    
                    for line in self.endComponent(): yield line
                    components.append(componentId)
            
        # Drain pushed physical directories
        while stack:
            popped = stack.pop()
            for line in self.endDirectory(): yield line
                
        # Drain pushed virtual directories
        while virtualstack:
            popped = virtualstack.pop()
            for line in self.endDirectory(): yield line

        for line in self.endDirectoryRef(): yield line
        for line in self.endFragment(): yield line

        # Emit ComponentGroup
        for line in self.startFragment(): yield line
        for line in self.startComponentGroup(self.componentGroup): yield line
        for component in components:
            for line in self.startComponentRef(component): yield line
            for line in self.endComponentRef(): yield line
        for line in self.endComponentGroup(): yield line
        for line in self.endFragment(): yield line

        for line in self.endWix(): yield line

def GenerateWixFragment(sourcePath, componentGroup, directoryRef, virtualDir, excludedFile, win64):
	fileGenerator = packagewindows.WalkPath(sourcePath)
	wf = WixFragment(fileGenerator, componentGroup, directoryRef, virtualDir, excludedFile, win64)
	for line in wf.yieldFragment(): yield line

if __name__ == '__main__':
	parser = OptionParser()
	parser.add_option("-c", "--componentGroup", type="string", dest="componentGroup")
	parser.add_option("-r", "--directoryRef", type="string", dest="directoryRef")
	parser.add_option("-s", "--sourcePath", type="string", dest="sourcePath")
	parser.add_option("-v", "--virtualDir", type="string", dest="virtualDir")
	parser.add_option("-x", "--excludedFile", type="string", dest="excludedFile")
	parser.add_option("-w", "--win64", type="string", dest="win64")
	(options, args) = parser.parse_args()

	for line in GenerateWixFragment(options.sourcePath, options.componentGroup, options.directoryRef, options.virtualDir, options.excludedFile, options.win64):
		print line