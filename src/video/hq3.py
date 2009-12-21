# $Id$

from hq import (
	BaseParser,
	blendWeights, computeLiteWeightCells, computeNeighbours, computeOffsets,
	computeWeights, computeWeightCells, formatOffsetsTable, formatWeightsTable,
	genHQLiteOffsetsTable, genSwitch, makeLite, permuteCases,
	printSubExpr, printText, writeBinaryFile, writeTextFile
	)

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

class Variant3x(object):

	def __init__(self, pixelExpr, lite, narrow, table):
		self.lite = lite
		self.narrow = narrow
		self.table = table
		if lite:
			pixelExpr = makeLite(pixelExpr, (2, 5, 8) if table else ())
		if narrow:
			pixelExpr = makeNarrow3to2(pixelExpr)
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
	parser = Parser3x()

	fullTableVariant = Variant3x(parser.pixelExpr, lite = False, narrow = False, table = True )
	liteTableVariant = Variant3x(parser.pixelExpr, lite = True,  narrow = False, table = True )

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

	Variant3x(parser.pixelExpr, lite = False, narrow = False, table = False).writeSwitch(
		'HQ3xScaler-1x1to3x3.nn'
		)
	Variant3x(parser.pixelExpr, lite = True,  narrow = False, table = False).writeSwitch(
		'HQ3xLiteScaler-1x1to3x3.nn'
		)
