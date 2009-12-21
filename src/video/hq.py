# $Id$
#
# To run:
#   python hq.py
#
# To profile:
#   python -m cProfile -s cumulative hq.py > hq-profile.txt

from collections import defaultdict
from itertools import count, izip
from math import sqrt
import sys

# Parser:

class BaseParser(object):
	zoom = None

	@staticmethod
	def _filterSwitch(stream):
		log = False
		inIf = False
		for line in stream:
			line = line.strip()
			if line == 'switch (pattern) {':
				log = True
			elif line == '}':
				if inIf:
					inIf = False
				elif log:
					break
			elif line.startswith('if'):
				inIf = True
			if log:
				if '?' in line:
					line += ' ' + stream.next().strip()
					split0 = line.index('=')
					split1 = line.index('?')
					split2 = line.index(':')
					split3 = line.index(';')
					varName = line[ : split0].strip()
					exprTest = line[split0 + 1 : split1].strip()
					exprTrue = line[split1 + 1 : split2].strip()
					exprFalse = line[split2 + 1 : split3].strip()
					yield 'if (%s) {' % exprTest
					yield '%s = %s;' % (varName, exprTrue)
					yield '} else {'
					yield '%s = %s;' % (varName, exprFalse)
					yield '}'
				else:
					yield line

	@staticmethod
	def _parseSubPixel(name):
		raise NotImplementedError

	def __init__(self):
		self.pixelExpr = [ [ None ] * (self.zoom ** 2) for _ in range(1 << 12) ]
		self._parse()
		self._sanityCheck()

	def _parse(self):
		cases = []
		subCases = range(1 << 4)
		for line in self._filterSwitch(file('HQ%dxScaler.in' % self.zoom)):
			if line.startswith('case'):
				cases.append(int(line[5 : line.index(':', 5)]))
			elif line.startswith('pixel'):
				subPixel = self._parseSubPixel(line[5])
				expr = line[line.index('=') + 1 : ].strip()
				self._addCases(cases, subCases, subPixel, expr[ : -1])
			elif line.startswith('if'):
				index = line.find('edge')
				assert index != -1
				index1 = line.index('(', index)
				index2 = line.index(',', index1)
				index3 = line.index(')', index2)
				pix1s = line[index1 + 1 : index2].strip()
				pix2s = line[index2 + 1 : index3].strip()
				assert pix1s[0] == 'c', pix1s
				assert pix2s[0] == 'c', pix2s
				pix1 = int(pix1s[1:])
				pix2 = int(pix2s[1:])
				if pix1 == 2 and pix2 == 6:
					subCase = 0
				elif pix1 == 6 and pix2 == 8:
					subCase = 1
				elif pix1 == 8 and pix2 == 4:
					subCase = 2
				elif pix1 == 4 and pix2 == 2:
					subCase = 3
				else:
					assert False, (line, pix1, pix2)
				subCases = [ x for x in range(1 << 4) if x & (1 << subCase) ]
			elif line.startswith('} else'):
				subCases = [ x for x in range(1 << 4) if x not in subCases ]
			elif line.startswith('}'):
				subCases = range(1 << 4)
			elif line.startswith('break'):
				cases = []
				subCases = range(1 << 4)

	def _addCases(self, cases, subCases, subPixel, expr):
		pixelExpr = self.pixelExpr
		for case in cases:
			for subCase in subCases:
				weights = [ 0 ] * 9
				if expr.startswith('interpolate'):
					factorsStr = expr[
						expr.index('<') + 1 : expr.index('>')
						].split(',')
					pixelsStr = expr[
						expr.index('(') + 1 : expr.index(')')
						].split(',')
					assert len(factorsStr) == len(pixelsStr)
					for factorStr, pixelStr in izip(factorsStr, pixelsStr):
						factor = int(factorStr)
						pixelStr = pixelStr.strip()
						assert pixelStr[0] == 'c'
						pixel = int(pixelStr[1 : ]) - 1
						weights[pixel] = factor
				else:
					assert expr[0] == 'c'
					weights[int(expr[1 : ]) - 1] = 1
				pixelExpr[(case << 4) | subCase][subPixel] = tuple(weights)

	def _sanityCheck(self):
		'''Check various observed properties.
		'''
		for case, expr in enumerate(self.pixelExpr):
			for weights in expr:
				# Sum of weight factors is always a power of two.
				assert isPow2(sum(weights)), (case, weights)

				# There are at most 3 non-zero weights, and if there are 3,
				# one of those must be for the center pixel.
				numNonZero = sum(weight != 0 for weight in weights)
				assert numNonZero <= 3, (case, weights)
				assert numNonZero < 3 or weights[4] != 0, (case, weights)

		# Subpixel depends only on the center and three neighbours in the
		# direction of the subpixel itself.
		center = (self.zoom - 1) / 2.0
		def influentialNeighbours(x, y):
			qx = 0 if x < center else 2 if x > center else 1
			qy = 0 if y < center else 2 if y > center else 1
			for ny in set([1, qy]):
				for nx in set([1, qx]):
					yield 3 * ny + nx
		subsets = tuple(
			tuple(sorted(influentialNeighbours(x, y)))
			for y in xrange(self.zoom)
			for x in xrange(self.zoom)
			)
		for case, expr in enumerate(self.pixelExpr):
			assert len(expr) == len(subsets)
			for weights, subset in izip(expr, subsets):
				for neighbour in range(9):
					if neighbour not in subset:
						assert weights[neighbour] == 0, (
							case, neighbour, weights
							)

