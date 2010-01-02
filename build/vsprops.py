# $Id$

from xml.etree.ElementTree import Element, ElementTree, SubElement

def createVSPropertyNode(properties):
	root = Element(
		'VisualStudioPropertySheet',
		ProjectType = 'Visual C++',
		Version = '8.00',
		Name = '3rdparty'
		)
	root.text = '\n'

	for name, value in sorted(properties.iteritems()):
		macro = SubElement(
			root, 'UserMacro',
			Name = name,
			Value = value,
			PerformEnvironmentSet = 'true'
			)
		macro.tail = '\n'

	root.tail = '\n'
	return root

def writeVSPropertyFile(fileName, properties):
	tree = ElementTree(createVSPropertyNode(properties))
	tree.write(fileName, encoding = 'UTF-8')
