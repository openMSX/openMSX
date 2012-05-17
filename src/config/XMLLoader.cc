// $Id$

#include "XMLLoader.hh"
#include "XMLElement.hh"
#include "XMLException.hh"
#include "File.hh"
#include "FileException.hh"
#include <cassert>
#include <cstring>
#include <libxml/parser.h>
#include <libxml/xmlversion.h>

using std::auto_ptr;
using std::string;

namespace openmsx {
namespace XMLLoader {

struct XMLLoaderHelper
{
	XMLLoaderHelper()
	{
	}

	std::auto_ptr<XMLElement> root;
	std::vector<XMLElement*> current;
	std::string data;
	std::string systemID;
};

static void cbStartElement(
	XMLLoaderHelper* helper,
	const xmlChar* localname, const xmlChar* /*prefix*/, const xmlChar* /*uri*/,
	int /*nb_namespaces*/, const xmlChar** /*namespaces*/,
	int nb_attributes, int /*nb_defaulted*/, const xmlChar** attrs
	)
{
	std::auto_ptr<XMLElement> newElem(
		new XMLElement(reinterpret_cast<const char*>(localname)));

	for (int i = 0; i < nb_attributes; i++) {
		const char* valueStart =
			reinterpret_cast<const char*>(attrs[i * 5 + 3]);
		const char* valueEnd =
			reinterpret_cast<const char*>(attrs[i * 5 + 4]);
		newElem->addAttribute(
			reinterpret_cast<const char*>(attrs[i * 5 + 0]),
			std::string(valueStart, valueEnd - valueStart)
			);
	}

	XMLElement* newElem2 = newElem.get();
	if (!helper->current.empty()) {
		helper->current.back()->addChild(newElem);
	} else {
		helper->root = newElem;
	}
	helper->current.push_back(newElem2);

	helper->data.clear();
}

static void cbEndElement(
	XMLLoaderHelper* helper,
	const xmlChar* localname, const xmlChar* /*prefix*/, const xmlChar* /*uri*/)
{
	assert(!helper->current.empty());
	XMLElement& current = *helper->current.back();
	assert(reinterpret_cast<const char*>(localname) == current.getName());
	(void)localname;

	if (!current.hasChildren()) {
		current.setData(helper->data);
	}
	helper->current.pop_back();
}

static void cbCharacters(XMLLoaderHelper* helper, const xmlChar* chars, int len)
{
	assert(!helper->current.empty());
	if (!helper->current.back()->hasChildren()) {
		helper->data.append(reinterpret_cast<const char*>(chars), len);
	}
}

static void cbInternalSubset(XMLLoaderHelper* helper, const xmlChar* /*name*/,
                             const xmlChar* /*extID*/, const xmlChar* systemID)
{
	helper->systemID = reinterpret_cast<const char*>(systemID);
}

auto_ptr<XMLElement> load(const string& filename, const string& systemID)
{
	File file(filename);
	// TODO: Reading blocks to a fixed-size buffer would require less memory
	//       when reading (g)zipped XML.
	// Note: On destruction of "file", munmap() is called automatically.
	const byte* fileContent;
	unsigned size;
	try {
		fileContent = file.mmap(size);
	} catch (FileException& e) {
		throw XMLException(filename + ": failed to mmap: " + e.getMessage());
	}

	xmlSAXHandler handler;
	memset(&handler, 0, sizeof(handler));
	handler.startElementNs = (startElementNsSAX2Func)cbStartElement;
	handler.endElementNs   = (endElementNsSAX2Func)  cbEndElement;
	handler.characters     = (charactersSAXFunc)     cbCharacters;
	handler.internalSubset = (internalSubsetSAXFunc) cbInternalSubset;
	handler.initialized = XML_SAX2_MAGIC;

	XMLLoaderHelper helper;

	xmlParserCtxtPtr ctxt = xmlCreatePushParserCtxt(
		&handler, &helper, NULL, 0, filename.c_str()
		);
	if (!ctxt) {
		throw XMLException(filename + ": Could not create XML parser context");
	}
	const int parseError = xmlParseChunk(
		ctxt, reinterpret_cast<const char *>(fileContent), size, true);
	xmlFreeParserCtxt(ctxt);
	if (parseError) {
		throw XMLException(filename + ": Document parsing failed");
	}

	if (!helper.root.get()) {
		throw XMLException(filename +
			": Document doesn't contain mandatory root Element");
	}
	if (helper.systemID.empty()) {
		throw XMLException(filename + ": Missing systemID.\n"
			"You're probably using an old incompatible file format.");
	}
	if (helper.systemID != systemID) {
		throw XMLException(filename + ": systemID doesn't match "
			"(expected " + systemID + ", got " + helper.systemID + ")\n"
			"You're probably using an old incompatible file format.");
	}

	return helper.root;
}

} // namespace XMLLoader
} // namespace openmsx
