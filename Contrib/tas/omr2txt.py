#!/usr/bin/env python3

from collections import Counter, defaultdict
from gzip import GzipFile
from math import floor
from sys import stderr
from xml.sax import make_parser
from xml.sax.handler import feature_external_ges
from xml.dom import pulldom

cpuClock = 3579545
vdpClock = cpuClock * 6
masterClock = cpuClock * 960

vdpTicksPerLine = 1368

# The keys we're interested in: the letter to be used in the text file,
# followed by the keyboard matrix location (row, column).
# This should probably be made configurable at some point, but for now you
# can edit them here.
inputMap = {
	'r': (8, 7), 'd': (8, 6), 'u': (8, 5), 'l': (8, 4), 's': (8, 0)
	}
# Coleco:
#inputMap = {
#	'r': (0, 1), 'd': (0, 2), 'u': (0, 0), 'l': (0, 3), 's': (0, 6), 'o': (2, 1)
#	}
inputMapReverse = dict((pos, name) for name, pos in inputMap.items())

def readEvents(filename):
	'''Reads input events from an openMSX replay file (OMR).
	Returns a list of tuples consisting of a timestamp, keyboard row, press
	mask and release mask.
	'''
	print('reading replay:', filename, file=stderr)

	inputEvents = []

	def getText(node):
		return ''.join(
			child.data
			for child in node.childNodes
			if child.nodeType == child.TEXT_NODE
			)

	def parseItem():
		time = None
		row = None
		press = None
		release = None
		for event, node in xmlEvents:
			if event == pulldom.START_ELEMENT:
				doc.expandNode(node)
				if node.tagName == 'StateChange':
					assert time is None, time
					timeNode1, timeNode2 = node.getElementsByTagName('time')
					time = int(getText(timeNode2))
				elif node.tagName == 'row':
					assert row is None, row
					row = int(getText(node))
				elif node.tagName == 'press':
					assert press is None, press
					press = int(getText(node))
				elif node.tagName == 'release':
					assert release is None, release
					release = int(getText(node))
				else:
					pass #print(node.toxml())
			elif event == pulldom.END_ELEMENT:
				if node.tagName == 'item':
					break

		assert time is not None
		assert row is not None
		assert press is not None
		assert release is not None
		inputEvents.append((time, row, press, release))

	def parseEvents():
		for event, node in xmlEvents:
			if event == pulldom.START_ELEMENT:
				if node.tagName == 'item' \
						and node.getAttribute('type') == 'KeyMatrixState':
					parseItem()
			elif event == pulldom.END_ELEMENT:
				if node.tagName == 'events':
					break

	def parseTopLevel():
		for event, node in xmlEvents:
			if event == pulldom.START_ELEMENT:
				if node.tagName == 'events':
					parseEvents()

	parser = make_parser()
	parser.setFeature(feature_external_ges, False)

	with GzipFile(filename) as inp:
		doc = pulldom.parse(inp, parser)
		xmlEvents = iter(doc)
		parseTopLevel()

	print('read %d input events' % len(inputEvents), file=stderr)
	return inputEvents

def combineEvents(inputEvents):
	'''Combines multiple events on the same keyboard row at the same timestamp
	into a single event.
	'''
	pendingTime = None

	def outputPending():
		if pendingTime is not None:
			for row in sorted(pressForRow.keys() | releaseForRow.keys()):
				press = pressForRow[row]
				release = releaseForRow[row]
				if press != 0 or release != 0:
					yield pendingTime, row, press, release

	for time, row, press, release in inputEvents:
		if time != pendingTime:
			assert pendingTime is None or time > pendingTime, time

			# Output previous event.
			yield from outputPending()

			# Start new event.
			pendingTime = time
			pressForRow = defaultdict(int)
			releaseForRow = defaultdict(int)

		pressForRow[row] = (pressForRow[row] | press) & ~release
		releaseForRow[row] = (releaseForRow[row] | release) & ~press
	else:
		yield from outputPending()

def removeRedundantEvents(inputEvents):
	'''Remove (parts of) events that don't change the keyboard matrix state.
	'''
	matrix = [0] * 12
	for time, row, press, release in inputEvents:
		old = matrix[row]
		press &= ~old
		release &= old
		if press != 0 or release != 0:
			matrix[row] = (old | press) & ~release
			yield time, row, press, release

def filterEvents(inputEvents, wantedKeys):
	'''Filter the given input events to contain only presses and releases of
	the given keys.
	The wantedKeys argument should be mapping from row to a column bitmask,
	where bits are set if we want to preserve changes in the corresponding
	position in the keyboard matrix.
	'''
	for time, row, press, release in inputEvents:
		mask = wantedKeys.get(row, 0)
		press &= mask
		release &= mask
		if press != 0 or release != 0:
			yield time, row, press, release

def scaleTime(inputEvents, tickScale, offset):
	'''Yields the given input events, with the time stamps relative to the
	given offset, divided by the given scale and rounded to the nearest
	integer.
	'''
	for time, row, press, release in inputEvents:
		yield floor((time - offset) / tickScale + 0.5), row, press, release

