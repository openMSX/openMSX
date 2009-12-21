# $Id$

from hqcommon import (
	BaseParser,
	computeLiteWeightCells, computeNeighbours, computeOffsets,
	computeWeights, computeWeightCells, formatOffsetsTable, formatWeightsTable,
	genHQLiteOffsetsTable, genSwitch, makeLite, permuteCases,
	printSubExpr, printText, writeBinaryFile
	)

class Parser(BaseParser):

	@staticmethod
	def _parseSubPixel(name):
		subPixel = int(name, 16)
		assert 0 <= subPixel < 16
		return subPixel

	def __init__(self):
		self.fileName = 'HQ4xScaler.in'
		self.pixelExpr = [ [ None ] * 16 for _ in range(1 << 12) ]
		self._parse()
		self._sanityCheck()

class Variant(object):

	def __init__(self, pixelExpr, lite, narrow, table):
		self.lite = lite
		self.narrow = narrow
		self.table = table
		if lite:
			pixelExpr = makeLite(pixelExpr, (2, 3, 6, 7, 10, 11, 14, 15) if table else ())
		if narrow:
			assert False
		pixelExpr = permuteCases(
			(5, 0, 4, 6, 3, 10, 11, 2, 1, 9, 8, 7)
			if table else
			(2, 9, 7, 4, 3, 10, 11, 1, 8, 0, 6, 5),
			pixelExpr
			)
		self.pixelExpr = pixelExpr

if __name__ == '__main__':
	parser = Parser()

	fullTableVariant = Variant(parser.pixelExpr, lite = False, narrow = False, table = True )
	liteTableVariant = Variant(parser.pixelExpr, lite = True,  narrow = False, table = True )

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
		#parser.pixelExpr, lite = False, narrow = False, table = False
		#).pixelExpr))