class Parser2x(BaseParser):
	zoom = 2

	@staticmethod
	def _parseSubPixel(name):
		subPixel = int(name) - 1
		assert 0 <= subPixel < 4
		return subPixel

	def _sanityCheck(self):
		BaseParser._sanityCheck(self)

		# Weight of the center pixel is never zero.
		for case, expr in enumerate(self.pixelExpr):
			for weights in expr:
				assert weights[4] != 0

class Parser3x(BaseParser):
	zoom = 3

	@staticmethod
	def _parseSubPixel(name):
		subPixel = int(name) - 1
		assert 0 <= subPixel < 9
		assert subPixel != 5 - 1
		return subPixel

	def _parse(self):
		BaseParser._parse(self)
		for expr in self.pixelExpr:
			assert expr[4] is None
			expr[4] = (0, 0, 0, 0, 1, 0, 0, 0, 0)

class Parser4x(BaseParser):
	zoom = 4

	@staticmethod
	def _parseSubPixel(name):
		subPixel = int(name, 16)
		assert 0 <= subPixel < 16
		return subPixel

# Variant classes:

class Variant(object):

	def __init__(self, pixelExpr, lite, table, narrow = None):
		self.lite = lite
		self.table = table
		if lite:
			pixelExpr = makeLite(pixelExpr, table)
		if narrow is not None:
			pixelExpr = narrow(pixelExpr)
		pixelExpr = permuteCases(
			(5, 0, 4, 6, 3, 10, 11, 2, 1, 9, 8, 7)
			if table else
			(2, 9, 7, 4, 3, 10, 11, 1, 8, 0, 6, 5),
			pixelExpr
			)
		self.pixelExpr = pixelExpr

	def writeSwitch(self, fileName):
		writeTextFile(fileName, genSwitch(self.pixelExpr))

# I/O:

def printText(contents):
	for text in contents:
		sys.stdout.write(text)

def writeFile(fileName, mode, contents):
	content = ''.join(contents)
	out = open(fileName, mode)
	try:
		out.write(content)
	finally:
		out.close()

def writeTextFile(fileName, contents):
	writeFile(fileName, 'w', contents)

def writeBinaryFile(fileName, bytes):
	writeFile(fileName, 'wb', ( chr(byte) for byte in bytes ))

# Output as switch statement:

def genSwitch(pixelExpr):
	# Note: In practice, only the center subpixel of HQ3x is independent of
	#       the edge bits. So maybe this is a bit overengineered, but it does
	#       express why this optimization is correct.
	subExprForSubPixel = tuple(
		set(expr[subPixel] for expr in pixelExpr if expr[subPixel] is not None)
		for subPixel in range(len(pixelExpr[0]))
		)
	isIndependentSubPixel = tuple(
		len(subExprs) == 1
		for subExprs in subExprForSubPixel
		)

	exprToCases = defaultdict(list)
	for case, expr in enumerate(pixelExpr):
		if expr[0] is None:
			assert all(subExpr is None for subExpr in expr)
			key = None
		else:
			key = tuple(tuple(subExpr) for subExpr in expr)
		exprToCases[key].append(case)
	yield 'switch (pattern) {\n'
	for cases, expr in sorted(
		( sorted(cases), expr )
		for expr, cases in exprToCases.iteritems()
		):
		if expr is not None:
			for case in cases:
				yield 'case %d:\n' % case
			for subPixel, subExpr in enumerate(expr):
				if not isIndependentSubPixel[subPixel]:
					yield '\tpixel%s = %s;\n' % (
						hex(subPixel)[2 : ], getBlendCode(subExpr)
						)
			yield '\tbreak;\n'
	for case in sorted(exprToCases[None]):
		yield 'case %d:\n' % case
	yield 'default:\n'
	yield '\tUNREACHABLE;\n'
	yield '\t%s = 0; // avoid warning\n' % ' = '.join(
		'pixel%d' % subPixel
		for subPixel, independent_ in enumerate(isIndependentSubPixel)
		)
	yield '}\n'

	for subPixel, independent in enumerate(isIndependentSubPixel):
		if independent:
			subExpr, = subExprForSubPixel[subPixel]
			yield 'pixel%s = %s;\n' % (
				hex(subPixel)[2 : ], getBlendCode(subExpr)
				)

