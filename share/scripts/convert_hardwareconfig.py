#!/usr/bin/env python
# $Id$
#
# Script for updating openMSX 0.4.0 configurations (hardwareconfig.xml)
# to the new format.

# TODO:
# - Clean up output.

import os, os.path, sys
from xml.dom.minidom import parse

ioAddresses = {
	# Generic internal:
	'PPI': [ (0xA8, 4, 'IO') ],
	'S1990': [ (0xE4, 2, 'IO') ],
	'F4Device': [ (0xF4, 1, 'IO') ],
	'E6Timer': [ (0xE6, 2, 'IO') ],
	'TurboRLeds': [ (0xA7, 1, 'O') ],
	'TurboRPause': [ (0xA7, 1, 'I') ],
	'RTC': [ (0xB4, 2, 'O'), (0xB5, 1, 'I') ],
	'PrinterPort': [ (0x90, 2, 'IO') ],
	# Kanji is actually a device with only 2 IO ports and only 128kb ROM
	# In case of 256kb the device is repeated on the next 2 IO port (and with
	# different ROM of course).
	# So we could simplify the code slightly if we treat it like this.
	'Kanji1': [ (0xD8, 2, 'O'), (0xD9, 1, 'I') ],
	'Kanji2': [ (0xDA, 2, 'O'), (0xDB, 1, 'I') ],
	# TODO: [ (0xFC, 4, 'IO') ],
	#       Memory mapper IO port are hardcoded in openMSX (because they are
	#       shared between diffent memory mappers).
	'MemoryMapper': [],
	
	# Video:
	'VDP1': [ (0x98, 2, 'IO') ], # TMSxxxx
	'VDP2': [ (0x98, 4, 'O'), (0x98, 2, 'I') ], # V9938/V9958
	'V9990': [ (0x60, 16, 'IO') ],
	
	# Audio:
	'PSG': [ (0xA0, 2, 'O'), (0xA2, 1, 'I') ],
	'MSX-MUSIC': [ (0x7C, 2, 'O') ],
	'FMPAC': [ (0x7C, 2, 'O') ],
	'MSX-AUDIO': [ (0xC0, 2, 'IO') ],
	'Music Module MIDI': [ (0x00, 2, 'O'), (0x04, 2, 'I') ],
	'MoonSound': [ (0x7E, 2, 'IO'), (0xC4, 4, 'IO') ],
	'MSX-MIDI': [ (0xE8, 8, 'IO') ],
	'TurboRPCM': [ (0xA4, 2, 'IO') ],
	
	# Extensions:
	'MSX-RS232': [ (0x80, 8, 'IO') ],
	'MegaRam': [ (0x8E, 1, 'IO') ],
	'HBI55': [ (0xB0, 4, 'IO') ],
	'DebugDevice': [ (0x2E, 2, 'O') ], # can be any (free) port

	# FDCs:
	'MB8877A': [],
	'TC8566AF': [],
	'WD2793': [],
	# TODO: Microsol FDC uses IO ports (0xd0 - 0xd4  5(!) ports), but I doubt
	#       this FDC really works in openMSX.
	'Microsol': [],

	# Devices without I/O addresses:
	'ROM': [],
	'RAM': [],
	'PanasonicRAM': [],
	'Bunsetsu': [],
	'SunriseIDE': [],
	'PAC': [],
	'SCC+': [],
	
	# Switched devices (share ports 0x40-0x4F):
	'Matsushita': [],
	'S1985': [],
	'Kanji12': [],
	}

def getText(node):
	assert len(node.childNodes) == 1
	textNode = node.childNodes[0]
	assert textNode.nodeType == node.TEXT_NODE
	return textNode.nodeValue

def getParameter(node, name):
	for child in node.childNodes:
		if child.nodeType == node.ELEMENT_NODE and child.nodeName == name:
			return getText(child)
	return None

def renameElement(node, name):
	newNode = node.ownerDocument.createElement(name)
	for index in range(node.attributes.length):
		attribute = node.attributes.item(index)
		newNode.setAttribute(attribute.name, attribute.nodeValue)
	for child in list(node.childNodes):
		node.removeChild(child)
		newNode.appendChild(child)
	node.parentNode.replaceChild(newNode, node)
	node.unlink()
	return newNode

