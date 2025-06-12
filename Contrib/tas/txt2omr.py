#!/usr/bin/env python3

from collections import defaultdict
from datetime import datetime
from gzip import GzipFile
from sys import stderr
from xml.etree.ElementTree import SubElement, parse as parseXML

import platform

cpuClock = 3579545
masterClock = cpuClock * 960

def readStates(topFilename):
	openFilenames = []
	base = None
	out = None
	scale = 1
	inputMap = {}
	inputStates = []
	time = 0

	def handleProcessingInstruction(words):
		if not words:
			raise ValueError('Missing processing instruction')
		elif words[0] == 'base':
			if len(words) != 2:
				raise ValueError(
						'Processing instruction "base" expects 1 argument, '
						'got %d' % (len(words) - 1))
			nonlocal base
			if base is not None:
				raise ValueError('Attempt to change base file name')
			base = words[1]
		elif words[0] == 'out':
			if len(words) != 2:
				raise ValueError(
						'Processing instruction "out" expects 1 argument, '
						'got %d' % (len(words) - 1))
			nonlocal out
			if out is not None:
				raise ValueError('Attempt to change output file name')
			out = words[1]
		elif words[0] == 'scale':
			if len(words) != 2:
				raise ValueError(
						'Processing instruction "scale" expects 1 argument, '
						'got %d' % (len(words) - 1))
			nonlocal scale
			scale = int(words[1])
			print('time scale: %.2f frames per second' % (masterClock / scale))
		elif words[0] == 'input':
			if len(words) != 5:
				raise ValueError(
						'Processing instruction "input" expects 4 arguments, '
						'got %d' % (len(words) - 1))
			name, inpType, rowStr, colStr = words[1:]
			if inpType != 'key':
				raise ValueError('Unknown input type "%s"' % inpType)
			row = int(rowStr)
			col = int(colStr)
			inputMap[name] = (row, col)
		elif words[0] == 'include':
			if len(words) != 2:
				raise ValueError(
						'Processing instruction "include" expects 1 argument, '
						'got %d' % (len(words) - 1))
			readFile(words[1])
		else:
			raise ValueError('Unknown processing instruction: %s' % words[0])

	def handleState(words):
		frames = int(words[0])
		try:
			inputs = [inputMap[name] for name in words[1:]]
		except KeyError as ex:
			raise ValueError('Undefined input: %s' % ex)
		nonlocal time
		inputStates.append((time, inputs))
		time += frames * scale
		return frames

	def readFile(filename):
		if filename in openFilenames:
			raise ValueError('Circular include: %s' % filename)
		localStateCount = 0
		localFrameCount = 0
		startTime = time
		openFilenames.append(filename)
		try:
			with open(filename, 'r') as stream:
				for lineNr, line in enumerate(stream, 1):
					try:
						line = line.strip()
						if not line or line[0] == '#':
							# Ignore empty line or comment.
							continue
						if line[0] == '=':
							handleProcessingInstruction(line[1:].split())
						else:
							localFrameCount += handleState(line.split())
							localStateCount += 1
					except ValueError:
						role = 'In' if filename == openFilenames[-1] \
								else 'included from'
						print('%s "%s" line %d,' % (role, filename, lineNr),
							file=stderr)
						raise
		except OSError as ex:
			print('Failed to open input file "%s",' % filename, file=stderr)
			raise ValueError(str(ex)) from ex
		del openFilenames[-1]
		localTime = time - startTime
		print('%-17s %5d states, %6d frames, %7.2f seconds' % (
				filename + ':', localStateCount, localFrameCount,
				localTime / masterClock
				), file=stderr)

	readFile(topFilename)
	inputStates.append((time, []))
	if base is None:
		raise ValueError('Base file not defined')
	return base, out, inputStates

def statesToEvents(inputStates):
	active = {}
	for time, state in inputStates:
		stateByRow = defaultdict(int)
		for row, col in state:
			stateByRow[row] |= 1 << col
		for row in sorted(active.keys() | stateByRow.keys()):
			old = active.get(row, 0)
			new = stateByRow.get(row, 0)
			press = new & ~old
			release = ~new & old
			if press != 0 or release != 0:
				yield time, row, press, release
			active[row] = new

