# $Id$

from collections import defaultdict
from itertools import izip
import sys

# I/O:

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

def writeBinaryFile(fileName, bytes):
	writeFile(fileName, 'wb', ( chr(byte) for byte in bytes ))

# Output as switch statement:

def genSwitch(pixelExpr):
	# Note: In practice, only the center subpixel of HQ3x is independent of
	#       the edge bits. So maybe this is a bit overengineered, but it does
	#       express why this optimization is correct.
	subExprForSubPixel = tuple(
		# TODO: Make the parser return tuples.
		set(tuple(expr[subPixel]) for expr in pixelExpr)
		for subPixel in range(len(pixelExpr[0]))
		)
	isIndependentSubPixel = tuple(
		len(subExprs) == 1
		for subExprs in subExprForSubPixel
		)

	exprToCases = defaultdict(list)
	for case, expr in enumerate(pixelExpr):
		exprToCases[tuple(tuple(subExpr) for subExpr in expr)].append(case)
	yield 'switch (pattern) {\n'
	for cases, expr in sorted(
		( sorted(cases), expr )
		for expr, cases in exprToCases.iteritems()
		):
		for case in cases:
			yield 'case %d:\n' % case
		for subPixel, subExpr in enumerate(expr):
			if not isIndependentSubPixel[subPixel]:
				yield '\tpixel%s = %s;\n' % (
					hex(subPixel)[2 : ], printSubExpr(subExpr)
					)
		yield '\tbreak;\n'
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
				hex(subPixel)[2 : ], printSubExpr(subExpr)
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

def printSubExpr(subExpr):
	wsum = sum(subExpr)
	if not isPow2(wsum):
		return (
			'((' + ' + '.join(
				'(c%d & 0x0000FF) * %d' % (index + 1, weight)
				for index, weight in enumerate(subExpr)
				if weight != 0
				) +
			') / %d)' % wsum +
			' | ' +
			'(((' + ' + '.join(
				'(c%d & 0x00FF00) * %d' % (index + 1, weight)
				for index, weight in enumerate(subExpr)
				if weight != 0
				) +
			') / %d) & 0x00FF00)' % wsum +
			' | ' +
			'(((' + ' + '.join(
				'(c%d & 0xFF0000) * %d' % (index + 1, weight)
				for index, weight in enumerate(subExpr)
				if weight != 0
				) +
			') / %d) & 0xFF0000)' % wsum
			)
	elif wsum == 1:
		#assert subExpr[5 - 1] == 1, subExpr
		nonZeroIndices = [ i for i in range(9) if subExpr[i] != 0 ]
		assert len(nonZeroIndices) == 1
		return 'c%s' % (nonZeroIndices[0] + 1)
	elif wsum <= 8:
		# Because the lower 3 bits of each colour component (R,G,B)
		# are zeroed out, we can operate on a single integer as if it
		# is a vector.
		return (
			'(' +
			' + '.join(
				'c%d * %d' % (index + 1, weight)
				for index, weight in enumerate(subExpr)
				if weight != 0
			) +
			') / %d' % wsum
			)
	else:
		return (
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
			')) / %d' % wsum
			)

def isContradiction(case):
	inv = case ^ 0xFFF
	return (
		(inv & 0x121) in [0x120, 0x101, 0x021] or
		(inv & 0x502) in [0x500, 0x402, 0x102] or
		(inv & 0x484) in [0x480, 0x404, 0x084] or
		(inv & 0x0A8) in [0x0A0, 0x088, 0x028] or
		(inv & 0x00F) in [0x00E, 0x00D, 0x00B, 0x007]
		)

def simplifyWeights2(weights):
	for w in range(9):
		if weights[w] % 2:
			return weights
	for w in range(9):
		weights[w] /= 2
	return simplifyWeights2(weights)

def simplifyWeights3(weights):
	for w in range(9):
		if weights[w] % 3:
			return weights
	for w in range(9):
		weights[w] /= 3
	return simplifyWeights3(weights)

def simplifyWeights(weights):
	return simplifyWeights3(simplifyWeights2(weights))

def blendWeights(weights1, weights2, factor1 = 1, factor2 = 1):
	factor1 *= sum(weights2)
	factor2 *= sum(weights1)
	return simplifyWeights([
		factor1 * w1 + factor2 * w2
		for w1, w2 in izip(weights1, weights2)
		])

def makeLite(pixelExpr, preferC6subPixels):
	'''
			if pix1 == 2 and pix2 == 6:
				subCase = 0
			elif pix1 == 6 and pix2 == 8:
				subCase = 1
			elif pix1 == 8 and pix2 == 4:
				subCase = 2
			elif pix1 == 4 and pix2 == 2:
				subCase = 3
	'''
	edges = [[5, 1], [5, 7], [3, 7], [3, 1],
	         [4, 0], [4, 1], [4, 2], [4, 3],
	         [4, 5], [4, 6], [4, 7], [4, 8]]
	# Sanity check:
	# To allow the weight transfers to be done in a single pass, no pixel
	# should be transferred to when it has already been used to transfer from.
	beenSrc = set()
	for dst, src in edges:
		assert dst not in beenSrc
		beenSrc.add(src)

	def lighten(case, subPixel, weights):
		#print "case:", case, "subPixel:", subPixel, "weights:", weights
		if isContradiction(case):
			newWeights = [0, 0, 0, 0, 1, 0, 0, 0, 0]
		else:
			# simplify using edge info
			pixelToSet = dict( ( pixel, set([pixel]) ) for pixel in range(9) )
			for edge, (pixel1, pixel2) in enumerate(edges):
				if (case & (1 << edge)) == 0:
					# No edge, so the two pixels are equal.
					# Merge the sets of equal pixels.
					set1 = pixelToSet[pixel1]
					set2 = pixelToSet[pixel2]
					set1 |= set2
					for p in set1:
						pixelToSet[p] = set1
			done = set()
			newWeights = [ 0 ] * 9
			if subPixel in preferC6subPixels:
				rem = ( 4, 5, 3, 1, 7, 0, 2, 6, 8 )
			else:
				rem = ( 4, 3, 5, 1, 7, 0, 2, 6, 8 )
			for remPixel in rem:
				if remPixel not in done:
					equalPixels = pixelToSet[remPixel]
					newWeights[remPixel] = sum(
						weights[orgPixel] for orgPixel in equalPixels
						)
					done |= equalPixels
		newWeights = simplifyWeights(newWeights)
		for c in (0, 1, 2, 6, 7, 8):
			# only c4, c5, c6 have non-0 weight
			assert newWeights[c] == 0
		#print "case:", case, "subPixel:", subPixel, "weights:", newWeights
		return newWeights

	return [
		[ lighten(case, subPixel, weights)
		  for subPixel, weights in enumerate(expr) ]
		for case, expr in enumerate(pixelExpr)
		]
