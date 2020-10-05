#!/usr/bin/env python3

from executils import captureStdout

from collections import defaultdict, namedtuple
from os.path import normpath
import re, sys


Symbol = namedtuple('Symbol', ('name', 'typ', 'size', 'source', 'lineNo'))

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
				if sizeStr is None:
					continue
				if typ in 'Bb':
					# Symbols in BSS (uninitialized data section) do not
					# contribute to executable size, so ignore them.
					continue
				if originStr is None:
					source = lineNo = None
				else:
					source, lineNo = originStr.lstrip().rsplit(':', 1)
					source = normpath(source)
					lineNo = int(lineNo)
				yield Symbol(name, typ, int(sizeStr, 16), source, lineNo)

if __name__ == '__main__':
	if len(sys.argv) == 2:
		executable = sys.argv[1]

		# Get symbol information.
		symbolsBySource = defaultdict(list)
		for symbol in parseSymbolSize(executable):
			symbolsBySource[symbol.source].append(symbol)

		# Build directory tree.
		def newDict():
			return defaultdict(newDict)
		dirTree = newDict()
		for source, symbols in symbolsBySource.items():
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
			names = set(node.keys())
			while names:
				name = names.pop()
				content = node[name]
				if isinstance(content, dict) and len(content) == 1:
					subName, subContent = next(iter(content.items()))
					if isinstance(subContent, dict):
						# A directory containing a single directory.
						del node[name]
						node[name + subName] = subContent
						names.add(name + subName)
			for content in node.values():
				if isinstance(content, dict):
					compactTree(content)
		compactTree(dirTree)

		# Compute size of all nodes in the tree.
		def buildSizeTree(node):
			if isinstance(node, dict):
				newNode = {}
				for name, content in node.items():
					newNode[name] = buildSizeTree(content)
				nodeSize = sum(size for size, subNode in newNode.values())
				return nodeSize, newNode
			else:
				nodeSize = sum(symbol.size for symbol in node)
				return nodeSize, node
		totalSize, sizeTree = buildSizeTree(dirTree)

		# Output.
		def printTree(size, node, indent):
			if isinstance(node, dict):
				for name, (contentSize, content) in sorted(
						node.items(),
						key=lambda item: (-item[1][0], item[0])
						):
					print('%s%8d %s' % (indent, contentSize, name))
					printTree(contentSize, content, indent + '  ')
			else:
				for symbol in sorted(
						node,
						key=lambda symbol: (-symbol.size, symbol.name)
						):
					lineNo = symbol.lineNo
					print('%s%8d %s %s %s' % (
						indent, symbol.size, symbol.typ, symbol.name,
						'' if lineNo is None else '(line %d)' % lineNo
						))
		printTree(totalSize, sizeTree, '')
	else:
		print('Usage: python3 sizestats.py executable', file=sys.stderr)
		sys.exit(2)
