# $Id$

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

def resetContradictions(pixelExpr):
	count = 0
	default = [0, 0, 0, 0, 1, 0, 0, 0, 0]
	for case in range(1 << 12):
		inv = case ^ 0xFFF
		if (((inv & 0x121) in [0x120, 0x101, 0x021]) or
		    ((inv & 0x502) in [0x500, 0x402, 0x102]) or
		    ((inv & 0x484) in [0x480, 0x404, 0x084]) or
		    ((inv & 0x0A8) in [0x0A0, 0x088, 0x028]) or
		    ((inv & 0x00F) in [0x00E, 0x00D, 0x00B, 0x007])):
			for subPixel in range(len(pixelExpr[case])):
				pixelExpr[case][subPixel] = default
			count += 1
	#print "Eliminated", count, "contradictions"

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

def makeLite(pixelExpr, preferC6subPixels = ()):
	resetContradictions(pixelExpr)
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
	edges = [[6, 2], [6, 8], [4, 8], [4, 2],
	         [5, 1], [5, 2], [5, 3], [5, 4],
	         [5, 6], [5, 7], [5, 8], [5, 9]]
	# Sanity check:
	# To allow the weight transfers to be done in a single pass, no pixel
	# should be transferred to when it has already been used to transfer from.
	beenSrc = set()
	for dst, src in edges:
		assert dst not in beenSrc
		beenSrc.add(src)

	for case in range(1 << 12):
		for subPixel in range(len(pixelExpr[case])):
			weights = pixelExpr[case][subPixel]
			#print "case:", case, "subPixel:", subPixel, "weights:", weights
			# simplify using edge info
			pixelToSet = dict( ( pixel, set([pixel]) ) for pixel in range(9) )
			for edge, (pixel1, pixel2) in enumerate(edges):
				pixel1 -= 1
				pixel2 -= 1
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
				rem = ( 5, 6, 4, 2, 8, 1, 3, 7, 9 )
			else:
				rem = ( 5, 4, 6, 2, 8, 1, 3, 7, 9 )
			for remPixel in rem:
				remPixel -= 1
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
			pixelExpr[case][subPixel] = newWeights
			#print "case:", case, "subPixel:", subPixel, "weights:", newWeights
