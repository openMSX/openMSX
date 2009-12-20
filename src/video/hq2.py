# $Id$

from hqcommon import (
	blendWeights, computeLiteWeightCells, computeNeighbours, computeOffsets,
	computeWeights, computeWeightCells, formatOffsetsTable, formatWeightsTable,
	genHQLiteOffsetsTable, genSwitch, makeLite, permuteCases,
	printSubExpr, printText, writeBinaryFile, writeTextFile
	)

from itertools import izip

def sanityCheck(pixelExpr):
	'''Check various observed properties.
	'''
	subsets = [ (5, 4, 2, 1), (5, 6, 2, 3), (5, 4, 8, 7), (5, 6, 8, 9) ]

	for case, expr in enumerate(pixelExpr):
		for corner in expr:
			# Weight of the center pixel is never zero.
			assert corner[4] != 0
			# Sum of weight factors is always a power of two. But 2 doesn't
			# occur for some reason, so there is never an equal blend.
			total = sum(corner)
			assert total in (1, 4, 8, 16), total
			# There are at most 3 non-zero weights, and if there are 3 one of
			# those must be for the center pixel.
			numNonZero = sum(weight != 0 for weight in corner)
			assert numNonZero <= 3, (case, corner)
			assert numNonZero < 3 or corner[4] != 0

		# Subpixel depends only on the center and three neighbours in the
		# direction of the subpixel itself.
		for corner, subset in izip(expr, subsets):
			for pixel in range(9):
				if (pixel + 1) not in subset:
					assert corner[pixel] == 0, corner

def makeNarrow(pixelExpr):
	return [
		[ None, None ] if a is None else
			[ blendWeights(a, b), blendWeights(c, d) ]
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
			if log:
				if 'edge(' in line:
					line += ' ' + stream.next().strip()
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
		pixelExpr = permuteCases(
			(5, 0, 4, 6, 3, 10, 11, 2, 1, 9, 8, 7)
			if table else
			(2, 9, 7, 4, 3, 10, 11, 1, 8, 0, 6, 5),
			pixelExpr
			)
		self.pixelExpr = pixelExpr

	def writeSwitch(self, fileName):
		writeTextFile(fileName, genSwitch(self.pixelExpr))

if __name__ == '__main__':
	parser = Parser()

	fullTableVariant = Variant(parser.pixelExpr, lite = False, narrow = False, table = True )
	liteTableVariant = Variant(parser.pixelExpr, lite = True,  narrow = False, table = True )

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
