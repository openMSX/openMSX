# $Id$

from hq import (
	Parser2x, Parser3x, Parser4x,
	edges, getZoom, permuteCase, simplifyWeights
	)

from itertools import izip

def computeCasePermutation(neighbourPermutation):
	return tuple(
		edges.index(tuple(sorted(
			(neighbourPermutation[n1], neighbourPermutation[n2])
			)))
		for n1, n2 in edges
		)

def permute(seq, permutation):
	seq = tuple(seq)
	assert len(seq) == len(permutation)
	return tuple(seq[index] for index in permutation)

def extractTopLeftWeights(weights):
	assert all(weights[n] == 0 for n in (2, 5, 6, 7, 8)), weights
	return weights[0 : 2] + weights[3 : 5]

def expandTopLeftWeights(weights):
	return weights[0 : 2] + (0, ) + weights[2 : 4] + (0, 0, 0, 0)

def extractTopLeftQuadrant(pixelExpr):
	zoom = getZoom(pixelExpr)
	quadrantWidth = (zoom + 1) / 2
	quadrantMap = [
		qy * zoom + qx
		for qy in xrange(quadrantWidth)
		for qx in xrange(quadrantWidth)
		]
	for expr in [
		[ expr[subPixel] for subPixel in quadrantMap ]
		for expr in pixelExpr
		]:
		for weights in expr:
			for neighbour in (2, 5, 6, 7, 8):
				assert weights[neighbour] == 0, weights
	return [
		[ extractTopLeftWeights(expr[subPixel]) for subPixel in quadrantMap ]
		for expr in pixelExpr
		]

def expandQuadrant(topLeftQuadrant, zoom):
	quadrantWidth = (zoom + 1) / 2
	assert quadrantWidth ** 2 == len(topLeftQuadrant[0])
	mirrorMap = [None] * (zoom ** 2)
	permId = (0, 1, 2, 3, 4, 5, 6, 7, 8)
	permLR = (2, 1, 0, 5, 4, 3, 8, 7, 6)
	permTB = (6, 7, 8, 3, 4, 5, 0, 1, 2)
	for quadrantIndex in xrange(quadrantWidth ** 2):
		qy, qx = divmod(quadrantIndex, quadrantWidth)
		for ty, py in ((zoom - qy - 1, permTB), (qy, permId)):
			for tx, px in ((zoom - qx - 1, permLR), (qx, permId)):
				nperm = permute(px, py)
				cperm = computeCasePermutation(nperm)
				mirrorMap[ty * zoom + tx] = (quadrantIndex, cperm, nperm)
	return [
		[	permute(
				expandTopLeftWeights(
					topLeftQuadrant[permuteCase(case, cperm)][quadrantIndex]
					),
				nperm
				)
			for quadrantIndex, cperm, nperm in mirrorMap
			]
		for case in xrange(len(topLeftQuadrant))
		]

def convertExpr4to2(case, expr4):
	weights2 = [0] * 4
	for weights4 in expr4:
		wsum = sum(weights4)
		assert 256 % wsum == 0
		factor = 256 / wsum
		for neighbour in range(4):
			weights2[neighbour] += weights4[neighbour] * factor
	weights2 = simplifyWeights(weights2)
	if ((case >> 4) & 15) in (2, 6, 8, 12):
		assert sorted(weights2) == [0, 2, 7, 23]
		weightMap = { 0: 0, 2: 1, 7: 1, 23: 2 }
	elif ((case >> 4) & 15) in (0, 1, 4, 5):
		assert sorted(weights2) == [0, 3, 3, 10]
		weightMap = { 0: 0, 3: 1, 10: 2 }
	else:
		weightMap = None
	if weightMap:
		weights2 = tuple(weightMap[weight] for weight in weights2)
	return [weights2]

def convert4to2(topLeftQuadrant4):
	return [
		convertExpr4to2(case, expr4)
		for case, expr4 in enumerate(topLeftQuadrant4)
		]

# Various analysis:

def findRelevantEdges():
	for parserClass in (Parser2x, Parser3x, Parser4x):
		parser = parserClass()
		topLeftQuadrant = extractTopLeftQuadrant(parser.pixelExpr)
		for subPixel in xrange(len(topLeftQuadrant[0])):
			irrelevant = [
				edgeNum
				for edgeNum in xrange(12)
				if all(
					topLeftQuadrant[case][subPixel] ==
						topLeftQuadrant[case ^ (1 << edgeNum)][subPixel]
					for case in xrange(len(topLeftQuadrant))
					)
				]
			print 'zoom %d: irrelevant edges for subpixel %d: %s' % (
				parser.zoom,
				subPixel,
				', '.join(str(edges[i]) for i in irrelevant)
				)
	# Result: Only edge 5-7 is irrelevant for the top-left quadrant.
	#         So it is not possible to simplify significantly by removing
	#         irrelevant edges from the edge bits.

# Visualization:

def formatWeights(weights):
	return ' '.join('%3d' % weight for weight in weights)

def comparePixelExpr(pixelExpr1, pixelExpr2):
	zoom = getZoom(pixelExpr1)
	assert zoom == getZoom(pixelExpr2)
	mismatchCount = 0
	for case, (expr1, expr2) in enumerate(izip(pixelExpr1, pixelExpr2)):
		if expr1 != expr2:
			binStr = bin(case)[2 : ].zfill(12)
			print 'case: %d (%s)' % (
				case,
				' '.join(binStr[i : i + 4] for i in range(0, 12, 4))
				)
			for sy in xrange(zoom):
				matrices = []
				for sx in xrange(zoom):
					subPixel = sy * zoom + sx
					subExpr1 = expr1[subPixel]
					subExpr2 = expr2[subPixel]
					rows = []
					for ny in xrange(3):
						rows.append('%s   %s %s' % (
							formatWeights(subExpr1[ny * 3 : (ny + 1) * 3]),
							'.' if subExpr1 == subExpr2 else '!',
							formatWeights(subExpr2[ny * 3 : (ny + 1) * 3])
							))
					matrices.append(rows)
				for ny in xrange(3):
					print '  %s' % '       '.join(
						matrices[sx][ny]
						for sx in xrange(zoom)
						)
				print
			mismatchCount += 1
	print 'Number of mismatches: %d' % mismatchCount

# Sanity checks:

def checkQuadrants():
	for parserClass in (Parser2x, Parser3x, Parser4x):
		parser = parserClass()
		topLeftQuadrant = extractTopLeftQuadrant(parser.pixelExpr)
		expanded = expandQuadrant(topLeftQuadrant, parser.zoom)
		comparePixelExpr(expanded, parser.pixelExpr)

def checkConvert4to2():
	parser4 = Parser4x()
	topLeftQuadrant4 = extractTopLeftQuadrant(parser4.pixelExpr)
	topLeftQuadrant2 = convert4to2(topLeftQuadrant4)
	pixelExpr2 = expandQuadrant(topLeftQuadrant2, 2)

	parser2 = Parser2x()
	comparePixelExpr(pixelExpr2, parser2.pixelExpr)

# Main:

if __name__ == '__main__':
	checkQuadrants()
	checkConvert4to2()
	findRelevantEdges()
