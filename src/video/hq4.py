# $Id$

from hqcommon import (
	BaseParser,
	computeLiteWeightCells, computeNeighbours, computeOffsets,
	computeWeights, computeWeightCells, formatOffsetsTable, formatWeightsTable,
	genHQLiteOffsetsTable, genSwitch, makeLite, permuteCases,
	printSubExpr, printText, writeBinaryFile
	)

class Parser(BaseParser):

	def __init__(self):
		self.fileName = 'HQ4xScaler.in'
		self.pixelExpr = [ [ None ] * 16 for _ in range(1 << 12) ]
		self.__parse()
		sanityCheck(self.pixelExpr)

	def __parse(self):
		cases = []
		subCases = range(1 << 4)
		for line in self._filterSwitch(file(self.fileName)):
			if line.startswith('case'):
				cases.append(int(line[5 : line.index(':', 5)]))
			elif line.startswith('pixel'):
				subPixel = int(line[5], 16)
				assert 0 <= subPixel < 16
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

def sanityCheck(pixelExpr):
	'''Check various observed properties.
	'''
	subsets = [ (5, 4, 2, 1), (5, 6, 2, 3), (5, 4, 8, 7), (5, 6, 8, 9) ]

	for case, expr in enumerate(pixelExpr):
		for corner in expr:
			# Weight of the center pixel is never zero.
			# TODO: This is not the case for 4x, is that expected or a problem?
			#assert corner[4] != 0
			# Sum of weight factors is always a power of two.
			total = sum(corner)
			assert total in (1, 2, 4, 8, 16), total
			# There are at most 3 non-zero weights, and if there are 3 one of
			# those must be for the center pixel.
			numNonZero = sum(weight != 0 for weight in corner)
			assert numNonZero <= 3, (case, corner)
			assert numNonZero < 3 or corner[4] != 0

		# Subpixel depends only on the center and three neighbours in the
		# direction of the subpixel itself.
		# TODO: This is not the case for 4x, is that expected or a problem?
		#for corner, subset in zip(expr, subsets):
			#for pixel in range(9):
				#if (pixel + 1) not in subset:
					#assert corner[pixel] == 0, corner

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