def getBlendCode(weights):
	wsum = sum(weights)
	assert isPow2(wsum)
	if wsum == 1:
		index, = (index for index, weight in enumerate(weights) if weight != 0)
		return 'c%d' % (index + 1)
	elif wsum <= 8:
		# Because the lower 3 bits of each colour component (R,G,B)
		# are zeroed out, we can operate on a single integer as if it
		# is a vector.
		return '(%s) / %d' % (
			' + '.join(
				'c%d * %d' % (index + 1, weight)
				for index, weight in enumerate(weights)
				if weight != 0
				),
			wsum
			)
	else:
		return '(((%s) & (0x00FF00 * %d)) | ((%s) & (0xFF00FF * %d))) / %d' % (
			' + '.join(
				'(c%d & 0x00FF00) * %d' % (index + 1, weight)
				for index, weight in enumerate(weights)
				if weight != 0
				),
			wsum,
			' + '.join(
				'(c%d & 0xFF00FF) * %d' % (index + 1, weight)
				for index, weight in enumerate(weights)
				if weight != 0
				),
			wsum,
			wsum
			)

# Table output as text:

def formatOffsetsTable(pixelExpr):
	for case, expr in enumerate(pixelExpr):
		yield '// %d\n' % case
		for weights in expr:
			for x, y in transformOffsets(weights):
				yield ' %3d, %3d,' % (x, y)
			yield '\n'

def formatWeightsTable(pixelExpr, cellFunc):
	for case, expr in enumerate(pixelExpr):
		yield '// %d\n' % case
		for weights in expr:
			for weight in transformWeights(weights, cellFunc):
				yield ' %3d,' % weight
			yield '\n'

# The rest:

def getZoom(pixelExpr):
	zoom = int(sqrt(len(pixelExpr[0])))
	assert zoom * zoom == len(pixelExpr[0])
	return zoom

def isPow2(num):
	if num == 1:
		return True
	if (num % 2) == 1:
		return False
	return isPow2(num / 2)

def permuteCase(permutation, case):
	return sum(
		((case >> oldBit) & 1) << newBit
		for newBit, oldBit in enumerate(permutation)
		)

def permuteCases(permutation, pixelExpr):
	pixelExpr2 = [ None ] * len(pixelExpr)
	for case, expr in enumerate(pixelExpr):
		pixelExpr2[permuteCase(permutation, case)] = expr
	assert None not in pixelExpr2
	return pixelExpr2

def computeNeighbours(weights):
	neighbours = [ i for i in range(9) if i != 4 and weights[i] != 0 ]
	assert len(neighbours) <= 2
	neighbours += [ None ] * (2 - len(neighbours))
	return neighbours

def transformOffsets(weights):
	for neighbour in computeNeighbours(weights):
		yield (
			min(255, (1 if neighbour is None else neighbour % 3) * 128),
			min(255, (1 if neighbour is None else neighbour / 3) * 128)
			)

def transformWeights(weights, cellFunc):
	factor = 256 / sum(weights)
	for cell in cellFunc(weights):
		yield min(255, 0 if cell is None else factor * weights[cell])

def computeOffsets(pixelExpr):
	for expr in pixelExpr:
		for weights in expr:
			for x, y in transformOffsets(weights):
				yield x
				yield y

def computeWeights(pixelExpr, cellFunc):
	for expr in pixelExpr:
		for weights in expr:
			for transformedWeight in transformWeights(weights, cellFunc):
				yield transformedWeight

def computeWeightCells(weights):
	neighbours = computeNeighbours(weights)
	return (neighbours[0], neighbours[1], 4)

def computeLiteWeightCells(weights_):
	return (3, 4, 5)

def isContradiction(case):
	inv = case ^ 0xFFF
	return (
		(inv & 0x121) in [0x120, 0x101, 0x021] or
		(inv & 0x502) in [0x500, 0x402, 0x102] or
		(inv & 0x484) in [0x480, 0x404, 0x084] or
		(inv & 0x0A8) in [0x0A0, 0x088, 0x028] or
		(inv & 0x00F) in [0x00E, 0x00D, 0x00B, 0x007]
		)

