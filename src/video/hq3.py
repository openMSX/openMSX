# $Id$

import sys
from hqcommon import *

def filterSwitch(stream):
	log = False
	inIf = False
	for line in stream:
		line = line.strip()
		if line == 'switch (pattern) {':
			log = True
		elif line == '}':
			if inIf:
				inIf = False
			elif log:
				break
		elif line.startswith('//pixel'):
			line = line[2 : ]
		elif line.startswith('if'):
			inIf = True
		if log:
			yield line

def addCases(cases, subCases, subPixel, expr):
	global pixelExpr
	if subPixel > (5 - 1):
		subPixel -= 1
	for case in cases:
		for subCase in subCases:
			weights = [ 0 ] * 9
			if expr.startswith('interpolate'):
				factorsStr = expr[
					expr.index('<') + 1 : expr.index('>')
					].split(',')
				pixelsStr = expr[
					expr.index('(') + 1 : expr.index(')')
					].split(',')
				assert len(factorsStr) == len(pixelsStr)
				for factorStr, pixelStr in zip(factorsStr, pixelsStr):
					factor = int(factorStr)
					pixelStr = pixelStr.strip()
					assert pixelStr[0] == 'c'
					pixel = int(pixelStr[1 : ]) - 1
					weights[pixel] = factor
			else:
				assert expr[0] == 'c'
				weights[int(expr[1 : ]) - 1] = 1
			pixelExpr[(case << 4) | subCase][subPixel] = weights

pixelExpr = [ [ None ] * 8 for _ in range(1 << 12) ]

cases = []
subCases = range(1 << 4)
for line in filterSwitch(file('HQ3xScaler.in')):
	if line.startswith('case'):
		cases.append(int(line[5 : line.index(':', 5)]))
	elif line.startswith('pixel'):
		subPixel = int(line[5]) - 1
		assert 0 <= subPixel < 9
		assert subPixel != (5 - 1)
		expr = line[line.index('=') + 1 : ].strip()
		addCases(cases, subCases, subPixel, expr[ : -1])
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

subsets = [ (5, 4, 2, 1), (5, 6, 2, 3), (5, 4, 8, 7), (5, 6, 8, 9) ]


# Check various observed properties:

for case, expr in enumerate(pixelExpr):
	# Weight factor of center pixel is never zero.
	#for corner in expr:
	#	assert corner[5 - 1] != 0, corner

	# Sum of weight factors is always a power of two.
	for corner in expr:
		total = sum(corner)
		assert total in (1, 2, 4, 8, 16), total
		#
		count = reduce(lambda x, y: x+(y!=0), corner, 0)
		# at most 3 non-zero coef
		assert count <= 3, (case, corner)
		# and if there are 3 one of those must be c5
		assert (count < 3) or (corner[4] != 0)
#
#	# Subpixel depends only on the center and three neighbours in the direction
#	# of the subpixel itself.
#	for corner, subset in zip(expr, subsets):
#		for pixel in range(9):
#			if (pixel + 1) not in subset:
#				#assert corner[pixel] == 0, corner


permutation = (2, 9, 7, 4, 3, 10, 11, 1, 8, 0, 6, 5)    ;#for SDL
#permutation = (5, 0, 4, 6, 3, 10, 11, 2, 1, 9, 8, 7)   ;#for openGL
def permuteCase(case):
	return sum(
		((case >> oldBit) & 1) << newBit
		for newBit, oldBit in enumerate(permutation)
		)

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
		index = -1
		for i in range(9):
			if subExpr[i] != 0:
				index = i
		assert i != -1
		return 'c%s' % (index + 1)
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

def printSwitch(filename):
	file = open(filename, 'w')
	exprToCases = {}
	for case, expr in enumerate(pixelExpr):
		exprToCases.setdefault(
			tuple(tuple(subExpr) for subExpr in expr),
			[]
			).append(permuteCase(case))
	print >> file, 'switch (pattern) {'
	for cases, expr in sorted(
		( sorted(cases), expr )
		for expr, cases in exprToCases.iteritems()
		):
		for case in cases:
			print >> file, 'case %d:' % case
		for subPixel, subExpr in enumerate(expr):
			num = subPixel + 1
			if num > 4:
				num += 1
			print >> file, ('\tpixel%d = %s;' %
			       (num, printSubExpr(subExpr)))
		print >> file, '\tbreak;'
	print >> file, 'default:'
	print >> file, '\tUNREACHABLE;'
	print >> file, '\tpixel1 = pixel2 = pixel3 = pixel4 ='
	print >> file, '\tpixel6 = pixel7 = pixel8 = pixel9 = 0; // avoid warning'
	print >> file, '}'
	file.close()

