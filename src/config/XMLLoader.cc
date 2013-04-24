#include "XMLLoader.hh"
#include "XMLElement.hh"
#include "XMLException.hh"
#include "File.hh"
#include "FileException.hh"
#include "memory.hh"
#include <cassert>
#include <limits>
#include <cstring>
#include <libxml/parser.h>
#include <libxml/xmlversion.h>

using std::unique_ptr;
using std::vector;
using std::string;

namespace openmsx {
namespace XMLLoader {

struct XMLLoaderHelper
{
	XMLLoaderHelper()
	{
	}

	XMLElement root;
	vector<XMLElement*> current;
	string data;
	string systemID;
};

static void cbStartElement(
	void* helper_,
	const xmlChar* localname, const xmlChar* /*prefix*/, const xmlChar* /*uri*/,
	int /*nb_namespaces*/, const xmlChar** /*namespaces*/,
	int nb_attributes, int /*nb_defaulted*/, const xmlChar** attrs)
{
	auto helper = static_cast<XMLLoaderHelper*>(helper_);
	XMLElement newElem(reinterpret_cast<const char*>(localname));

	for (int i = 0; i < nb_attributes; i++) {
		auto valueStart =
			reinterpret_cast<const char*>(attrs[i * 5 + 3]);
		auto valueEnd =
			reinterpret_cast<const char*>(attrs[i * 5 + 4]);
		newElem.addAttribute(
			reinterpret_cast<const char*>(attrs[i * 5 + 0]),
			std::string(valueStart, valueEnd - valueStart));
	}

	XMLElement* newElem2;
	if (!helper->current.empty()) {
		newElem2 = &helper->current.back()->addChild(std::move(newElem));
	} else {
		helper->root = std::move(newElem);
		newElem2 = &helper->root;
	}
	helper->current.push_back(newElem2);

	helper->data.clear();
}

static void cbEndElement(
	void* helper_,
	const xmlChar* localname, const xmlChar* /*prefix*/, const xmlChar* /*uri*/)
{
	auto helper = static_cast<XMLLoaderHelper*>(helper_);
	assert(!helper->current.empty());
	XMLElement& current = *helper->current.back();
	assert(reinterpret_cast<const char*>(localname) == current.getName());
	(void)localname;

	if (!current.hasChildren()) {
		current.setData(helper->data);
	}
	helper->current.pop_back();
}

static void cbCharacters(void* helper_, const xmlChar* chars, int len)
{
	auto helper = static_cast<XMLLoaderHelper*>(helper_);
	assert(!helper->current.empty());
	if (!helper->current.back()->hasChildren()) {
		helper->data.append(reinterpret_cast<const char*>(chars), len);
	}
}

static void cbInternalSubset(void* helper_, const xmlChar* /*name*/,
                             const xmlChar* /*extID*/, const xmlChar* systemID)
{
	auto helper = static_cast<XMLLoaderHelper*>(helper_);
	helper->systemID = reinterpret_cast<const char*>(systemID);
}

XMLElement load(const string& filename, const string& systemID)
{
	File file(filename);
	// TODO: Reading blocks to a fixed-size buffer would require less memory
	//       when reading (g)zipped XML.
	// Note: On destruction of "file", munmap() is called automatically.
	const byte* fileContent;
	size_t size_;
	try {
		fileContent = file.mmap(size_);
	} catch (FileException& e) {
		throw XMLException(filename + ": failed to mmap: " + e.getMessage());
	}
	if (size_ > size_t(std::numeric_limits<int>::max())) {
		throw XMLException(filename + ": file too big");
	}
	auto size = int(size_);

	xmlSAXHandler handler;
	memset(&handler, 0, sizeof(handler));
	handler.startElementNs = cbStartElement;
	handler.endElementNs   = cbEndElement;
	handler.characters     = cbCharacters;
	handler.internalSubset = cbInternalSubset;
	handler.initialized = XML_SAX2_MAGIC;

	XMLLoaderHelper helper;

	xmlParserCtxtPtr ctxt = xmlCreatePushParserCtxt(
		&handler, &helper, nullptr, 0, filename.c_str()
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

	if (helper.root.getName().empty()) {
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

	return std::move(helper.root);
}

} // namespace XMLLoader
} // namespace openmsx
