# $Id$

import hqcommon

from copy import deepcopy
from itertools import izip
import sys

def flatten(sequence):
	for elem1 in sequence:
		if hasattr(elem1, '__iter__'):
			for elem2 in flatten(elem1):
				yield elem2
		else:
			yield elem1

def permuteCase(permutation, case):
	return sum(
		((case >> oldBit) & 1) << newBit
		for newBit, oldBit in enumerate(permutation)
		)

def permuteCases(permutation, pixelExpr):
	pixelExpr2 = [ None ] * (1 << 12)
	for case in range(1 << 12):
		# TODO: If we rewrite a[permute1(c)] = c to a[c] = permute2(c),
		#       this function will become a single list comprehension.
		pixelExpr2[permuteCase(permutation, case)] = pixelExpr[case]
	return pixelExpr2

def printText(contents):
	for text in contents:
		sys.stdout.write(text)

def writeFile(fileName, mode, contents):
	out = open(fileName, mode)
	try:
		for text in contents:
			out.write(text)
	finally:
		out.close()

def writeTextFile(fileName, contents):
	writeFile(fileName, 'w', contents)

def writeBinaryFile(fileName, contents):
	writeFile(fileName, 'wb', contents)

def byteStream(bytes):
	return ( chr(byte) for byte in flatten(bytes) )

def genSwitch(pixelExpr, narrow):
	permutation = (2, 9, 7, 4, 3, 10, 11, 1, 8, 0, 6, 5)
	exprToCases = {}
	for case, expr in enumerate(pixelExpr):
		exprToCases.setdefault(
			tuple(tuple(subExpr) for subExpr in expr),
			[]
			).append(permuteCase(permutation, case))
	#print exprToCases
	yield 'switch (pattern) {\n'
	for cases, expr in sorted(
		( sorted(cases), expr )
		for expr, cases in exprToCases.iteritems()
		):
		for case in cases:
			yield 'case %d:\n' % case
		for subPixel, subExpr in enumerate(expr):
			yield '\tpixel%d = ' % (subPixel + 1)
			#sys.stderr.write('%s\n' % repr(subExpr))
			wsum = sum(subExpr)
			if wsum == 1:
				assert subExpr[5 - 1] == 1
				yield 'c5;\n'
			elif wsum <= 8:
				# Because the lower 3 bits of each colour component (R,G,B)
				# are zeroed out, we can operate on a single integer as if it
				# is a vector.
				yield (
					'(' +
					' + '.join(
						'c%d * %d' % (index + 1, weight)
						for index, weight in enumerate(subExpr)
						if weight != 0
						) +
					') / %d;\n' % wsum
					)
			else:
				yield (
					'(('
						'(' + ' + '.join(
							'(c%d & 0x00FF00) * %d' % (index + 1, weight)
							for index, weight in enumerate(subExpr)
							if weight != 0
							) +
						') & (0x00FF00 * %d)' % wsum +
					') | ('
						'(' + ' + '.join(
							'(c%d & 0xFF00FF) * %d' % (index + 1, weight)
							for index, weight in enumerate(subExpr)
							if weight != 0
							) +
						') & (0xFF00FF * %d)' % wsum +
					')) / %d;\n' % wsum
					)
		yield ('\tbreak;\n')
	yield ('default:\n')
	yield ('\tUNREACHABLE;\n')
	yield '\t%s = 0; // avoid warning\n' % (
		' = '.join('pixel%d' % (i + 1) for i in range(2 if narrow else 4))
		)
	yield '}\n'

def computeXY(pixelExpr):
	# TODO: Would None instead of -1 be better?
	#       As far as I can see, the value is used to fill positions that
	#       are unused: the value only tested, never computed with.
	xy = [[[-1] * 2 for _ in range(4)] for _ in range(1 << 12)]
	for case in range(1 << 12):
		for subPixel in range(4):
			j = 0
			for i in (0, 1, 2, 3, 5, 6, 7, 8):
				if pixelExpr[case][subPixel][i] != 0:
					assert j < 2
					xy[case][subPixel][j] = i
					j += 1
	return xy

def transformOffsets(weights_, neighbours):
	return [
		( min(255, (1 if neighbour == -1 else neighbour % 3) * 128),
		  min(255, (1 if neighbour == -1 else neighbour / 3) * 128) )
		for neighbour in neighbours
		]

def transformWeights(weights, cells):
	factor = 256 / sum(weights)
	return tuple(
		min(255, 0 if c == -1 else factor * weights[c])
		for c in cells
		)

