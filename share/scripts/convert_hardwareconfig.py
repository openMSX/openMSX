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
	#       Memory mapper IO ports are hardcoded in openMSX (because they are
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

def hexFill(number, size):
	return ('0x%0' + str(size) + 'X') % number

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

def extractElements(node, names):
	ret = []
	for child in list(node.childNodes):
		if child.nodeType == node.ELEMENT_NODE:
			if child.nodeName in names:
				node.removeChild(child)
				ret.append(child)
	return ret

def convertDoc(dom):
	assert dom.nodeType == dom.DOCUMENT_NODE
	if dom.doctype.systemId == 'msxconfig2.dtd':
		print '  already in new format, skipping'
		return
	elif dom.doctype.systemId != 'msxconfig.dtd':
		print '  NOTE: unknown format, skipping'
		return
	dom.doctype.systemId = 'msxconfig2.dtd'

	index = 0

	def isCVSId(node):
		if node.nodeType != dom.COMMENT_NODE: return False
		if not node.nodeValue.startswith(' $Id'): return False
		if not node.nodeValue.endswith('$ '): return False
		return True
	if isCVSId(dom.childNodes[index]):
		index += 1
	else:
		print '  NOTE: no CVS id'
	
	def checkDocType(node):
		if node.nodeType != dom.DOCUMENT_TYPE_NODE: return False
		if node.nodeName != 'msxconfig': return False
		return True
	if checkDocType(dom.childNodes[index]):
		index += 1
	else:
		print '  NOTE: unknown document type, skipping'
		return
	
	def checkRoot(node):
		if node.nodeType != dom.ELEMENT_NODE: return False
		if node.nodeName != 'msxconfig': return False
		return True
	if len(dom.childNodes) - 1 != index:
		print '  NOTE: unexpected top-level node(s)'
		return
	elif checkRoot(dom.childNodes[index]):
		convertRoot(dom.childNodes[index])
	else:
		print '  NOTE: wrong root node, skipping'
		return

