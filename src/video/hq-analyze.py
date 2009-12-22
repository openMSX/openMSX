# $Id$

from hq import Parser2x, Parser3x, Parser4x, edges, getZoom

from itertools import izip

def permute(seq, permutation):
	seq = tuple(seq)
	assert len(seq) == len(permutation)
	return tuple(seq[index] for index in permutation)

def permuteCase(case, permutation):
	return sum(
		((case >> newBit) & 1) << oldBit
		for newBit, oldBit in enumerate(permutation)
		)

def extractTopLeftQuadrant(pixelExpr):
	zoom = getZoom(pixelExpr)
	quadrantWidth = (zoom + 1) / 2
	quadrantMap = [
		qy * zoom + qx
		for qy in xrange(quadrantWidth)
		for qx in xrange(quadrantWidth)
		]
	return [
		[ expr[subPixel] for subPixel in quadrantMap ]
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
		for ty, py in ((qy, permId), (zoom - qy - 1, permTB)):
			for tx, px in ((qx, permId), (zoom - qx - 1, permLR)):
				nperm = permute(px, py)
				cperm = [
					edges.index((nperm[n1], nperm[n2]))
					for n1, n2 in edges
					]
				mirrorMap[ty * zoom + tx] = (quadrantIndex, cperm, nperm)
	return [
		[	permute(
				topLeftQuadrant[permuteCase(case, cperm)][quadrantIndex],
				nperm
				)
			for quadrantIndex, cperm, nperm in mirrorMap
			]
		for case in xrange(len(topLeftQuadrant))
		]

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

def checkQuadrants():
	for parserClass in (Parser2x, Parser3x, Parser4x):
		parser = parserClass()
		topLeftQuadrant = extractTopLeftQuadrant(parser.pixelExpr)
		expanded = expandQuadrant(topLeftQuadrant, parser.zoom)
		comparePixelExpr(expanded, parser.pixelExpr)

if __name__ == '__main__':
	checkQuadrants()