def printHQLiteScalerTable():
	pixelExpr2 = [ [ None ] * 16 for _ in range(1 << 12) ]
	for case in range(1 << 12):
		pixelExpr2[permuteCase(case)] = pixelExpr[case]
	#
	for case in range(1 << 12):
		sys.stdout.write('// %d\n' % case)
		for subPixel in range(9):
			if subPixel == 4:
				sys.stdout.write('   0, 255,   0,')
			else:
				if subPixel > 4:
					subPixel -= 1
				factor = 256 / sum(pixelExpr2[case][subPixel])
				for c in (3, 4, 5):
					sys.stdout.write(' %3d,' % min(255, factor * pixelExpr2[case][subPixel][c]))
			sys.stdout.write('\n')

def printHQLiteScalerTable2Binary(filename):
	file = open(filename, 'wb')
	pixelExpr2 = [ [ None ] * 16 for _ in range(1 << 12) ]
	for case in range(1 << 12):
		#pixelExpr2[permuteCase(case)] = pixelExpr[case]
		pixelExpr2[case] = pixelExpr[case]
	#
	offset_x = ( 43,   0, -43,  43, -43,  43,   0, -43)
	offset_y = ( 43,  43,  43,   0,   0, -43, -43, -43)
	for case in range(1 << 12):
		#sys.stdout.write('// %d\n' % case)
		for subPixel in range(9):
			if subPixel == 4:
				x = 128
				y = 128
			else:
				if subPixel > 4:
					subPixel -= 1
				for c in (0, 1, 2, 6, 7, 8):
					assert pixelExpr2[case][subPixel][c] == 0
				assert (pixelExpr2[case][subPixel][3] == 0) or (pixelExpr2[case][subPixel][5] == 0)
				factor = sum(pixelExpr2[case][subPixel])
				x = offset_x[subPixel] + 128
				y = offset_y[subPixel] + 128
				if pixelExpr2[case][subPixel][5] == 0:
					x -= 128 * pixelExpr2[case][subPixel][3] / factor
				else:
					x += 128 * pixelExpr2[case][subPixel][5] / factor
			#print x, y
			assert 0 <= x
			assert x <= 255
			file.write(chr(x) + chr(y))
	file.close()

def printHQLiteScalerTableBinary(filename):
	file = open(filename, 'wb')
	pixelExpr2 = [ [ None ] * 16 for _ in range(1 << 12) ]
	for case in range(1 << 12):
		pixelExpr2[permuteCase(case)] = pixelExpr[case]
	#
	for case in range(1 << 12):
		for subPixel in range(9):
			if subPixel == 4:
				file.write(chr(0) + chr(255) + chr(0))
			else:
				if subPixel > 4:
					subPixel -= 1
				factor = 256 / sum(pixelExpr2[case][subPixel])
				for c in (3, 4, 5):
					file.write(chr(min(255, factor * pixelExpr2[case][subPixel][c])))
	file.close()

def printHQScalerTable():
	pixelExpr2 = [ [ None ] * 9 for _ in range(1 << 12) ]
	for case in range(1 << 12):
		pcase = permuteCase(case)
		for subPixel in range(9):
			if subPixel < 4:
				pixelExpr2[pcase][subPixel] = pixelExpr[case][subPixel]
			elif subPixel == 4:
				pixelExpr2[pcase][subPixel] = [0, 0, 0, 0, 1, 0, 0, 0, 0]
			else:
				pixelExpr2[pcase][subPixel] = pixelExpr[case][subPixel - 1]
	#
	xy = [[[-1] * 2 for _ in range(9)] for _ in range(1 << 12)]
	for case in range(1 << 12):
		for subPixel in range(9):
			j = 0
			for i in (0, 1, 2, 3, 5, 6, 7, 8):
				if pixelExpr2[case][subPixel][i] != 0:
					assert j < 2
					xy[case][subPixel][j] = i
					j = j + 1
	for case in range(1 << 12):
		sys.stdout.write('// %d\n' % case)
		for subPixel in range(9):
			for i in range(2):
				t = xy[case][subPixel][i]
				if t == -1:
					t = 4
				x = min(255, (t % 3) * 128)
				y = min(255, (t / 3) * 128)
				sys.stdout.write(' %3d, %3d,' % (x, y))
			sys.stdout.write('\n')
	sys.stdout.write('//-------------\n')
	for case in range(1 << 12):
		sys.stdout.write('// %d\n' % case)
		for subPixel in range(9):
			factor = 256 / sum(pixelExpr2[case][subPixel])
			for c in (xy[case][subPixel][0], xy[case][subPixel][1], 4):
				t = 0
				if c != -1:
					t = pixelExpr2[case][subPixel][c]
				sys.stdout.write(' %3d,' % min(255, factor * t))
			sys.stdout.write('\n')