def replaceEvents(inp, out, inputEvents):
	doc = parseXML(inp)

	# Set the serialization date to now.
	rootElem = doc.getroot()
	rootElem.attrib['date_time'] = \
			datetime.now().strftime('%a %b %d %H:%M:%S %Y')
	rootElem.attrib['openmsx_version'] = 'txt2omr'
	rootElem.attrib['platform'] = platform.system().lower()

	# Remove snapshots except the one at timestamp 0.
	snapshots = doc.find('replay/snapshots')
	if snapshots is None:
		print('Base replay lacks snapshots', file=stderr)
	else:
		seenInitialSnapshot = False
		for snapshot in snapshots.findall('item'):
			timeElem = snapshot.find('scheduler/currentTime/time')
			time = int(timeElem.text)
			if time == 0:
				seenInitialSnapshot = True
			else:
				snapshots.remove(snapshot)
		if not seenInitialSnapshot:
			print('No snapshot found with timestamp 0', file=stderr)

	# Replace event log.
	eventsElem = doc.find('replay/events')
	if eventsElem is None:
		print('No events tag found; cannot insert events', file=stderr)
	else:
		tail = eventsElem.tail
		eventsElem.clear()
		eventsElem.text = '\n'
		eventsElem.tail = tail

		# IDs must be unique for the entire document. We look for the highest
		# in-use ID and generate new IDs counting up from there.
		baseID = max(
			(
				int(id_val)
				for elem in doc.iterfind('.//*[@id]')
				if (id_val := elem.attrib['id']).isdecimal()
			),
			default=-1
		) + 1

		def createEvent(i, time):
			itemElem = SubElement(eventsElem, 'item',
					id=str(baseID + i), type='KeyMatrixState')
			itemElem.tail = '\n'
			stateChangeElem = SubElement(itemElem, 'StateChange')
			timeElem1 = SubElement(stateChangeElem, 'time')
			timeElem2 = SubElement(timeElem1, 'time')
			timeElem2.text = str(time)
			return itemElem

		for i, (time, row, press, release) in enumerate(inputEvents):
			itemElem = createEvent(i, time)
			SubElement(itemElem, 'row').text = str(row)
			SubElement(itemElem, 'press').text = str(press)
			SubElement(itemElem, 'release').text = str(release)
		endTime = inputEvents[-1][0] if inputEvents else 0
		createEvent(len(inputEvents), endTime).attrib['type'] = 'EndLog'

	# Reset re-record count.
	reRecordCount = doc.find('replay/reRecordCount')
	if reRecordCount is not None:
		reRecordCount.text = '0'

	# Reset the current time.
	currentTime = doc.find('replay/currentTime/time')
	if currentTime is not None:
		currentTime.text = '0'

	out.write(b'<?xml version="1.0" ?>\n')
	out.write(b"<!DOCTYPE openmsx-serialize SYSTEM 'openmsx-serialize.dtd'>\n")
	doc.write(out, encoding='utf-8', xml_declaration=False)

def convert(inFilename):
	try:
		base, outFilename, inputStates = readStates(inFilename)
	except ValueError as ex:
		print('ERROR: %s' % ex, file=stderr)
		exit(1)

	inputEvents = list(statesToEvents(inputStates))

	def createOutput(inp):
		if outFilename is None:
			print('No output file name set', file=stderr)
		else:
			try:
				with GzipFile(outFilename, 'wb') as out:
					replaceEvents(inp, out, inputEvents)
			except OSError as ex:
				print('Failed to open output replay:', ex)
				exit(1)
			else:
				print('wrote output replay:', outFilename)

	try:
		if base.endswith('.omr'):
			with GzipFile(base, 'rb') as inp:
				createOutput(inp)
		elif base.endswith('.xml'):
			with open(base, 'rb') as inp:
				createOutput(inp)
		else:
			print('Unknown base file type in "%s" '
					'(".xml" and ".omr" are supported)'
					% base, file=stderr)
			exit(1)
	except OSError as ex:
		print('Failed to open base replay:', ex, file=stderr)
		exit(1)

if __name__ == '__main__':
	from sys import argv
	if len(argv) != 2:
		print('Usage: txt2omr.py replay.txt', file=stderr)
		print('Converts the text version of an openMSX replay to an OMR file.',
				file=stderr)
		exit(2)
	else:
		convert(argv[1])
