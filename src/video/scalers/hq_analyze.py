from hq import getZoom, scaleWeights
from hq_gen import (
	edges, expandQuadrant, genExpr2, genExpr3, genExpr4, simplifyWeights
	)

from collections import defaultdict
from itertools import count

def normalizeWeights(pixelExpr):
	maxSum = max(
		sum(weights)
		for expr in pixelExpr
		for weights in expr
		)
	return [
		[ scaleWeights(weights, maxSum) for weights in expr ]
		for expr in pixelExpr
		]

def extractTopLeftWeights(weights):
	assert all(weights[n] == 0 for n in (2, 5, 6, 7, 8)), weights
	return weights[0 : 2] + weights[3 : 5]

def extractTopLeftQuadrant(pixelExpr):
	zoom = getZoom(pixelExpr)
	quadrantWidth = (zoom + 1) // 2
	quadrantMap = [
		qy * zoom + qx
		for qy in range(quadrantWidth)
		for qx in range(quadrantWidth)
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

def convertExpr4to2(case, expr4):
	weights2 = [0] * 4
	for weights4 in expr4:
		for neighbour, weight in enumerate(scaleWeights(weights4, 256)):
			weights2[neighbour] += weight
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

def analyzeCaseFunction(caseToWeights):
	weightsToCase = defaultdict(set)
	for case, weights in enumerate(caseToWeights):
		weightsToCase[weights].add(case)
	for weights in sorted(weightsToCase.keys()):
		cases = weightsToCase[weights]
		partitions = {
			tuple((case >> edgeNum) & 1 for edgeNum in range(11, -1, -1))
			for case in cases
			}
		# Repeatedly merge partitions until we have a minimal set.
		for edgeNum in range(12):
			changed = True
			while changed:
				changed = False
				for part in list(partitions):
					if part not in partitions:
						continue
					if part[edgeNum] < 2:
						pre = part[ : edgeNum]
						post = part[edgeNum + 1 : ]
						dual = pre + (part[edgeNum] ^ 1,) + post
						if dual in partitions:
							partitions.remove(part)
							partitions.remove(dual)
							partitions.add(pre + (2,) + post)
							changed = True
		yield weights, [
			''.join('01x'[bit] for bit in partition)
			for partition in sorted(partitions)
			]

# Various analysis:

def findRelevantEdges():
	for zoom, genExpr in zip(count(2), (genExpr2, genExpr3, genExpr4)):
		quadrant = genExpr()
		quadrantWidth = (zoom + 1) // 2
		assert quadrantWidth ** 2 == len(quadrant[0])
		subPixelOutput = [[] for _ in range(quadrantWidth * 10)]
		for subPixel in range(quadrantWidth ** 2):
			neighbourOutput = [[] for _ in range(8)]
			for neighbour in range(4):
				relevant = [
					edgeNum
					for edgeNum in range(12)
					if any(
						quadrant[case][subPixel][neighbour] !=
							quadrant[case ^ (1 << edgeNum)][subPixel][neighbour]
						for case in range(len(quadrant))
						)
					]
				zero = len(relevant) == 0 and all(
					quadrant[case][subPixel][neighbour] == 0
					for case in range(len(quadrant))
					)
				center = ('.' if zero else str(neighbour))
				for rowNum, row in enumerate(formatEdges(relevant)):
					if rowNum == 1:
						assert row[1] == 'o'
						row = row[0] + center + row[2]
					neighbourOutput[(neighbour // 2) * 4 + rowNum].append(row)
				neighbourOutput[(neighbour // 2) * 4 + 3].append('   ')
			for lineNum, line in enumerate(neighbourOutput):
				lineOutput = '  %s  |' % '  '.join(line)
				subPixelOutput[
					(subPixel // quadrantWidth) * 10 + lineNum + 1
					].append(lineOutput)
				if lineNum == 7:
					subPixelOutput[(subPixel // quadrantWidth) * 10].append(
						lineOutput
						)
					subPixelOutput[(subPixel // quadrantWidth) * 10 + 9].append(
						'%ss%d' % ('-' * (len(lineOutput) - 2), subPixel)
						)
		print('Relevant edges for zoom %d:' % zoom)
		print()
		for line in subPixelOutput:
			print('  %s' % ''.join(line))
		print()

# Visualization:

def formatEdges(edgeNums):
	cells = ['.' for _ in range(9)]
	cells[4] = 'o'
	def combine(index, ch):
		old = cells[index]
		if old == '.':
			cells[index] = ch
		elif (old == '/' and ch == '\\') or (old == '\\' and ch == '/'):
			cells[index] = 'X'
		else:
			assert False, (index, old, ch)
	for edgeNum in edgeNums:
		edge = edges[edgeNum]
		if 4 in edge:
			other = sum(edge) - 4
			combine(other, '\\|/-'[other if other < 4 else 8 - other])
		else:
			assert edge in ((1, 5), (5, 7), (3, 7), (1, 3))
			x = 0 if 3 in edge else 2
			y = 0 if 1 in edge else 2
			combine(y * 3 + x, '/' if x == y else '\\')
	for y in range(3):
		yield ''.join(cells[y * 3 : (y + 1) * 3])

def formatWeights(weights):
	return ' '.join('%3d' % weight for weight in weights)

def comparePixelExpr(pixelExpr1, pixelExpr2):
	zoom = getZoom(pixelExpr1)
	assert zoom == getZoom(pixelExpr2)
	mismatchCount = 0
	for case, (expr1, expr2) in enumerate(zip(pixelExpr1, pixelExpr2)):
		if expr1 != expr2:
			binStr = bin(case)[2 : ].zfill(12)
			print('case: %d (%s)' % (
				case,
				' '.join(binStr[i : i + 4] for i in range(0, 12, 4))
				))
			for sy in range(zoom):
				matrices = []
				for sx in range(zoom):
					subPixel = sy * zoom + sx
					subExpr1 = expr1[subPixel]
					subExpr2 = expr2[subPixel]
					rows = []
					for ny in range(3):
						rows.append('%s   %s %s' % (
							formatWeights(subExpr1[ny * 3 : (ny + 1) * 3]),
							'.' if subExpr1 == subExpr2 else '!',
							formatWeights(subExpr2[ny * 3 : (ny + 1) * 3])
							))
					matrices.append(rows)
				for ny in range(3):
					print('  %s' % '       '.join(
						matrices[sx][ny]
						for sx in range(zoom)
						))
				print()
			mismatchCount += 1
	print('Number of mismatches: %d' % mismatchCount)

# Sanity checks:

def checkConvert4to2():
	topLeftQuadrant4 = genExpr4()
	topLeftQuadrant2 = convert4to2(topLeftQuadrant4)
	pixelExpr2 = expandQuadrant(topLeftQuadrant2, 2)

	comparePixelExpr(pixelExpr2, expandQuadrant(genExpr2(), 2))

# Main:

if __name__ == '__main__':
	findRelevantEdges()
	checkConvert4to2()
