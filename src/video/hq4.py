# $Id$

from hqcommon import makeLite, printSubExpr

import sys

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
	for case in cases:
		for subCase in subCases:
			weights = [ 0 ] * 16
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

pixelExpr = [ [ None ] * 16 for _ in range(1 << 12) ]

cases = []
subCases = range(1 << 4)
for line in filterSwitch(file('HQ4xScaler.in')):
	if line.startswith('case'):
		cases.append(int(line[5 : line.index(':', 5)]))
	elif line.startswith('pixel'):
		subPixel = int(line[5], 16)
		assert 0 <= subPixel < 16
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


#oldpermutation = (2, 9, 7, 4, 3, 10, 11, 1, 8, 0, 6, 5)
permutation = (5, 0, 4, 6, 3, 10, 11, 2, 1, 9, 8, 7)
def permuteCase(case):
	return sum(
		((case >> oldBit) & 1) << newBit
		for newBit, oldBit in enumerate(permutation)
		)

def printSwitch():
	exprToCases = {}
	for case, expr in enumerate(pixelExpr):
		exprToCases.setdefault(
			tuple(tuple(subExpr) for subExpr in expr),
			[]
			).append(permuteCase(case))
	#print exprToCases
	print 'switch (pattern) {'
	for cases, expr in sorted(
		( sorted(cases), expr )
		for expr, cases in exprToCases.iteritems()
		):
		for case in cases:
			print 'case %d:' % case
		for subPixel, subExpr in enumerate(expr):
			print ('\tpixel%s = %s;' %
			       (hex(subPixel)[2:], printSubExpr(subExpr)))
		print '\tbreak;'
	print 'default:'
	print '\tassert(false);'
	print '\tpixel0 = pixel1 = pixel2 = pixel3 ='
	print '\tpixel4 = pixel5 = pixel6 = pixel7 ='
	print '\tpixel8 = pixel9 = pixela = pixelb ='
	print '\tpixelc = pixeld = pixele = pixelf = 0; // avoid warning'
	print '}'

def printHQLiteScalerTable():
	pixelExpr2 = [ [ None ] * 16 for _ in range(1 << 12) ]
	for case in range(1 << 12):
		pixelExpr2[permuteCase(case)] = pixelExpr[case]
	#
	for case in range(1 << 12):
		sys.stdout.write('// %d\n' % case)
		for subPixel in range(16):
			factor = 256 / sum(pixelExpr2[case][subPixel])
			for c in (3, 4, 5):
				sys.stdout.write(' %3d,' % min(255, factor * pixelExpr2[case][subPixel][c]))
			sys.stdout.write('\n')

def printHQLiteScalerTableBinary(filename):
	file = open(filename, 'wb')
	pixelExpr2 = [ [ None ] * 16 for _ in range(1 << 12) ]
	for case in range(1 << 12):
		pixelExpr2[permuteCase(case)] = pixelExpr[case]
	#
	for case in range(1 << 12):
		for subPixel in range(16):
			factor = 256 / sum(pixelExpr2[case][subPixel])
			for c in (3, 4, 5):
				file.write(chr(min(255, factor * pixelExpr2[case][subPixel][c])))
	file.close()

def printHQScalerTable():
	pixelExpr2 = [ [ None ] * 16 for _ in range(1 << 12) ]
	for case in range(1 << 12):
		pixelExpr2[permuteCase(case)] = pixelExpr[case]
	#
	xy = [[[-1] * 2 for _ in range(16)] for _ in range(1 << 12)]
	for case in range(1 << 12):
		for subPixel in range(16):
			j = 0
			for i in (0, 1, 2, 3, 5, 6, 7, 8):
				if pixelExpr2[case][subPixel][i] != 0:
					assert j < 2
					xy[case][subPixel][j] = i
					j = j + 1
	for case in range(1 << 12):
		sys.stdout.write('// %d\n' % case)
		for subPixel in range(16):
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
		for subPixel in range(16):
			factor = 256 / sum(pixelExpr2[case][subPixel])
			for c in (xy[case][subPixel][0], xy[case][subPixel][1], 4):
				t = 0
				if c != -1:
					t = pixelExpr2[case][subPixel][c]
				sys.stdout.write(' %3d,' % min(255, factor * t))
			sys.stdout.write('\n')

def printHQScalerTableBinary(offsetsFilename, weightsFilename):
	pixelExpr2 = [ [ None ] * 16 for _ in range(1 << 12) ]
	for case in range(1 << 12):
		pixelExpr2[permuteCase(case)] = pixelExpr[case]
		#pixelExpr2[case] = pixelExpr[case]
	#
	offsetsFile = open(offsetsFilename, 'wb')
	xy = [[[-1] * 2 for _ in range(16)] for _ in range(1 << 12)]
	for case in range(1 << 12):
		for subPixel in range(16):
			j = 0
			for i in (0, 1, 2, 3, 5, 6, 7, 8):
				if pixelExpr2[case][subPixel][i] != 0:
					assert j < 2
					xy[case][subPixel][j] = i
					j = j + 1
	for case in range(1 << 12):
		for subPixel in range(16):
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
		for subPixel in range(16):
			factor = 256 / sum(pixelExpr2[case][subPixel])
			for c in (xy[case][subPixel][0], xy[case][subPixel][1], 4):
				t = 0
				if c != -1:
					t = pixelExpr2[case][subPixel][c]
				weightsFile.write(chr(min(255, factor * t)))
	weightsFile.close()

def printHQLiteScalerTable2Binary(filename):
	file = open(filename, 'wb')
	pixelExpr2 = [ [ None ] * 16 for _ in range(1 << 12) ]
	for case in range(1 << 12):
		pixelExpr2[permuteCase(case)] = pixelExpr[case]
		#pixelExpr2[case] = pixelExpr[case]
	#
	offset_x = ( 48,  16, -16, -48,  48,  16, -16, -48,  48,  16, -16, -48,  48,  16, -16, -48)
	offset_y = ( 48,  48,  48,  48,  16,  16,  16,  16, -16, -16, -16, -16, -48, -48, -48, -48)
	for case in range(1 << 12):
		#sys.stdout.write('// %d\n' % case)
		for subPixel in range(16):
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


#printHQScalerTable()
printHQScalerTableBinary('HQ4xOffsets.dat', 'HQ4xWeights.dat')

#makeLite(pixelExpr)
#printHQLiteScalerTable()
#printHQLiteScalerTableBinary('HQ4xLiteWeights.dat')

makeLite(pixelExpr, (2, 3, 6, 7, 10, 11, 14, 15))
printHQLiteScalerTable2Binary('HQ4xLiteOffsets.dat')

#printSwitch()
