# To run:
#   python3 hq.py
#
# To profile:
#   python3 -m cProfile -s cumulative hq.py > hq-profile.txt

from hq_gen import (
	edges, expandQuadrant, genExpr2, genExpr3, genExpr4,
	permuteCase, simplifyWeights
	)

from collections import defaultdict
from itertools import count
from math import sqrt
import sys

# Variant classes:

class Variant(object):

	def __init__(self, pixelExpr, lite, table, narrow = None):
		self.lite = lite
		self.table = table
		if lite:
			pixelExpr = makeLite(pixelExpr, table)
		if narrow is not None:
			pixelExpr = narrow(pixelExpr)
                #   0 | 1 | 2
                #  ---+---+---
                #   3 | 4 | 5
                #  ---+---+---
                #   6 | 7 | 8
                # For the openGL shaders we want the following order:
                #   (4,6), (3,7), (3,4), (1,3), (0,4), (4,7), (1,4), (5,7), (4,8), (4,5), (2,4), (1,5)
                #   see comments in src/video/scalers/HQCommon.hh for why this order was chosen.
                # Compare this to the original order in 'edges' in hq_gen.py.
                # To switch between these different orderings we use the following permutations.
		pixelExpr = permuteCases(
			(9, 2, 7, 3, 4, 10, 5, 1, 11, 8, 6, 0)  # openGL
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

def writeTextFile(fileName, contents):
	with open(fileName, 'w') as out:
		for content in contents:
			out.write(content)

def writeBinaryFile(fileName, data):
	with open(fileName, 'wb') as out:
		out.write(bytes(data))

# Output as switch statement:

def genSwitch(pixelExpr):
	# Note: In practice, only the center subpixel of HQ3x is independent of
	#       the edge bits. So maybe this is a bit overengineered, but it does
	#       express why this optimization is correct.
	subExprForSubPixel = tuple(
		{expr[subPixel] for expr in pixelExpr if expr[subPixel] is not None}
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
		for expr, cases in exprToCases.items()
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
	wSum = sum(weights)
	assert isPow2(wSum)
	if wSum == 1:
		index, = (index for index, weight in enumerate(weights) if weight != 0)
		return 'c%d' % (index + 1)
	elif wSum <= 8:
		# Because the lower 3 bits of each colour component (R,G,B)
		# are zeroed out, we can operate on a single integer as if it
		# is a vector.
		return ' + '.join(
			'(c%d / %d) * %d' % (index + 1, wSum, weight)
			for index, weight in enumerate(weights)
			if weight != 0
			)
	else:
		return '((%s) & 0xFF00FF00) | (((%s) / %d) & 0x00FF00FF)' % (
			' + '.join(
				'((c%d & 0xFF00FF00) / %d) * %d' % (index + 1, wSum, weight)
				for index, weight in enumerate(weights)
				if weight != 0
				),
			' + '.join(
				'(c%d & 0x00FF00FF) * %d' % (index + 1, weight)
				for index, weight in enumerate(weights)
				if weight != 0
				),
			wSum
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
	while num & 1 == 0:
		num >>= 1
	return num == 1

def permuteCases(permutation, pixelExpr):
	return [
		pixelExpr[permuteCase(case, permutation)]
		for case in range(len(pixelExpr))
		]

def computeNeighbours(weights):
	neighbours = [ i for i in range(9) if i != 4 and weights[i] != 0 ]
	assert len(neighbours) <= 2
	neighbours += [ None ] * (2 - len(neighbours))
	return neighbours

def transformOffsets(weights):
	for neighbour in computeNeighbours(weights):
		yield (
			min(255, (1 if neighbour is None else neighbour % 3) * 128),
			min(255, (1 if neighbour is None else neighbour // 3) * 128)
			)

def transformWeights(weights, cellFunc):
	scaledWeights = scaleWeights(weights, 256)
	for cell in cellFunc(scaledWeights):
		yield min(255, 0 if cell is None else scaledWeights[cell])

def computeOffsets(pixelExpr):
	'''Computes offsets for the fragment shader.
	Output is a 64N * 64N texture, where N is the zoom factor.
	'''
	zoom = getZoom(pixelExpr)
	for caseMajor in range(0, len(pixelExpr), 64):
		for subY in range(0, zoom ** 2, zoom):
			for caseMinor in range(64):
				for subX in range(zoom):
					weights = pixelExpr[caseMajor + caseMinor][subY + subX]
					for x, y in transformOffsets(weights):
						yield x
						yield y

def computeWeights(pixelExpr, cellFunc):
	'''Computes weights for the fragment shader.
	Output is a 64N * 64N texture, where N is the zoom factor.
	'''
	zoom = getZoom(pixelExpr)
	for caseMajor in range(0, len(pixelExpr), 64):
		for subY in range(0, zoom ** 2, zoom):
			for caseMinor in range(64):
				for subX in range(zoom):
					weights = pixelExpr[caseMajor + caseMinor][subY + subX]
					for tw in transformWeights(weights, cellFunc):
						yield tw

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

def scaleWeights(weights, newSum):
	'''Returns the given weights, scaled such that their sum is the given one.
	'''
	oldSum = sum(weights)
	factor, remainder = divmod(newSum, oldSum)
	assert remainder == 0
	return tuple(weight * factor for weight in weights)

def blendWeights(weights1, weights2, factor1 = 1, factor2 = 1):
	factor1 *= sum(weights2)
	factor2 *= sum(weights1)
	return simplifyWeights(
		factor1 * w1 + factor2 * w2
		for w1, w2 in zip(weights1, weights2)
		)

def calcNeighbourToSet():
	# Compute equivalence classes.
	ret = []
	for case in range(1 << len(edges)):
		if isContradiction(case):
			ret.append(None)
		else:
			neighbourToSet = [{neighbour} for neighbour in range(9)]
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

def lighten(case, weights, neighbourPreference):
	equalNeighboursOf = neighbourToSet[case]
	if equalNeighboursOf is None:
		return None
	else:
		done = set()
		newWeights = [ 0 ] * 9
		for neighbour in neighbourPreference:
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
	zoom = getZoom(pixelExpr)
	biasLeft  = (4, 3, 5, 1, 7, 0, 2, 6, 8)
	biasRight = (4, 5, 3, 1, 7, 2, 0, 8, 6)
	if biased:
		center = (zoom - 1) / 2
		neighbourPreferences = [
			biasRight if subPixel % zoom > center else biasLeft
			for subPixel in range(zoom ** 2)
			]
	else:
		neighbourPreferences = [ biasLeft ] * (zoom ** 2)
	return [
		[ lighten(case, weights, pref)
		  for weights, pref in zip(expr, neighbourPreferences) ]
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
	Output is a 64N * 64N texture, where N is the zoom factor.
	'''
	zoom = getZoom(pixelExpr)
	for caseMajor in range(0, len(pixelExpr), 64):
		for subY in range(zoom):
			for caseMinor in range(64):
				for subX in range(zoom):
					subPixel = zoom * subY + subX
					weights = pixelExpr[caseMajor + caseMinor][subPixel]
					if weights is None:
						neighbour = None
					else:
						neighbours = computeNeighbours(weights)
						assert neighbours[1] is None, neighbours
						neighbour = neighbours[0]
						factor = sum(weights)

					x = int(192.5 - 128 * (0.5 + subX) / zoom)
					y = int(192.5 - 128 * (0.5 + subY) / zoom)
					if neighbour == 3:
						x -= 128 * weights[3] // factor
					elif neighbour == 5:
						x += 128 * weights[5] // factor
					else:
						assert neighbour is None, neighbour
					assert 0 <= x < 256, x
					assert 0 <= y < 256, y
					yield x
					yield y

# Main:

def process2x():
	pixelExpr = expandQuadrant(genExpr2(), 2)

	fullTableVariant = Variant(pixelExpr, lite = False, table = True)
	liteTableVariant = Variant(pixelExpr, lite = True,  table = True)

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

	Variant(pixelExpr, lite = False, table = False).writeSwitch(
		'HQ2xScaler-1x1to2x2.nn'
		)
	Variant(pixelExpr, lite = False, table = False, narrow = makeNarrow2to1).writeSwitch(
		'HQ2xScaler-1x1to1x2.nn'
		)
	Variant(pixelExpr, lite = True,  table = False).writeSwitch(
		'HQ2xLiteScaler-1x1to2x2.nn'
		)
	Variant(pixelExpr, lite = True,  table = False, narrow = makeNarrow2to1).writeSwitch(
		'HQ2xLiteScaler-1x1to1x2.nn'
		)

def process3x():
	pixelExpr = expandQuadrant(genExpr3(), 3)

	fullTableVariant = Variant(pixelExpr, lite = False, table = True)
	liteTableVariant = Variant(pixelExpr, lite = True,  table = True)

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

	Variant(pixelExpr, lite = False, table = False).writeSwitch(
		'HQ3xScaler-1x1to3x3.nn'
		)
	Variant(pixelExpr, lite = True,  table = False).writeSwitch(
		'HQ3xLiteScaler-1x1to3x3.nn'
		)

def process4x():
	pixelExpr = expandQuadrant(genExpr4(), 4)

	fullTableVariant = Variant(pixelExpr, lite = False, table = True)
	liteTableVariant = Variant(pixelExpr, lite = True,  table = True)

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
		#pixelExpr, lite = False, table = False
		#).pixelExpr))

if __name__ == '__main__':
	process2x()
	process3x()
	process4x()