def gcd(a, b):
	'''Returns the greatest common divisor of a and b.
	'''
	while a != 0:
		a, b = b % a, a
	return b

def simplifyWeights(weights):
	weights = tuple(weights)
	divider = reduce(gcd, weights, 0)
	return tuple(w / divider for w in weights)

def blendWeights(weights1, weights2, factor1 = 1, factor2 = 1):
	factor1 *= sum(weights2)
	factor2 *= sum(weights1)
	return simplifyWeights(
		factor1 * w1 + factor2 * w2
		for w1, w2 in izip(weights1, weights2)
		)

def calcNeighbourToSet():
	# Edges in the same order as the edge bits in "case".
	edges = (
		(5, 1), (5, 7), (3, 7), (3, 1),
		(4, 0), (4, 1), (4, 2), (4, 3),
		(4, 5), (4, 6), (4, 7), (4, 8),
		)
	# Compute equivalence classes.
	ret = []
	for case in xrange(1 << len(edges)):
		if isContradiction(case):
			ret.append(None)
		else:
			neighbourToSet = [ set([neighbour]) for neighbour in xrange(9) ]
			for edge, (neighbour1, neighbour2) in enumerate(edges):
				if (case & (1 << edge)) == 0:
					# No edge, so the two pixels are equal.
					# Merge the sets of equal pixels.
					set1 = neighbourToSet[neighbour1]
					set2 = neighbourToSet[neighbour2]
					set1 |= set2
					for n in set2:
						neighbourToSet[n] = set1
			ret.append(neighbourToSet)
	return ret

neighbourToSet = calcNeighbourToSet()

def lighten(case, weights, preferRight):
	equalNeighboursOf = neighbourToSet[case]
	if equalNeighboursOf is None:
		return None
	else:
		done = set()
		newWeights = [ 0 ] * 9
		for neighbour in (
			(4, 5, 3, 1, 7, 2, 0, 8, 6)
			if preferRight else
			(4, 3, 5, 1, 7, 0, 2, 6, 8)
			):
			if neighbour not in done:
				equalNeighbours = equalNeighboursOf[neighbour]
				newWeights[neighbour] = sum(
					weights[n] for n in equalNeighbours
					)
				done |= equalNeighbours
		for c in (0, 1, 2, 6, 7, 8):
			# Only c4, c5, c6 have non-zero weight.
			assert newWeights[c] == 0
		return simplifyWeights(newWeights)

def makeLite(pixelExpr, biased):
	if biased:
		zoom = getZoom(pixelExpr)
		center = (zoom - 1) / 2.0
		preferRightSubPixels = tuple(
			subPixel
			for subPixel in xrange(zoom ** 2)
			if subPixel % zoom > center
			)
	else:
		preferRightSubPixels = ()
	return [
		[ lighten(case, weights, subPixel in preferRightSubPixels)
		  for subPixel, weights in enumerate(expr) ]
		for case, expr in enumerate(pixelExpr)
		]

def makeNarrow2to1(pixelExpr):
	return tuple(
		(None, None) if a is None else (blendWeights(a, b), blendWeights(c, d))
		for a, b, c, d in pixelExpr
		)

def makeNarrow3to2(pixelExpr):
	return [
		[	blendWeights(expr[0], expr[1], 2, 1),
			blendWeights(expr[2], expr[1], 2, 1),
			blendWeights(expr[3], expr[4], 2, 1),
			blendWeights(expr[5], expr[4], 2, 1),
			blendWeights(expr[6], expr[7], 2, 1),
			blendWeights(expr[8], expr[7], 2, 1),
			]
		for expr in pixelExpr
		]

def genHQLiteOffsetsTable(pixelExpr):
	'''In the hqlite case, the result color depends on at most one neighbour
	color. Therefore, an offset into an interpolated texture is used instead
	of explicit weights.
	'''
	zoom = getZoom(pixelExpr)
	for expr in pixelExpr:
		for subPixel, weights in enumerate(expr):
			if weights is None:
				neighbour = None
			else:
				neighbours = computeNeighbours(weights)
				assert neighbours[1] is None, neighbours
				neighbour = neighbours[0]
				factor = sum(weights)

			sy, sx = divmod(subPixel, zoom)
			x, y = (int(192.5 - 128.0 * (0.5 + sc) / zoom) for sc in (sx, sy))
			if neighbour == 3:
				x -= 128 * weights[3] / factor
			elif neighbour == 5:
				x += 128 * weights[5] / factor
			else:
				assert neighbour is None, neighbour
			assert 0 <= x < 256, x
			assert 0 <= y < 256, y
			yield x
			yield y

