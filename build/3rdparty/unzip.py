import sys
import zipfile
import os
import os.path

def UnzipFile(file, outputdir):
	if not os.path.exists(outputdir):
		os.mkdir(outputdir, 755)
	zip = zipfile.ZipFile(open(file, 'rb'))
	for name in zip.namelist():
		output = os.path.join(outputdir, name)
		if name.endswith('/'):
			if not os.path.exists(output):
				os.mkdir(output)
		else:
			output = open(output, 'wb')
			output.write(zip.read(name))
			output.close()
		
if __name__ == '__main__':
	if len(sys.argv) == 3:
		UnzipFile(sys.argv[1], sys.argv[2])
	else:
		print >> sys.stderr, \
			'Usage: python unzip.py zipfile outputdir'
		sys.exit(2)