def convertDoc(dom):
	assert dom.nodeType == dom.DOCUMENT_NODE
	if dom.doctype.systemId == 'msxconfig2.dtd':
		print '  already in new format, skipping'
		return
	elif dom.doctype.systemId != 'msxconfig.dtd':
		print '  unknown format, skipping'
		return
	dom.doctype.systemId = 'msxconfig2.dtd'

	def isCVSId(node):
		if node.nodeType != dom.COMMENT_NODE: return False
		if not node.nodeValue.startswith(' $Id'): return False
		if not node.nodeValue.endswith('$ '): return False
		#print 'found ID:', node.nodeValue
		return True
	if not isCVSId(dom.childNodes[0]):
		print 'missing CVS id'
		assert False, 'manually insert $''Id''$ or add that to converter'
	
	def checkDocType(node):
		if node.nodeType != dom.DOCUMENT_TYPE_NODE: return False
		if node.nodeName != 'msxconfig': return False
		return True
	if not checkDocType(dom.childNodes[1]):
		print 'wrong document type'
		assert False
	
	if len(dom.childNodes) != 3:
		print 'unexpected top-level node(s)'
		assert False
	
	def checkRoot(node):
		if node.nodeType != dom.ELEMENT_NODE: return False
		if node.nodeName != 'msxconfig': return False
		return True
	if not checkRoot(dom.childNodes[2]):
		print 'wrong root node'
		assert False
	
	convertRoot(dom.childNodes[2])

def convertRoot(node):
	for child in list(node.childNodes):
		if child.nodeType == node.ELEMENT_NODE:
			if child.nodeName == 'config':
				convertConfig(child)
			elif child.nodeName == 'device':
				convertDevice(child)
			elif child.nodeName != 'info':
				print '  NOTE: ignoring new-style element at top level:', \
					child.nodeName
		elif child.nodeType == node.TEXT_NODE:
			pass
		elif child.nodeType == node.COMMENT_NODE:
			pass
		else:
			print 'cannot handle node type at top level:', child
			assert False

def convertConfig(node):
	doc = node.ownerDocument
	configId = node.attributes['id'].nodeValue
	print '  config:', configId

	for child in list(node.childNodes):
		if child.nodeType == node.ELEMENT_NODE:
			if child.nodeName == 'parameter':
				child = convertParameter(child)
		elif child.nodeType == node.TEXT_NODE:
			pass
		elif child.nodeType == node.COMMENT_NODE:
			pass
		else:
			print 'cannot handle node inside config:', child
			assert False

	# Type is now stored in tag name instead of "id" attribute.
	node.removeAttribute('id')
	node = renameElement(node, configId)

	if configId == 'MapperType':
		# Store type in body text instead of "type" tag.
		newNode = doc.createElement(configId)
		newNode.appendChild(doc.createTextNode(getParameter(node, 'type')))
		node.parentNode.replaceChild(newNode, node)
		node.unlink()
	elif configId not in [
		'CassettePort', 'ExternalSlots', 'MotherBoard', 'RenShaTurbo'
		]:
		print '    NOTE: conversion of config type "%s" may be wrong' \
			% configId
		# TODO: CassettePort: if only whitespace text nodes, then remove them
		# TODO: MotherBoard: if slot is expanded but empty, it will disappear
		# TODO: myHD: conversion of IDE HD configuration is not implemented