def findPhase(timestamps, period):
	'''Returns the phase (the offset within the period) that occurs most
	often in the given timestamps, and the number of timestamps that are
	close to that phase.
	This assumes that the input was entered at a fixed position within each
	frame, using tools like advance_frame: that runs exactly one frame from
	where the emulation is paused, so the events of keys pressed while
	paused are at exactly the same position within every frame. Some events
	can be later in the frame (e.g. keys pressed while the emulation was
	running), but not before it, so we use the most common phase rather than
	the center of all phases.
	'''
	tolerance = period // 1000
	phases = Counter(time % period for time in timestamps)
	bestPhase = phases.most_common(1)[0][0]
	aligned = sum(
		count
		for phase, count in phases.items()
		if min(abs(phase - bestPhase), period - abs(phase - bestPhase))
			<= tolerance
		)
	return bestPhase, aligned

def detectTiming(timestamps):
	'''Determine the most likely number of master clock ticks per frame, and
	the position of the input events within the frame (the offset), by
	looking at how well the timestamps align when using the two frame
	timings that openMSX supports. The input events are usually not at
	multiples of the frame length counted from time 0, so for each frame
	timing we use the most common position within the frame as the offset
	(see findPhase()).
	This assumes that the frame timing is the same throughout the replay,
	which is not guaranteed in general, but will hopefully be the case for
	tool-assisted speedruns.
	Returns the number of ticks per frame and the offset.
	'''
	ticksPerFrame50 = (masterClock * vdpTicksPerLine * 313) // vdpClock
	ticksPerFrame60 = (masterClock * vdpTicksPerLine * 262) // vdpClock

	timestamps = tuple(timestamps)
	offset50, aligned50 = findPhase(timestamps, ticksPerFrame50)
	offset60, aligned60 = findPhase(timestamps, ticksPerFrame60)

	if aligned50 > aligned60:
		fps, ticksPerFrame, offset, aligned = \
				50, ticksPerFrame50, offset50, aligned50
	else:
		fps, ticksPerFrame, offset, aligned = \
				60, ticksPerFrame60, offset60, aligned60
	print('event timestamps align with %d fps timing: %.2f%% '
			'(offset: %d ticks)'
			% (fps, 100 * aligned / len(timestamps), offset), file=stderr)
	return ticksPerFrame, offset

def eventsToState(inputEvents):
	'''Yields pairs of a timestamp and a set of the active keys at that time.
	'''
	active = set()
	prevTime = None
	for time, row, press, release in inputEvents:
		assert (press & release) == 0, (press, release)

		if prevTime != time:
			if prevTime is not None:
				yield prevTime, frozenset(active)
			prevTime = time

		col = 0
		while press:
			if press & 1:
				active.add((row, col))
			col += 1
			press >>= 1

		col = 0
		while release:
			if release & 1:
				active.remove((row, col))
			col += 1
			release >>= 1

	if prevTime is not None:
		yield prevTime, frozenset(active)

def formatState(active):
	return ' '.join(
		inputMapReverse[pos]
		for pos in sorted(active, key=lambda pos: (pos[0], -pos[1]))
		)

def convert(inFilename, outFilename):
	wantedKeys = {}
	for name, (row, col) in inputMap.items():
		wantedKeys[row] = wantedKeys.get(row, 0) | (1 << col)

	inputEvents = list(removeRedundantEvents(
			filterEvents(combineEvents(readEvents(inFilename)), wantedKeys)
			))
	print('after cleanup %d events remain' % len(inputEvents), file=stderr)
	if not inputEvents:
		print(
			"no events match; you should probably customize 'inputMap' "
			"at the top of this script", file=stderr
			)
		return

	ticksPerFrame, offset = detectTiming(evt[0] for evt in inputEvents)
	scaledEvents = list(removeRedundantEvents(
			combineEvents(scaleTime(inputEvents, ticksPerFrame, offset))
			))

	with open(outFilename, 'w') as out:
		print('writing output:', outFilename)

		print('= base', inFilename, file=out)
		print('= out', 'replay.omr', file=out)
		print('= scale', ticksPerFrame, file=out)
		print('= offset', offset, file=out)
		for name, (row, col) in sorted(
				inputMap.items(), key=lambda item: (item[1][0], -item[1][1])
				):
			print('= input', name, 'key', row, col, file=out)
		print(file=out)

		def printMilestone(frame, seconds):
			print('# frame %d (%d:%02d)' % ((frame,) + divmod(seconds, 60)),
					file=out)
		milestone = 10 # seconds

		prevFrame = 0
		prevSeconds = 0
		printMilestone(prevFrame, prevSeconds)
		prevActive = set()
		for frame, active in eventsToState(scaledEvents):
			delta = frame - prevFrame
			print(('%4d  %s' % (delta, formatState(prevActive))).rstrip(),
					file=out)
			seconds = (offset + frame * ticksPerFrame) // masterClock
			if seconds // milestone != prevSeconds // milestone:
				printMilestone(frame, seconds)
			prevFrame = frame
			prevSeconds = seconds
			prevActive = active
		printMilestone(prevFrame, prevSeconds)

if __name__ == '__main__':
	from sys import argv
	if len(argv) != 2:
		print('Usage: omr2txt.py file1.omr', file=stderr)
		print('Converts an openMSX replay to a text file.', file=stderr)
		exit(2)
	else:
		inFilename = argv[1]
		if inFilename.endswith('.omr'):
			outFilename = inFilename[:-4] + '.txt'
			convert(inFilename, outFilename)
		else:
			print('File name does not end in ".omr":', inFilename)
			exit(2)