def convertRoot(node):
	doc = node.ownerDocument

	# Process everything except devices.
	externalSlotsNode = None
	motherBoardNode = None
	for child in list(node.childNodes):
		if child.nodeType == node.ELEMENT_NODE:
			if child.nodeName == 'config':
				child = convertConfig(child)
				if child is not None:
					if child.nodeName == 'ExternalSlots':
						externalSlotsNode = child
						node.removeChild(child)
					elif child.nodeName == 'MotherBoard':
						motherBoardNode = child
						node.removeChild(child)
			elif child.nodeName not in ['device', 'info']:
				print '  NOTE: ignoring new-style element at top level:', \
					child.nodeName
		elif child.nodeType == node.TEXT_NODE:
			pass
		elif child.nodeType == node.COMMENT_NODE:
			pass
		else:
			print 'cannot handle node type at top level:', child
			assert False

	# Determine which slots are expanded.
	slotExpanded = [ None ] * 4
	if motherBoardNode is not None:
		for child in motherBoardNode.childNodes:
			if child.nodeType == node.ELEMENT_NODE:
				assert child.nodeName == 'slot'
				num = int(child.attributes['num'].nodeValue)
				assert 0 <= num < 4
				expanded = bool([ 'false', 'true' ].index(
					child.attributes['expanded'].nodeValue
					))
				slotExpanded[num] = expanded
		motherBoardNode.unlink()

	# Determine which slots are external.
	externalSlots = {}
	if externalSlotsNode is not None:
		for child in externalSlotsNode.childNodes:
			if child.nodeType == node.ELEMENT_NODE:
				assert child.nodeName.startswith('slot')
				slot = map(int, getText(child).split('-'))
				assert len(slot) == 2
				externalSlots[tuple(slot)] = None

	# Process devices.
	devicesNode = doc.createElement('devices')
	node.appendChild(devicesNode)
	slottedDevices = {}
	for child in list(node.childNodes):
		if child.nodeType == node.ELEMENT_NODE and child.nodeName == 'device':
			slotted, child = convertDevice(child)
			if child is not None:
				node.removeChild(child)
				# Insert slot mapping, if any.
				if slotted == []:
					devicesNode.appendChild(child)
				else:
					primary, secondary, page = slotted[0]
					pages = [ False ] * 4
					for ps, ss, pg in slotted:
						assert primary == ps and secondary == ss
						assert -1 <= ps < 4
						if ps == -1:
							assert ss == -1
						elif slotExpanded[ps]:
							assert 0 <= ss < 4
						else:
							assert ss == 0
						assert 0 <= pg < 4
						pages[pg] = True
					devices = slottedDevices.setdefault(
						(primary, secondary), [] )
					devices.append((child, pages))
	def convertSlotted(parentNode, primary, secondary = 0):
		if (primary, secondary) in externalSlots:
			assert primary != -1
			print '    external'
			del externalSlots[(primary, secondary)]
			parentNode.setAttribute('external', 'true')
		else:
			devices = slottedDevices.get((primary, secondary))
			if devices is None:
				return
			del slottedDevices[primary, secondary]
			for deviceNode, pages in devices:
				page = 0
				while page < 4:
					# Skip empty pages.
					while page < 4 and not pages[page]:
						page += 1
					if page == 4: break
					# Iterate through non-empty pages.
					start = page
					while page < 4 and pages[page]:
						page += 1
					if page == start + 1:
						print '    device "%s" in page %d' % (
							deviceNode.tagName, start )
					else:
						print '    device "%s" in pages %d-%d' % (
							deviceNode.tagName, start, page - 1 )
					memNode = parentNode.ownerDocument.createElement('mem')
					memNode.setAttribute(
						'base', hexFill(start * 0x4000, 4) )
					memNode.setAttribute(
						'size', hexFill((page - start) * 0x4000, 4) )
					deviceNode.appendChild(memNode)
					parentNode.appendChild(deviceNode)
	if (-1, -1) in slottedDevices:
		print '  cartridge devices:'
		primaryNode = doc.createElement('primary')
		primaryNode.setAttribute('slot', 'any')
		devicesNode.appendChild(primaryNode)
		secondaryNode = doc.createElement('secondary')
		secondaryNode.setAttribute('slot', 'any')
		primaryNode.appendChild(secondaryNode)
		convertSlotted(secondaryNode, primary, secondary)
		if slottedDevices != {}:
			print '  NOTE: file contains both fixed and variable slots'
	if slottedDevices != {}:
		for primary in range(4):
			primaryNode = doc.createElement('primary')
			primaryNode.setAttribute('slot', str(primary))
			devicesNode.appendChild(primaryNode)
			if slotExpanded[primary]:
				for secondary in range(4):
					print '  slot %d.%d:' % (primary, secondary)
					secondaryNode = doc.createElement('secondary')
					secondaryNode.setAttribute('slot', str(secondary))
					primaryNode.appendChild(secondaryNode)
					convertSlotted(secondaryNode, primary, secondary)
			else:
				print '  slot %d:' % primary
				convertSlotted(primaryNode, primary)
	leftoverDevices = list(slottedDevices.items())
	leftoverDevices.sort()
	for slot, deviceNode in leftoverDevices:
		print '  NOTE: skipping device "%s" in unavailable ' \
			'slot %d.%d page %d' % (
			deviceNode.tagName, slot[0], slot[1], slot[2]
			)
	leftoverExternalSlots = list(externalSlots)
	leftoverExternalSlots.sort()
	for primary, secondary in leftoverExternalSlots:
		print '  NOTE: skipping unavailable external slot %d.%d' % (
			primary, secondary
			)

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
		return None
	elif configId not in [
		'CassettePort', 'ExternalSlots', 'MotherBoard', 'RenShaTurbo'
		]:
		print '    NOTE: conversion of config type "%s" may be wrong' \
			% configId
		# TODO: CassettePort: if only whitespace text nodes, then remove them
		# TODO: myHD: conversion of IDE HD configuration is not implemented
	return node

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
		return None, None
	
	# Convert parameters.
	slotted = []
	drives = 0
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
				elif newName == 'slotted':
					slotted.append(parseSlotted(child))
					newName = None
				elif newName is not None and newName.startswith('drivename'):
					drives += 1
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

	# Insert <drives> tag to replace "drivename" parameters.
	if drives != 0:
		print '    inserting drive tag'
		drivesNode = node.ownerDocument.createElement('drives')
		numNode = node.ownerDocument.createTextNode(str(drives))
		drivesNode.appendChild(numNode)
		node.appendChild(drivesNode)

	# Bundle generic ROM parameters in <rom> tag.
	if deviceType != 'DebugDevice':
		romParams = extractElements(node, ['filename'])
		if romParams != []:
			print '    grouping rom parameters'
			romNode = node.ownerDocument.createElement('rom')
			for param in romParams:
				romNode.appendChild(param)
			node.appendChild(romNode)

	# Bundle generic sound parameters in <sound> tag.
	soundParams = extractElements(node, ['mode', 'volume'])
	if soundParams != []:
		print '    grouping sound parameters'
		soundNode = node.ownerDocument.createElement('sound')
		for param in soundParams:
			soundNode.appendChild(param)
		node.appendChild(soundNode)

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
				ioNode.setAttribute('base', hexFill(base, 2))
				ioNode.setAttribute('num', str(num))
				if direction != 'IO':
					ioNode.setAttribute('type', direction)
				node.appendChild(ioNode)

	return slotted, node

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
		newNode = node.ownerDocument.createElement('slot')
		newNode.setAttribute('num', name)
		newNode.setAttribute('expanded', value)
		node.parentNode.replaceChild(newNode, node)
		node.unlink()
		return newNode

def parseSlotted(node):
	primary, secondary, page = None, None, None
	for child in list(node.childNodes):
		if child.nodeType == node.ELEMENT_NODE:
			if child.nodeName == 'ps':
				primary = int(getText(child))
			elif child.nodeName == 'ss':
				secondary = int(getText(child))
			elif child.nodeName == 'page':
				page = int(getText(child))
	return primary, secondary, page

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
		if not os.path.isdir(outpath):
			os.makedirs(outpath)
		out = open(outpath + '/hardwareconfig.xml', 'w')
		dom.writexml(out)
		out.close()
		dom.unlink()
