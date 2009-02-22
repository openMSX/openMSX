# Generates Windows resource header.

import os, sys
import uuid
from hashlib import md5
from optparse import OptionParser

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

    def __init__(self, fileGenerator, componentGroup, directoryRef, virtualDir, excludedFile):
        self.fileGenerator = fileGenerator
        self.componentGroup = componentGroup
        self.directoryRef = directoryRef
        self.virtualDir = virtualDir
        self.indent = 0
        
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
        print line

    def startWix(self):
        self.printIndented("<?xml version=\"1.0\" encoding=\"utf-8\"?>")
        self.printIndented("<Wix xmlns=\"http://schemas.microsoft.com/wix/2006/wi\">")
        self.incrementIndent()

    def endWix(self):
        self.decrementIndent()
        self.printIndented("</Wix>")
        
    def startFragment(self):
        self.printIndented("<Fragment>")
        self.incrementIndent()

    def endFragment(self):
        self.decrementIndent()
        self.printIndented("</Fragment>")

    def startDirectoryRef(self, id):
        self.printIndented("<DirectoryRef Id=\"" + id + "\">")
        self.incrementIndent()

    def endDirectoryRef(self):
        self.decrementIndent()
        self.printIndented("</DirectoryRef>")
        
    def startComponentGroup(self, id):
        self.printIndented("<ComponentGroup Id=\"" + id + "\">")
        self.incrementIndent()

    def endComponentGroup(self):
        self.decrementIndent()
        self.printIndented("</ComponentGroup>")
        
    def startComponentRef(self, id):
        self.printIndented("<ComponentRef Id=\"" + id + "\">")
        self.incrementIndent()

    def endComponentRef(self):
        self.decrementIndent()
        self.printIndented("</ComponentRef>")

    def startDirectory(self, directoryId, directory):
        self.printIndented("<Directory Id=\"" + directoryId + "\" Name=\"" + directory + "\">")
        self.incrementIndent()

    def endDirectory(self):
        self.decrementIndent()
        self.printIndented("</Directory>")

    def startComponent(self, componentId, guid):
        self.printIndented("<Component Id=\"" + componentId + "\" Guid=\"" + guid + "\" DiskId=\"1\">")
        self.incrementIndent()
        
    def endComponent(self):
        self.decrementIndent()
        self.printIndented("</Component>")

    def startFile(self, fileId, fileName, sourcePath):
        self.printIndented("<File Id=\"" + fileId + "\" Name=\"" + fileName + "\" Source=\"" + sourcePath + "\">")
        self.incrementIndent()
        
    def endFile(self):
        self.decrementIndent()
        self.printIndented("</File>")

    def printFragment(self):
    
        # List that stores the components we've added
        components = list()
        
        # Stack that stores directories we've already visited
        stack = list()
        
        # List that stores the virtual directories we added
        virtualstack = list()

        self.startWix()
        self.startFragment()
        self.startDirectoryRef(self.directoryRef)

        # Add virtual directories
        if self.virtualDir:
            head = self.virtualDir
            while head:
                joinedPath = head
                head, tail = os.path.split(head)
                virtualstack.insert(0, joinedPath)
            for dir in virtualstack:
                componentId = "directory_" + (str)(uuid.uuid4()).replace("-", "_")
                self.startDirectory(componentId, os.path.basename(dir))

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
                        self.endDirectory()

                # Enter new directory
                stack.append(dirpath)
                self.startDirectory(makeDirectoryId(newGuid()), os.path.basename(dirpath))
            
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

                    self.startComponent(componentId, guid)
                    self.startFile(makeFileId(guid), file, sourcePath)
                    self.endFile()
                    
                    self.endComponent()
                    components.append(componentId)
            
        # Drain pushed physical directories
        while stack:
            popped = stack.pop()
            self.endDirectory()
                
        # Drain pushed virtual directories
        while virtualstack:
            popped = virtualstack.pop()
            self.endDirectory()

        self.endDirectoryRef()
        self.endFragment()

        # Emit ComponentGroup
        self.startFragment()
        self.startComponentGroup(self.componentGroup)
        for component in components:
            self.startComponentRef(component)
            self.endComponentRef()
        self.endComponentGroup()
        self.endFragment()

        self.endWix()


#
# This function is a temporary anticipation of mth providing generators from make install
#
def Walk(sourcePath):
    if os.path.isfile(sourcePath):
        filenames = list()
        filenames.append(os.path.basename(sourcePath))
        yield os.path.dirname(sourcePath), list(), filenames
    else:
        for dirpath, dirnames, filenames in os.walk(sourcePath):
            yield dirpath, dirnames, filenames

parser = OptionParser()
parser.add_option("-c", "--componentGroup", type="string", dest="componentGroup")
parser.add_option("-r", "--directoryRef", type="string", dest="directoryRef")
parser.add_option("-s", "--sourcePath", type="string", dest="sourcePath")
parser.add_option("-v", "--virtualDir", type="string", dest="virtualDir")
parser.add_option("-x", "--excludedFile", type="string", dest="excludedFile")

(options, args) = parser.parse_args()
fileGenerator = Walk(options.sourcePath)

#for dirpath, dirnames, filenames in fileGenerator:
#    print dirpath, dirnames, filenames
#    for f in filenames:
#        print f

wf = WixFragment(Walk(options.sourcePath), options.componentGroup, options.directoryRef, options.virtualDir, options.excludedFile)
wf.printFragment()