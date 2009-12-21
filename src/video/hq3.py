# $Id$

from hqcommon import (
	BaseParser,
	blendWeights, computeLiteWeightCells, computeNeighbours, computeOffsets,
	computeWeights, computeWeightCells, formatOffsetsTable, formatWeightsTable,
	genHQLiteOffsetsTable, genSwitch, makeLite, permuteCases,
	printSubExpr, printText, writeBinaryFile, writeTextFile
	)

from itertools import izip

class Parser(BaseParser):

	@staticmethod
	def _parseSubPixel(name):
		subPixel = int(name) - 1
		assert 0 <= subPixel < 9
		assert subPixel != 5 - 1
		return subPixel

	def __init__(self):
		self.fileName = 'HQ3xScaler.in'
		self.pixelExpr = [ [ None ] * 9 for _ in range(1 << 12) ]
		self._parse()
		self._sanityCheck()

	def _parse(self):
		BaseParser._parse(self)
		for expr in self.pixelExpr:
			assert expr[4] is None
			expr[4] = (0, 0, 0, 0, 1, 0, 0, 0, 0)

	def _sanityCheck(self):
		BaseParser._sanityCheck(self)

		# Subpixel depends only on the center and three neighbours in the
		# direction of the subpixel itself.
		subsets = (
			(0, 1, 3, 4), (1, 4), (1, 2, 4, 5),
			(3, 4), (4, ), (4, 5),
			(3, 4, 6, 7), (4, 7), (4, 5, 7, 8),
			)
		for case, expr in enumerate(self.pixelExpr):
			for weights, subset in izip(expr, subsets):
				for neighbour in range(9):
					if neighbour not in subset:
						assert weights[neighbour] == 0, (
							case, neighbour, weights
							)

def makeNarrow(pixelExpr):
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

class Variant(object):

	def __init__(self, pixelExpr, lite, narrow, table):
		self.lite = lite
		self.narrow = narrow
		self.table = table
		if lite:
			pixelExpr = makeLite(pixelExpr, (2, 5, 8) if table else ())
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

	Variant(parser.pixelExpr, lite = False, narrow = False, table = False).writeSwitch(
		'HQ3xScaler-1x1to3x3.nn'
		)
	Variant(parser.pixelExpr, lite = True,  narrow = False, table = False).writeSwitch(
		'HQ3xLiteScaler-1x1to3x3.nn'
		)