def computeTable(pixelExpr, transform):
	# TODO: This structure will be flattened when generating the data files.
	#       Only the format routines need the structure, but it might be
	#       simpler to let them call the transform function directly and
	#       skip the generation of the data structure.
	return [
		[ transform(weights, neighbours)
		  for weights, neighbours in izip(pixelCase, xyCase) ]
		for pixelCase, xyCase in izip(pixelExpr, computeXY(pixelExpr))
		]

def computeOffsets(pixelExpr):
	return computeTable(pixelExpr, transformOffsets)

def computeWeights(pixelExpr):
	return computeTable(
		pixelExpr,
		lambda weights, neighbours:
			transformWeights(weights, (neighbours[0], neighbours[1], 4))
		)

def computeLiteWeights(pixelExpr):
	return computeTable(
		pixelExpr,
		lambda weights, neighbours_: transformWeights(weights, (3, 4, 5))
		)

def genHQLiteOffsetsTable(pixelExpr):
	# TODO: Can this function be rewritten as a transform function?
	offset_x = ( 32, -32,  32, -32)
	offset_y = ( 32,  32, -32, -32)
	for case in range(1 << 12):
		for subPixel in range(4):
			for c in (0, 1, 2, 6, 7, 8):
				assert pixelExpr[case][subPixel][c] == 0
			assert (pixelExpr[case][subPixel][3] == 0) \
				or (pixelExpr[case][subPixel][5] == 0)
			factor = sum(pixelExpr[case][subPixel])
			x = offset_x[subPixel] + 128
			y = offset_y[subPixel] + 128
			if pixelExpr[case][subPixel][5] == 0:
				x -= 128 * pixelExpr[case][subPixel][3] / factor
			else:
				x += 128 * pixelExpr[case][subPixel][5] / factor
			yield x, y

def formatOffsetsTable(offsetsTable):
	for case, subPixelNeighbourOffsets in enumerate(offsetsTable):
		yield '// %d\n' % case
		for neighbourOffset in subPixelNeighbourOffsets:
			for x, y in neighbourOffset:
				yield ' %3d, %3d,' % (x, y)
			yield '\n'

def formatWeightsTable(weightsTable):
	for case, subPixelWeights in enumerate(weightsTable):
		yield '// %d\n' % case
		for weights in subPixelWeights:
			for weight in weights:
				yield ' %3d,' % weight
			yield '\n'

def sanityCheck(pixelExpr):
	'''Check various observed properties.
	'''
	subsets = [ (5, 4, 2, 1), (5, 6, 2, 3), (5, 4, 8, 7), (5, 6, 8, 9) ]

	for case, expr in enumerate(pixelExpr):
		# Weight factor of center pixel is never zero.
		for corner in expr:
			assert corner[5 - 1] != 0

		# Sum of weight factors is always a power of two.
		# But 2 doesn't occur for some reason, so there is never an equal blend.
		for corner in expr:
			total = sum(corner)
			assert total in (1, 4, 8, 16), total
			#
			count = reduce(lambda x, y: x + (y != 0), corner, 0)
			# at most 3 non-zero coef
			assert count <= 3, (case, corner)
			# and if there are 3 one of those must be c5
			assert (count < 3) or (corner[4] != 0)

		# Subpixel depends only on the center and three neighbours in the
		# direction of the subpixel itself.
		for corner, subset in izip(expr, subsets):
			for pixel in range(9):
				if (pixel + 1) not in subset:
					assert corner[pixel] == 0, corner

def makeLite(pixelExpr, preferC6subPixels):
	# TODO: Rewrite hqcommon.makeLite() so it doesn't change its input.
	liteExpr = deepcopy(pixelExpr)
	hqcommon.makeLite(liteExpr, preferC6subPixels)
	return liteExpr

def mixWeights(weights1, weights2):
	sum1 = sum(weights1)
	sum2 = sum(weights2)
	return hqcommon.simplifyWeights([
		sum2 * w1 + sum1 * w2
		for w1, w2 in izip(weights1, weights2)
		])

def makeNarrow(pixelExpr):
	return [
		[ mixWeights(a, b), mixWeights(c, d) ]
		for a, b, c, d in pixelExpr
		]