# Main:

def process2x():
	parser = Parser2x()

	fullTableVariant = Variant(parser.pixelExpr, lite = False, table = True)
	liteTableVariant = Variant(parser.pixelExpr, lite = True,  table = True)

	#printText(formatOffsetsTable(fullTableVariant.pixelExpr))
	#printText(formatOffsetsTable(liteTableVariant.pixelExpr))
	#printText(formatWeightsTable(
	#	fullTableVariant.pixelExpr, computeWeightCells
	#	))
	#printText(formatWeightsTable(
	#	liteTableVariant.pixelExpr, computeLiteWeightCells
	#	))

	writeBinaryFile(
		'HQ2xOffsets.dat',
		computeOffsets(fullTableVariant.pixelExpr)
		)
	writeBinaryFile(
		'HQ2xWeights.dat',
		computeWeights(fullTableVariant.pixelExpr, computeWeightCells)
		)
	writeBinaryFile(
		'HQ2xLiteOffsets.dat',
		genHQLiteOffsetsTable(liteTableVariant.pixelExpr)
		)
	# Note: HQ2xLiteWeights.dat is not needed, since interpolated texture
	#       offsets can perform all the blending we need.

	Variant(parser.pixelExpr, lite = False, table = False).writeSwitch(
		'HQ2xScaler-1x1to2x2.nn'
		)
	Variant(parser.pixelExpr, lite = False, table = False, narrow = makeNarrow2to1).writeSwitch(
		'HQ2xScaler-1x1to1x2.nn'
		)
	Variant(parser.pixelExpr, lite = True,  table = False).writeSwitch(
		'HQ2xLiteScaler-1x1to2x2.nn'
		)
	Variant(parser.pixelExpr, lite = True,  table = False, narrow = makeNarrow2to1).writeSwitch(
		'HQ2xLiteScaler-1x1to1x2.nn'
		)

def process3x():
	parser = Parser3x()

	fullTableVariant = Variant(parser.pixelExpr, lite = False, table = True )
	liteTableVariant = Variant(parser.pixelExpr, lite = True,  table = True )

	#printText(formatOffsetsTable(fullTableVariant.pixelExpr))
	#printText(formatOffsetsTable(liteTableVariant.pixelExpr))
	#printText(formatWeightsTable(
		#fullTableVariant.pixelExpr, computeWeightCells
		#))
	#printText(formatWeightsTable(
		#liteTableVariant.pixelExpr, computeLiteWeightCells
		#))

	writeBinaryFile(
		'HQ3xOffsets.dat',
		computeOffsets(fullTableVariant.pixelExpr)
		)
	writeBinaryFile(
		'HQ3xWeights.dat',
		computeWeights(fullTableVariant.pixelExpr, computeWeightCells)
		)
	writeBinaryFile(
		'HQ3xLiteOffsets.dat',
		genHQLiteOffsetsTable(liteTableVariant.pixelExpr)
		)
	# Note: HQ3xLiteWeights.dat is not needed, since interpolated texture
	#       offsets can perform all the blending we need.

	Variant(parser.pixelExpr, lite = False, table = False).writeSwitch(
		'HQ3xScaler-1x1to3x3.nn'
		)
	Variant(parser.pixelExpr, lite = True,  table = False).writeSwitch(
		'HQ3xLiteScaler-1x1to3x3.nn'
		)

def process4x():
	parser = Parser4x()

	fullTableVariant = Variant(parser.pixelExpr, lite = False, table = True)
	liteTableVariant = Variant(parser.pixelExpr, lite = True,  table = True)

	#printText(formatOffsetsTable(fullTableVariant.pixelExpr))
	#printText(formatWeightsTable(fullTableVariant.pixelExpr, computeWeightCells))
	#printText(formatWeightsTable(liteTableVariant.pixelExpr, computeLiteWeightCells))

	writeBinaryFile(
		'HQ4xOffsets.dat',
		computeOffsets(fullTableVariant.pixelExpr)
		)
	writeBinaryFile(
		'HQ4xWeights.dat',
		computeWeights(fullTableVariant.pixelExpr, computeWeightCells)
		)
	writeBinaryFile(
		'HQ4xLiteOffsets.dat',
		genHQLiteOffsetsTable(liteTableVariant.pixelExpr)
		)
	# Note: HQ4xLiteWeights.dat is not needed, since interpolated texture
	#       offsets can perform all the blending we need.

	#printText(genSwitch(Variant(
		#parser.pixelExpr, lite = False, table = False
		#).pixelExpr))

if __name__ == '__main__':
	process2x()
	process3x()
	process4x()
