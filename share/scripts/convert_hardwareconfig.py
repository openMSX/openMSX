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
	'RTC': [ (0xB4, 2, 'IO') ],
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
	'VDP2': [ (0x98, 4, 'IO') ], # V9938/V9958
	'V9990': [ (0x60, 16, 'IO') ],
	
	# Audio:
	'PSG': [ (0xA0, 2, 'O'), (0xA2, 1, 'I') ],
	'MSX-MUSIC': [ (0x7C, 2, 'O') ],
	'FMPAC': [ (0x7C, 2, 'O') ],
	'MSX-AUDIO': [ (0xC0, 2, 'IO') ],
	'MSX-AUDIO MIDI': [ (0x00, 2, 'O'), (0x04, 2, 'I') ],
	'MoonSound': [ (0x7E, 2, 'IO'), (0xC4, 4, 'IO') ],
	'MSX-MIDI': [ (0xE8, 8, 'IO') ],
	'TurboRPCM': [ (0xA4, 2, 'IO') ],
	
	# Extensions:
	'MSX-RS232': [ (0x80, 8, 'IO') ],
	'MegaRam': [ (0x8E, 1, 'IO') ],
	'HBI55': [ (0xB0, 4, 'IO') ],
	'DebugDevice': [ (0x2E, 2, 'O') ], # can be any (free) port
	
	# Devices without I/O addresses:
	'ROM': [],
	'RAM': [],
	'PanasonicRAM': [],
	'Bunsetsu': [],
	'SunriseIDE': [],
	'PAC': [],
	'SCC+': [], # TODO: Why "Cart"?
	# TODO: Microsol FDC uses IO ports (0xd0 - 0xd4  5(!) ports), but I doubt
	#       this FDC really works in openMSX.
	'FDC': [],
	
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

def convertDoc(dom):
	assert dom.nodeType == dom.DOCUMENT_NODE
	
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
	for child in node.childNodes:
		if child.nodeType == node.ELEMENT_NODE:
			if child.nodeName == 'config':
				convertConfig(child)
			elif child.nodeName == 'device':
				convertDevice(child)
			else:
				print 'cannot handle element at top level:', child.nodeName
				assert False
		elif child.nodeType == node.TEXT_NODE:
			pass
		elif child.nodeType == node.COMMENT_NODE:
			pass
		else:
			print 'cannot handle node type at top level:', child
			assert False

def convertConfig(node):
	print '  config:', node.attributes['id'].nodeValue
	for child in node.childNodes:
		if child.nodeType == node.ELEMENT_NODE:
			if child.nodeName == 'parameter':
				convertParameter(child)
		elif child.nodeType == node.TEXT_NODE:
			pass
		elif child.nodeType == node.COMMENT_NODE:
			pass
		else:
			print 'cannot handle node inside config:', child
			assert False

def convertDevice(node):
	print '  device:', node.attributes['id'].nodeValue
	deviceType = getParameter(node, 'type')
	assert deviceType is not None
	deviceType = {
		'FM-PAC': 'FMPAC',
		'Rom': 'ROM',
		'PanasonicRam': 'PanasonicRAM',
		'Music': 'MSX-MUSIC',
		'Audio': 'MSX-AUDIO',
		'Audio-Midi': 'MSX-AUDIO MIDI',
		'SCCPlusCart': 'SCC+',
		'MSX-Midi': 'MSX-MIDI',
		}.get(deviceType, deviceType)
	
	if deviceType == 'CPU':
		print '    removing CPU device'
		node.parentNode.removeChild(node)
		node.unlink()
		return
	
	# Convert parameters.
	for child in node.childNodes:
		if child.nodeType == node.ELEMENT_NODE:
			if child.nodeName == 'parameter':
				convertParameter(child)
			elif child.nodeName == 'type':
				oldType = getText(child)
				if oldType != deviceType:
					print '    convert "' + oldType + '" ' \
						'to "' + deviceType + '"'
					child.childNodes[0].nodeValue = deviceType
		elif child.nodeType == node.TEXT_NODE:
			pass
		elif child.nodeType == node.COMMENT_NODE:
			pass
		else:
			print 'cannot handle node inside config:', child
			assert False
	
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

def convertParameter(node):
	name = node.attributes['name'].nodeValue
	if len(node.attributes) == 1:
		parent = node.parentNode
		deviceType = getParameter(parent, 'type')
		# Extract info from old-style node.
		if name == 'type' and deviceType == 'FDC':
			name = 'fdc_type'
		print '    convert parameter:', name
		assert len(node.childNodes) == 1
		text = node.childNodes[0]
		node.removeChild(text)
		# Store info in new-style node.
		elem = node.ownerDocument.createElement(name)
		elem.appendChild(text)
		# Replace old node by new node.
		parent.replaceChild(elem, node)
		node.unlink()
	else:
		assert len(node.attributes) == 2
		clazz = node.attributes['class'].nodeValue
		assert clazz == 'subslotted', clazz
		print '    convert parameter:', clazz, name
		assert len(node.childNodes) == 1
		value = node.childNodes[0].nodeValue
		assert value in [ 'true', 'false' ], value
		# Store info in new-style node.
		elem = node.ownerDocument.createElement('slot')
		elem.setAttribute('num', name)
		elem.setAttribute('expanded', value)
		# Replace old node by new node.
		parent = node.parentNode
		parent.replaceChild(elem, node)
		node.unlink()
	#	print 'param:', node.attributes['name'].nodeValue
	#	# Fails when value is integer (Python 2.3).
	#	#for attrib in node.attributes:
	#	for index in range(node.attributes.length):
	#		attrib = node.attributes.item(index)
	#		print attrib.name, '=', attrib.nodeValue

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
