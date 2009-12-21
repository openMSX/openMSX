# $Id$

from hqcommon import (
	BaseParser,
	blendWeights, computeLiteWeightCells, computeNeighbours, computeOffsets,
	computeWeights, computeWeightCells, formatOffsetsTable, formatWeightsTable,
	genHQLiteOffsetsTable, genSwitch, makeLite, permuteCases,
	printSubExpr, printText, writeBinaryFile, writeTextFile
	)

def makeNarrow(pixelExpr):
	return tuple(
		(None, None) if a is None else (blendWeights(a, b), blendWeights(c, d))
		for a, b, c, d in pixelExpr
		)

class Parser(BaseParser):

	@staticmethod
	def _parseSubPixel(name):
		subPixel = int(name) - 1
		assert 0 <= subPixel < 4
		return subPixel

	def __init__(self):
		self.fileName = 'HQ2xScaler.in'
		self.pixelExpr = [ [ None ] * 4 for _ in range(1 << 12) ]
		self._parse()
		self._sanityCheck()

	def _sanityCheck(self):
		BaseParser._sanityCheck(self)

		# Weight of the center pixel is never zero.
		for case, expr in enumerate(self.pixelExpr):
			for weights in expr:
				assert weights[4] != 0

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