def convertDevice(node):
	print '  device:', node.attributes['id'].nodeValue
	deviceType = getParameter(node, 'type')
	assert deviceType is not None
	newDeviceType = {
		'FM-PAC': 'FMPAC',
		'Rom': 'ROM',
		'PanasonicRam': 'PanasonicRAM',
		'Music': 'MSX-MUSIC',
		'Audio': 'MSX-AUDIO',
		'Audio-Midi': 'Music Module MIDI',
		'MSX-AUDIO MIDI': 'Music Module MIDI',
		'SCCPlusCart': 'SCC+',
		'MSX-Midi': 'MSX-MIDI',
		}.get(deviceType)
	if newDeviceType is not None:
		print '    rename type "' + deviceType + '" to "' + newDeviceType + '"'
		deviceType = newDeviceType

	if deviceType == 'CPU':
		print '    removing CPU device'
		node.parentNode.removeChild(node)
		node.unlink()
		return
	
	# Convert parameters.
	for child in list(node.childNodes):
		if child.nodeType == node.ELEMENT_NODE:
			if child.nodeName == 'type':
				node.removeChild(child)
				child.unlink()
			else:
				if child.nodeName == 'parameter':
					child = convertParameter(child)
				newName = getNewParameterName(deviceType, child.nodeName)
				if deviceType == 'FDC' and newName == 'fdc_type':
					deviceType = getText(child)
					newName = None
				if newName is None:
					print '    remove parameter "' + child.nodeName + '"'
					node.removeChild(child)
					child.unlink()
				elif newName != child.nodeName:
					print '    rename parameter "' + child.nodeName + '" ' \
						'to "' + newName + '"'
					child = renameElement(child, newName)
		elif child.nodeType == node.TEXT_NODE:
			pass
		elif child.nodeType == node.COMMENT_NODE:
			pass
		else:
			print 'cannot handle node inside device:', child
			assert False

	# Note: must happen here because fdc_type can rename node.
	node = renameElement(node, deviceType)

	# Insert I/O addresses.
	if deviceType == 'VDP':
		deviceType = {
			'TMS99X8A': 'VDP1', 'TMS9929A': 'VDP1',
			'V9938': 'VDP2', 'V9958': 'VDP2',
			}[getParameter(node, 'version')]
	elif deviceType == 'Kanji':
		print '    NOTE: Check whether machine has 2 Kanji devices'
		deviceType = 'Kanji1'
	ioList = ioAddresses.get(deviceType)
	if ioList is None:
		print 'no I/O list for device:', deviceType
		assert False
	else:
		if len(node.getElementsByTagName('io')) == 0:
			if len(ioList) != 0:
				print '    inserting I/O addresses'
			for base, num, direction in ioList:
				assert 0 <= base < 0x100
				assert num in [ 1, 2, 4, 8, 16]
				assert direction in ['I', 'O', 'IO']
				ioNode = node.ownerDocument.createElement('io')
				ioNode.setAttribute('base', hex(base))
				ioNode.setAttribute('num', str(num))
				if direction != 'IO':
					ioNode.setAttribute('type', direction)
				node.appendChild(ioNode)
				#print 'I/O node:', ioNode

def getNewParameterName(deviceType, name):
	return {
		'FDC': {
			'type': 'fdc_type',
			'brokenFDCread': 'broken_fdc_read',
			},
		'RTC': {
			'mode': None,
			},
		}.get(deviceType, {}).get(name, name)

def convertParameter(node):
	name = node.attributes['name'].nodeValue
	node.removeAttribute('name')
	if len(node.attributes) == 0:
		parent = node.parentNode
		deviceType = getParameter(parent, 'type')
		print '    convert parameter:', name
		assert len(node.childNodes) == 1
		return renameElement(node, name)
	else:
		assert len(node.attributes) == 1
		clazz = node.attributes['class'].nodeValue
		node.removeAttribute('class')
		assert clazz == 'subslotted', clazz
		print '    convert parameter:', clazz, name
		assert len(node.childNodes) == 1
		value = node.childNodes[0].nodeValue
		assert value in [ 'true', 'false' ], value
		node = renameElement(node, 'slot')
		node.setAttribute('num', name)
		node.setAttribute('expanded', value)
		return node

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
		
		dom = parse(root + '/hardwareconfig.xml')
		dom.normalize()
		convertDoc(dom)
		#print dom.toxml()
		if not os.path.isdir(outpath):
			os.makedirs(outpath)
		out = open(outpath + '/hardwareconfig.xml', 'w')
		dom.writexml(out)
		out.close()
		dom.unlink()
