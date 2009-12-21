# $Id$

from hqcommon import (
	BaseParser,
	blendWeights, computeLiteWeightCells, computeNeighbours, computeOffsets,
	computeWeights, computeWeightCells, formatOffsetsTable, formatWeightsTable,
	genHQLiteOffsetsTable, genSwitch, makeLite, permuteCases,
	printSubExpr, printText, writeBinaryFile, writeTextFile
	)

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
		'''Check various observed properties.
		'''
		subsets = [ (5, 4, 2, 1), (5, 6, 2, 3), (5, 4, 8, 7), (5, 6, 8, 9) ]

		for case, expr in enumerate(self.pixelExpr):
			for corner in expr:
				# Weight of the center pixel is never zero.
				# TODO: This is not the case for 3x, is that expected or a problem?
				#assert corner[4] != 0
				# Sum of weight factors is always a power of two.
				total = sum(corner)
				assert total in (1, 2, 4, 8, 16), total
				# There are at most 3 non-zero weights, and if there are 3,
				# one of those must be for the center pixel.
				numNonZero = sum(weight != 0 for weight in corner)
				assert numNonZero <= 3, (case, corner)
				assert numNonZero < 3 or corner[4] != 0

			# Subpixel depends only on the center and three neighbours in the
			# direction of the subpixel itself.
			# TODO: This is not the case for 3x, is that expected or a problem?
			#for corner, subset in zip(expr, subsets):
				#for pixel in range(9):
					#if (pixel + 1) not in subset:
						#assert corner[pixel] == 0, corner

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
