#!/usr/bin/env python
# $Id$
#
# Script for updating openMSX 0.5.0 configurations (hardwareconfig.xml)
# to the new format used by CVS HEAD. Even though CVS HEAD is compatible
# with the 0.5.0 format (msxconfig2), the new format is preferred.
# If you run this script and see "NOTE:" in the output, please verify the
# created XML files manually.

import os, os.path, sys
from StringIO import StringIO
from xml.dom.minidom import parse

def convertDoc(dom, checksums):
	assert dom.nodeType == dom.DOCUMENT_NODE
	if dom.doctype.systemId == 'msxconfig.dtd':
		print '  old unsupported format, skipping'
		return
	elif dom.doctype.systemId != 'msxconfig2.dtd':
		print '  NOTE: unknown format, skipping'
		return

	sawCVSid = False
	for child in dom.childNodes:
		if child.nodeType == dom.COMMENT_NODE:
			if child.nodeValue.startswith(' $Id') \
			and child.nodeValue.endswith('$ '):
				sawCVSid = True
		elif child.nodeType == dom.DOCUMENT_TYPE_NODE:
			if child.nodeName != 'msxconfig':
				print '  NOTE: unknown document type "%s", skipping' % \
					child.nodeName
				return
		elif child.nodeType == dom.ELEMENT_NODE:
			if child.nodeName == 'msxconfig':
				convertRoot(child, checksums)
			else:
				print '  NOTE: wrong root node "%s", skipping' % child.nodeName
				return
	if not sawCVSid:
		print '  NOTE: no CVS id'

def convertRoot(node, checksums):
	def removeSlowDrainOnReset(memNode):
		for child in memNode.childNodes:
			if child.nodeType == node.ELEMENT_NODE \
			and child.nodeName == 'slow_drain_on_reset':
				print '  remove slow_drain_on_reset'
				memNode.removeChild(child)
				child.unlink()
				return
	for child in node.getElementsByTagName('RAM') \
			+ node.getElementsByTagName('MemoryMapper') \
			+ node.getElementsByTagName('PanasonicRAM'):
		removeSlowDrainOnReset(child)

def stripSpaces(node):
	if node.nodeType == node.TEXT_NODE:
		if node.nodeValue.isspace():
			node.parentNode.removeChild(node)
			node.unlink()
	else:
		for child in list(node.childNodes):
			stripSpaces(child)

def layoutDocument(dom):
	for child in dom.childNodes:
		if child.nodeType == dom.ELEMENT_NODE:
			layoutNode(child)

def layoutNode(node, indent = '', lineSpacing = 2):
	hasElements = False
	for child in list(node.childNodes):
		if child.nodeType == node.ELEMENT_NODE:
			if child.tagName == 'secondary': lineSpacing = 2
			hasElements = True
			node.insertBefore(
				child.ownerDocument.createTextNode(
					'\n' * lineSpacing + '  ' + indent
					),
				child
				)
			layoutNode(
				child,
				indent + '  ',
				1 + int(child.tagName in ['devices'])
				)
		elif child.nodeType == node.COMMENT_NODE:
			node.insertBefore(
				child.ownerDocument.createTextNode(
					'\n' * lineSpacing + '  ' + indent
					),
				child
				)
	if hasElements:
		node.appendChild(node.ownerDocument.createTextNode(
			'\n' * lineSpacing + indent
			))

if len(sys.argv) != 3:
	print 'Usage: convert_hardwareconfig.py <indir> <outdir>'
	sys.exit(2)
indir = sys.argv[1]
outdir = sys.argv[2]
if outdir[-1] != '/': outdir += '/'

for root, dirs, files in os.walk(indir):
	if 'hardwareconfig.xml' in files:
		print 'Converting ' + root.split('/')[-1] + '...'
		assert root.startswith(indir)
		outpath = outdir + root[len(indir) : ]
		
		checksums = {}
		if os.path.isdir(root + '/roms') \
		and os.path.isfile(root + '/roms/SHA1SUMS'):
			for line in open(root + '/roms/SHA1SUMS'):
				checksum, empty, fileName = line[ : -1].split(' ')
				assert empty == ''
				checksums[fileName] = checksum
		
		dom = parse(root + '/hardwareconfig.xml')
		dom.normalize()
		stripSpaces(dom)
		convertDoc(dom, checksums)
		layoutDocument(dom)
		if not os.path.isdir(outpath):
			os.makedirs(outpath)
		out = StringIO()
		dom.writexml(out)
		out.seek(0)
		outFile = open(outpath + '/hardwareconfig.xml', 'w')
		
		# Minidom doesn't seem to allow text nodes at document level,
		# so do some postprocessing using string manipulation.
		afterDoctype = False
		for line in out.readlines():
			if line.startswith('<!--'):
				index = 0
				while True:
					start = index
					index = line.find('-->', start)
					if index == -1:
						line = line[start : ]
						break
					index += 3
					outFile.write(line[start : index] + '\n')
			if line.startswith('<!DOCTYPE'):
				outFile.write(line[ : -1])
				afterDoctype = True
			elif afterDoctype:
				index = 0
				while line[index].isspace(): index += 1
				outFile.write(' ' + line[index : ])
				afterDoctype = False
			else:
				outFile.write(line)
		outFile.write('\n')
		
		outFile.close()
		out.close()
		dom.unlink()
