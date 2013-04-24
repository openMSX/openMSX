from executils import captureStdout

from collections import defaultdict
from os.path import normpath
import re, sys

_reSymbolInfo = re.compile(
	r'^([0-9a-f]+\s)?([0-9a-f]+\s)?\s*([A-Za-z])\s([^\t]+)(\t[^\t]+)?$'
	)
def parseSymbolSize(objectFile):
	text = captureStdout(sys.stderr, 'nm -CSl "%s"' % objectFile)
	if text is not None:
		for line in text.split('\n'):
			if line:
				match = _reSymbolInfo.match(line)
				assert match is not None, line
				addr_, sizeStr, typ, name, originStr = match.groups()
				if originStr is None:
					origin = (None, None)
				else:
					source, lineNo = originStr.lstrip().rsplit(':', 1)
					origin = (normpath(source), int(lineNo))
				if sizeStr is not None:
					if typ not in 'Bb':
						# Symbols in BSS (uninitialized data section) do not
						# contribute to executable size, so ignore them.
						yield name, typ, int(sizeStr, 16), origin

if __name__ == '__main__':
	if len(sys.argv) == 2:
		executable = sys.argv[1]

		# Get symbol information.
		symbolsBySource = defaultdict(list)
		for name, typ, size, (source, lineNo) in parseSymbolSize(executable):
			symbolsBySource[source].append((name, typ, size, lineNo))

		# Build directory tree.
		def newDict():
			return defaultdict(newDict)
		dirTree = newDict()
		for source, symbols in symbolsBySource.iteritems():
			if source is None:
				parts = [ '(no source)' ]
			else:
				assert source[0] == '/'
				parts = source[1 : ].split('/')
				parts[0] = '/' + parts[0]
			node = dirTree
			for part in parts[ : -1]:
				node = node[part + '/']
			node[parts[-1]] = symbols

		# Combine branches without forks.
		def compactTree(node):
			names = set(node.iterkeys())
			while names:
				name = names.pop()
				content = node[name]
				if isinstance(content, dict) and len(content) == 1:
					subName, subContent = content.iteritems().next()
					if isinstance(subContent, dict):
						# A directory containing a single directory.
						del node[name]
						node[name + subName] = subContent
						names.add(name + subName)
			for content in node.itervalues():
				if isinstance(content, dict):
					compactTree(content)
		compactTree(dirTree)

		# Compute size of all nodes in the tree.
		def buildSizeTree(node):
			if isinstance(node, dict):
				newNode = {}
				for name, content in node.iteritems():
					newNode[name] = buildSizeTree(content)
				nodeSize = sum(size for size, subNode in newNode.itervalues())
				return nodeSize, newNode
			else:
				nodeSize = sum(size for name, typ, size, lineNo in node)
				return nodeSize, node
		totalSize, sizeTree = buildSizeTree(dirTree)

		# Output.
		def printTree(size, node, indent):
			if isinstance(node, dict):
				for name, (contentSize, content) in sorted(
					node.iteritems(),
					key = lambda (name, (contentSize, content)):
						( -contentSize, name )
					):
					print '%s%8d %s' % (indent, contentSize, name)
					printTree(contentSize, content, indent + '  ')
			else:
				for name, typ, size, lineNo in sorted(
					node,
					key = lambda (name, typ, size, lineNo): ( -size, name )
					):
					print '%s%8d %s %s %s' % (
						indent, size, typ, name,
						'' if lineNo is None else '(line %d)' % lineNo
						)
		printTree(totalSize, sizeTree, '')
	else:
		print >> sys.stderr, 'Usage: python sizestats.py executable'
		sys.exit(2)