def printHQScalerTableBinary(offsetsFilename, weightsFilename):
	pixelExpr2 = [ [ None ] * 9 for _ in range(1 << 12) ]
	for case in range(1 << 12):
		#pcase = permuteCase(case)
		pcase = case
		for subPixel in range(9):
			if subPixel < 4:
				pixelExpr2[pcase][subPixel] = pixelExpr[case][subPixel]
			elif subPixel == 4:
				pixelExpr2[pcase][subPixel] = [0, 0, 0, 0, 1, 0, 0, 0, 0]
			else:
				pixelExpr2[pcase][subPixel] = pixelExpr[case][subPixel - 1]

	offsetsFile = open(offsetsFilename, 'wb')
	xy = [[[-1] * 2 for _ in range(9)] for _ in range(1 << 12)]
	for case in range(1 << 12):
		for subPixel in range(9):
			j = 0
			for i in (0, 1, 2, 3, 5, 6, 7, 8):
				if pixelExpr2[case][subPixel][i] != 0:
					assert j < 2
					xy[case][subPixel][j] = i
					j = j + 1
	for case in range(1 << 12):
		for subPixel in range(9):
			for i in range(2):
				t = xy[case][subPixel][i]
				if t == -1:
					t = 4
				x = min(255, (t % 3) * 128)
				y = min(255, (t / 3) * 128)
				offsetsFile.write(chr(x) + chr(y))
	offsetsFile.close()
	weightsFile = open(weightsFilename, 'wb')
	for case in range(1 << 12):
		for subPixel in range(9):
			factor = 256 / sum(pixelExpr2[case][subPixel])
			for c in (xy[case][subPixel][0], xy[case][subPixel][1], 4):
				t = 0
				if c != -1:
					t = pixelExpr2[case][subPixel][c]
				weightsFile.write(chr(min(255, factor * t)))
	weightsFile.close()

def blendWeights2(w1, w2):
	w = [0] * 9
	sum1 = sum(w1)
	sum2 = sum(w2)
	for i in range(9):
		w[i] = 2 * sum2 * w1[i] + sum1 * w2[i]
	return simplifyWeights(w)

def makeNarrow():
	global pixelExpr
	narrowExpr = [ [ None ] * 6 for _ in range(1 << 12) ]
	for case in range(1 << 12):
		narrowExpr[case][0] = blendWeights2(
			pixelExpr[case][0], pixelExpr[case][1])
		narrowExpr[case][1] = blendWeights2(
			pixelExpr[case][2], pixelExpr[case][1])
		narrowExpr[case][2] = blendWeights2(
			pixelExpr[case][3], [0, 0, 0, 0, 1, 0, 0, 0, 0])
		narrowExpr[case][3] = blendWeights2(
			pixelExpr[case][4], [0, 0, 0, 0, 1, 0, 0, 0, 0])
		narrowExpr[case][4] = blendWeights2(
			pixelExpr[case][5], pixelExpr[case][6])
		narrowExpr[case][5] = blendWeights2(
			pixelExpr[case][7], pixelExpr[case][6])
	pixelExpr = narrowExpr

if __name__ == '__main__':
	#for case in range(1 << 12):
	#	for pixel in range(8):
	#		print case, pixel, pixelExpr[case][pixel]

	#makeNarrow()

	#printHQScalerTable()
	#printHQScalerTableBinary('HQ3xOffsets.dat', 'HQ3xWeights.dat')

	#makeLite(pixelExpr)
	#printHQLiteScalerTable()
	#printHQLiteScalerTableBinary('HQ3xLiteWeights.dat')

	#makeLite(pixelExpr, (2, 4, 7))
	#printHQLiteScalerTable2Binary('HQ3xLiteOffset.dat')

	printSwitch('HQ3xScaler-1x1to3x3.nn')

	makeLite(pixelExpr)
	printSwitch('HQ3xLiteScaler-1x1to3x3.nn')