class Parser(object):

	def __init__(self):
		self.fileName = 'HQ2xScaler.in'
		self.pixelExpr = [ [ None ] * 4 for _ in range(1 << 12) ]
		self.__parse()
		sanityCheck(self.pixelExpr)

	@staticmethod
	def __filterSwitch(stream):
		log = False
		for line in stream:
			line = line.strip()
			if line == 'switch (pattern) {':
				log = True
			elif line == '}':
				if log:
					break
			elif line.startswith('//pixel'):
				line = line[2 : ]
			if log:
				if 'edge(' in line:
					line += ' ' + stream.next().strip()
				if '/' not in line:
					yield line

	def __parse(self):
		cases = []
		for line in self.__filterSwitch(file(self.fileName)):
			if line.startswith('case'):
				cases.append(int(line[5 : line.index(':', 5)]))
			elif line.startswith('pixel'):
				subPixel = int(line[5]) - 1
				assert 0 <= subPixel < 4
				expr = line[line.index('=') + 1 : ].strip()
				index = expr.find('edge')
				if index == -1:
					assert expr[-1] == ';'
					self.__addCases(cases, range(1 << 4), subPixel, expr[ : -1])
				else:
					index1 = expr.index('(', index)
					index2 = expr.index(',', index1)
					index3 = expr.index(')', index2)
					pix1s = expr[index1 + 1 : index2].strip()
					pix2s = expr[index2 + 1 : index3].strip()
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
						assert False, (pix1, pix2)
					split1 = expr.index('?')
					split2 = expr.index(':')
					split3 = expr.index(';')
					specialExpr = expr[split1 + 1 : split2].strip()
					restExpr = expr[split2 + 1 : split3].strip()
					first = [ x for x in range(1 << 4) if x & (1 << subCase) ]
					rest = [ x for x in range(1 << 4) if x not in first ]
					self.__addCases(cases, first, subPixel, specialExpr)
					self.__addCases(cases, rest, subPixel, restExpr)
			elif line.startswith('break'):
				cases = []

	def __addCases(self, cases, subCases, subPixel, expr):
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
				pixelExpr[(case << 4) | subCase][subPixel] = weights

class Variant(object):

	def __init__(self, pixelExpr, lite, narrow, table):
		self.lite = lite
		self.narrow = narrow
		self.table = table
		if lite:
			pixelExpr = makeLite(pixelExpr, (1, 3) if table else ())
		if narrow:
			pixelExpr = makeNarrow(pixelExpr)
		if table:
			pixelExpr = permuteCases(
				(5, 0, 4, 6, 3, 10, 11, 2, 1, 9, 8, 7),
				pixelExpr
				)
		self.pixelExpr = pixelExpr

	def writeSwitch(self, fileName):
		writeTextFile(fileName, genSwitch(self.pixelExpr, self.narrow))

if __name__ == '__main__':
	parser = Parser()

	#pixelExpr = Variant(parser.pixelExpr, lite = False, narrow = False, table = True ).pixelExpr
	#printText(formatOffsetsTable(computeOffsets(pixelExpr)))
	#print '//-------------'
	#printText(formatWeightsTable(computeWeights(pixelExpr)))
	#parser = Parser()
	#printText(formatWeightsTable(computeLiteWeights(
		#Variant(parser.pixelExpr, lite = True,  narrow = False, table = True ).pixelExpr
		#)))

	fullTableVariant = Variant(parser.pixelExpr, lite = False, narrow = False, table = True )
	liteTableVariant = Variant(parser.pixelExpr, lite = True,  narrow = False, table = True )
	writeBinaryFile(
		'HQ2xOffsets.dat',
		byteStream(computeOffsets(fullTableVariant.pixelExpr))
		)
	writeBinaryFile(
		'HQ2xWeights.dat',
		byteStream(computeWeights(fullTableVariant.pixelExpr))
		)
	writeBinaryFile(
		'HQ2xLiteOffsets.dat',
		byteStream(genHQLiteOffsetsTable(liteTableVariant.pixelExpr))
		)
	# Note: HQ2xLiteWeights.dat is not needed, since interpolated texture
	#       offsets can perform all the blending we need.
	#writeBinaryFile(
	#	'HQ2xLiteWeights.dat',
	#	byteStream(computeLiteWeights(liteTableVariant.pixelExpr))
	#	)

	Variant(parser.pixelExpr, lite = False, narrow = False, table = False).writeSwitch(
		'HQ2xScaler-1x1to2x2.nn'
		)
	Variant(parser.pixelExpr, lite = False, narrow = True,  table = False).writeSwitch(
		'HQ2xScaler-1x1to1x2.nn'
		)
	Variant(parser.pixelExpr, lite = True,  narrow = False, table = False).writeSwitch(
		'HQ2xLiteScaler-1x1to2x2.nn'
		)
	Variant(parser.pixelExpr, lite = True,  narrow = True,  table = False).writeSwitch(
		'HQ2xLiteScaler-1x1to1x2.nn'
		)
