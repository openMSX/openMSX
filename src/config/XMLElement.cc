#include "XMLElement.hh"

#include "ConfigException.hh"
#include "File.hh"
#include "FileContext.hh" // for bw compat
#include "FileException.hh"
#include "StringOp.hh"
#include "XMLException.hh"
#include "XMLOutputStream.hh"
#include "rapidsax.hh"
#include "serialize.hh"
#include "serialize_meta.hh"
#include "serialize_stl.hh"

#include <cassert>
#include <vector>

namespace openmsx {

// Returns nullptr when not found.
const XMLElement* XMLElement::findChild(std::string_view childName) const
{
	for (const auto* child = firstChild; child; child = child->nextSibling) {
		if (child->name == childName) {
			return child;
		}
	}
	return nullptr;
}

// Similar to above, but instead of starting the search from the start of the
// list, start searching at 'hint'. This 'hint' parameter must be initialized
// with 'firstChild', on each successful search this parameter is updated to
// point to the next child (so that the next search starts from there). If the
// end of the list is reached we restart the search from the start (so that the
// full list is searched before giving up).
const XMLElement* XMLElement::findChild(std::string_view childName, const XMLElement*& hint) const
{
	for (const auto* current = hint; current; current = current->nextSibling) {
		if (current->name == childName) {
			hint = current->nextSibling;
			return current;
		}
	}
	for (const auto* current = firstChild; current != hint; current = current->nextSibling) {
		if (current->name == childName) {
			hint = current->nextSibling;
			return current;
		}
	}
	return nullptr;
}

// Like findChild(), but throws when not found.
const XMLElement& XMLElement::getChild(std::string_view childName) const
{
	if (const auto* elem = findChild(childName)) {
		return *elem;
	}
	throw ConfigException("Missing tag \"", childName, "\".");
}

// Throws when not found.
std::string_view XMLElement::getChildData(std::string_view childName) const
{
	return getChild(childName).getData();
}

// Returns default value when not found.
std::string_view XMLElement::getChildData(
	std::string_view childName, std::string_view defaultValue) const
{
	const auto* child = findChild(childName);
	return child ? child->getData() : defaultValue;
}

bool XMLElement::getChildDataAsBool(std::string_view childName, bool defaultValue) const
{
	const auto* child = findChild(childName);
	return child ? StringOp::stringToBool(child->getData()) : defaultValue;
}

int XMLElement::getChildDataAsInt(std::string_view childName, int defaultValue) const
{
	const auto* child = findChild(childName);
	if (!child) return defaultValue;
	return StringOp::stringTo<int>(child->getData()).value_or(defaultValue);
}

size_t XMLElement::numChildren() const
{
	return std::distance(getChildren().begin(), getChildren().end());
}

// Return nullptr when not found.
const XMLAttribute* XMLElement::findAttribute(std::string_view attrName) const
{
	for (const auto* attr = firstAttribute; attr; attr = attr->nextAttribute) {
		if (attr->getName() == attrName) {
			return attr;
		}
	}
	return nullptr;
}

// Throws when not found.
const XMLAttribute& XMLElement::getAttribute(std::string_view attrName) const
{
	if (const auto* result = findAttribute(attrName)) {
		return *result;
	}
	throw ConfigException("Missing attribute \"", attrName, "\".");
}

// Throws when not found.
std::string_view XMLElement::getAttributeValue(std::string_view attrName) const
{
	return getAttribute(attrName).getValue();
}

std::string_view XMLElement::getAttributeValue(std::string_view attrName,
                                               std::string_view defaultValue) const
{
	const auto* attr = findAttribute(attrName);
	return attr ? attr->getValue() : defaultValue;
}

// Returns default value when not found.
bool XMLElement::getAttributeValueAsBool(std::string_view attrName,
                                         bool defaultValue) const
{
	const auto* attr = findAttribute(attrName);
	return attr ? StringOp::stringToBool(attr->getValue()) : defaultValue;
}

int XMLElement::getAttributeValueAsInt(std::string_view attrName,
                                       int defaultValue) const
{
	const auto* attr = findAttribute(attrName);
	if (!attr) return defaultValue;
	return StringOp::stringTo<int>(attr->getValue()).value_or(defaultValue);
}

// Like findAttribute(), but returns a pointer-to-the-XMLAttribute-pointer.
// This is mainly useful in combination with removeAttribute().
XMLAttribute** XMLElement::findAttributePointer(std::string_view attrName)
{
	for (auto** attr = &firstAttribute; *attr; attr = &(*attr)->nextAttribute) {
		if ((*attr)->getName() == attrName) {
			return attr;
		}
	}
	return nullptr;
}

// Remove a specific attribute from the (single-linked) list.
void XMLElement::removeAttribute(XMLAttribute** attrPtr)
{
	auto* attr = *attrPtr;
	*attrPtr = attr->nextAttribute;
}

size_t XMLElement::numAttributes() const
{
	return std::distance(getAttributes().begin(), getAttributes().end());
}


// Search for child with given name, if it doesn't exist yet, then create it now
// with given data. Don't change the data of an already existing child.
XMLElement* XMLDocument::getOrCreateChild(XMLElement& parent, const char* childName, const char* childData)
{
	auto** elem = &parent.firstChild;
	while (true) {
		if (!*elem) {
			auto* n = allocateElement(childName);
			n->setData(childData);
			*elem = n;
			return n;
		}
		if ((*elem)->getName() == childName) {
			return *elem;
		}
		elem = &(*elem)->nextSibling;
	}
}

// Search for child with given name, and change the data of that child. If the
// child didn't exist yet, then create it now (also with given data).
XMLElement* XMLDocument::setChildData(XMLElement& parent, const char* childName, const char* childData)
{
	auto** elem = &parent.firstChild;
	while (true) {
		if (!*elem) {
			auto* n = allocateElement(childName);
			*elem = n;
			(*elem)->setData(childData);
			return *elem;
		}
		if ((*elem)->getName() == childName) {
			(*elem)->setData(childData);
			return *elem;
		}
		elem = &(*elem)->nextSibling;
	}
}

// Set attribute to new value, if that attribute didn't exist yet, then also
// create it.
void XMLDocument::setAttribute(XMLElement& elem, const char* attrName, const char* attrValue)
{
	auto** attr = &elem.firstAttribute;
	while (true) {
		if (!*attr) {
			auto* a = allocateAttribute(attrName, attrValue);
			*attr = a;
			return;
		}
		if ((*attr)->getName() == attrName) {
			(*attr)->setValue(attrValue);
			return;
		}
		attr = &(*attr)->nextAttribute;
	}
}


// Helper to parse a XML file into a XMLDocument.
class XMLDocumentHandler : public rapidsax::NullHandler
{
public:
	explicit XMLDocumentHandler(XMLDocument& doc_)
		: doc(doc_)
		, nextElement(&doc.root) {}

	[[nodiscard]] std::string_view getSystemID() const { return systemID; }

	void start(zstring_view name) {
		stack.push_back(currentElement);

		auto* n = doc.allocateElement(name.c_str());
		currentElement = n;

		assert(*nextElement == nullptr);
		*nextElement = n;
		nextElement = &n->firstChild;

		nextAttribute = &n->firstAttribute;
	}

	void stop() {
		nextElement = &currentElement->nextSibling;
		nextAttribute = nullptr;
		currentElement = stack.back();
		stack.pop_back();
	}

	void text(zstring_view text) {
		currentElement->data = text.c_str();
	}

	void attribute(zstring_view name, zstring_view value) {
		auto* a = doc.allocateAttribute(name.c_str(), value.c_str());

		assert(nextAttribute);
		assert(*nextAttribute == nullptr);
		*nextAttribute = a;
		nextAttribute = &a->nextAttribute;
	}

	void doctype(zstring_view txt) {
		auto pos1 = txt.find(" SYSTEM ");
		if (pos1 == std::string_view::npos) return;
		if ((pos1 + 8) >= txt.size()) return;
		char q = txt[pos1 + 8];
		if (q != one_of('"', '\'')) return;
		auto t = txt.substr(pos1 + 9);
		auto pos2 = t.find(q);
		if (pos2 == std::string_view::npos) return;

		systemID = t.substr(0, pos2);
	}

private:
	XMLDocument& doc;

	std::string_view systemID;
	std::vector<XMLElement*> stack;
	XMLElement* currentElement = nullptr;
	XMLElement** nextElement = nullptr;
	XMLAttribute** nextAttribute = nullptr;
};

XMLElement* XMLDocument::allocateElement(const char* name)
{
	void* p = allocator.allocate(sizeof(XMLElement), alignof(XMLElement));
	return new (p) XMLElement(name);
}

XMLElement* XMLDocument::allocateElement(const char* name, const char* data)
{
	void* p = allocator.allocate(sizeof(XMLElement), alignof(XMLElement));
	return new (p) XMLElement(name, data);
}

XMLAttribute* XMLDocument::allocateAttribute(const char* name, const char* value)
{
	void* p = allocator.allocate(sizeof(XMLAttribute), alignof(XMLAttribute));
	return new (p) XMLAttribute(name, value);
}

const char* XMLDocument::allocateString(std::string_view str)
{
	auto* p = static_cast<char*>(allocator.allocate(str.size() + 1, alignof(char)));
	auto e = ranges::copy(str, p);
	*e = '\0';
	return p;
}

void XMLDocument::load(const std::string& filename, std::string_view systemID)
{
	assert(!root);

	try {
		File file(filename);
		auto size = file.getSize();
		buf.resize(size + rapidsax::EXTRA_BUFFER_SPACE);
		file.read(buf.first(size));
		buf[size] = 0;
	} catch (FileException& e) {
		throw XMLException(filename, ": failed to read: ", e.getMessage());
	}

	XMLDocumentHandler handler(*this);
	try {
		rapidsax::parse<rapidsax::zeroTerminateStrings>(handler, buf.data());
	} catch (rapidsax::ParseError& e) {
		throw XMLException(filename, ": Document parsing failed: ", e.what());
	}
	if (!root) {
		throw XMLException(filename,
			": Document doesn't contain mandatory root Element");
	}
	if (handler.getSystemID().empty()) {
		throw XMLException(filename, ": Missing systemID.\n"
			"You're probably using an old incompatible file format.");
	}
	if (handler.getSystemID() != systemID) {
		throw XMLException(filename, ": systemID doesn't match "
			"(expected ", systemID, ", got ", handler.getSystemID(), ")\n"
			"You're probably using an old incompatible file format.");
	}
}

XMLElement* XMLDocument::loadElement(MemInputArchive& ar)
{
	auto name = ar.loadStr();
	if (name.empty()) return nullptr; // should only happen for empty document
	auto* elem = allocateElement(allocateString(name));

	unsigned numAttrs; ar.load(numAttrs);
	auto** attrPtr = &elem->firstAttribute;
	repeat(numAttrs, [&] {
		const char* n = allocateString(ar.loadStr());
		const char* v = allocateString(ar.loadStr());
		auto* attr = allocateAttribute(n, v);
		*attrPtr = attr;
		attrPtr = &attr->nextAttribute;
	});

	unsigned numElems; ar.load(numElems);
	if (numElems) {
		auto** elemPtr = &elem->firstChild;
		repeat(numElems, [&] {
			auto* n = loadElement(ar);
			*elemPtr = n;
			elemPtr = &n->nextSibling;
		});
	} else {
		auto data = ar.loadStr();
		if (!data.empty()) {
			elem->setData(allocateString(data));
		}
	}

	return elem;
}

void XMLDocument::serialize(MemInputArchive& ar, unsigned /*version*/)
{
	root = loadElement(ar);
}

static void saveElement(MemOutputArchive& ar, const XMLElement& elem)
{
	ar.save(elem.getName());

	ar.save(unsigned(elem.numAttributes()));
	for (const auto& attr : elem.getAttributes()) {
		ar.save(attr.getName());
		ar.save(attr.getValue());
	}

	auto numElems = unsigned(elem.numChildren());
	ar.save(numElems);
	if (numElems) {
		for (const auto& child : elem.getChildren()) {
			saveElement(ar, child);
		}
	} else {
		ar.save(elem.getData());
	}
}

void XMLDocument::serialize(MemOutputArchive& ar, unsigned /*version*/) const
{
	if (root) {
		saveElement(ar, *root);
	} else {
		std::string_view empty;
		ar.save(empty);
	}
}

XMLElement* XMLDocument::clone(const XMLElement& inElem)
{
	auto* outElem = allocateElement(allocateString(inElem.getName()));

	auto** attrPtr = &outElem->firstAttribute;
	for (const auto& inAttr : inElem.getAttributes()) {
		const char* n = allocateString(inAttr.getName());
		const char* v = allocateString(inAttr.getValue());
		auto* outAttr = allocateAttribute(n, v);
		*attrPtr = outAttr;
		attrPtr = &outAttr->nextAttribute;
	}

	if (auto data = inElem.getData() ; !data.empty()) {
		outElem->setData(allocateString(data));
	}

	auto** childPtr = &outElem->firstChild;
	for (const auto& inChild : inElem.getChildren()) {
		auto* outChild = clone(inChild);
		*childPtr = outChild;
		childPtr = &outChild->nextSibling;
	}

	return outElem;
}

void XMLDocument::serialize(XmlInputArchive& ar, unsigned /*version*/)
{
	const auto* current = ar.currentElement();
	if (const auto* elem = current->getFirstChild()) {
		assert(elem->nextSibling == nullptr); // at most 1 child
		root = clone(*elem);
	}
}

static void saveElement(XMLOutputStream<XmlOutputArchive>& stream, const XMLElement& elem)
{
	stream.with_tag(elem.getName(), [&]{
		for (const auto& attr : elem.getAttributes()) {
			stream.attribute(attr.getName(), attr.getValue());
		}
		if (elem.hasChildren()) {
			for (const auto& child : elem.getChildren()) {
				saveElement(stream, child);
			}
		} else {
			stream.data(elem.getData());
		}
	});
}

void XMLDocument::serialize(XmlOutputArchive& ar, unsigned /*version*/) const
{
	auto& stream = ar.getXMLOutputStream();
	if (root) {
		saveElement(stream, *root);
	}
}

XMLElement* XMLDocument::clone(const OldXMLElement& inElem)
{
	auto* outElem = allocateElement(allocateString(inElem.name));

	auto** attrPtr = &outElem->firstAttribute;
	for (const auto& [inName, inValue] : inElem.attributes) {
		const char* n = allocateString(inName);
		const char* v = allocateString(inValue);
		auto* outAttr = allocateAttribute(n, v);
		*attrPtr = outAttr;
		attrPtr = &outAttr->nextAttribute;
	}

	if (!inElem.data.empty()) {
		outElem->setData(allocateString(inElem.data));
	}

	auto** childPtr = &outElem->firstChild;
	for (const auto& inChild : inElem.children) {
		auto* outChild = clone(inChild);
		*childPtr = outChild;
		childPtr = &outChild->nextSibling;
	}

	return outElem;
}

void XMLDocument::load(const OldXMLElement& elem)
{
	root = clone(elem);
}


static std::unique_ptr<FileContext> lastSerializedFileContext;
std::unique_ptr<FileContext> OldXMLElement::getLastSerializedFileContext()
{
	return std::move(lastSerializedFileContext);
}
// version 1: initial version
// version 2: removed 'context' tag
//            also removed 'parent', but that was never serialized
//        2b: (no need to increase version) name and data members are
//            serialized as normal members instead of constructor parameters
//        2c: (no need to increase version) attributes were initially stored as
//            map<string, string>, later this was changed to
//            vector<pair<string, string>>. To keep bw-compat the serialize()
//            method converted between these two formats. Though (by luck) in
//            the XML output both data structures are serialized to the same
//            format, so we can drop this conversion step without breaking
//            bw-compat.
template<typename Archive>
void OldXMLElement::serialize(Archive& ar, unsigned version)
{
	assert(Archive::IS_LOADER);
	ar.serialize("name",       name,
	             "data",       data,
	             "attributes", attributes,
	             "children",   children);

	if (ar.versionBelow(version, 2)) {
		std::unique_ptr<FileContext> context;
		ar.serialize("context", context);
		if (context) {
			lastSerializedFileContext = std::move(context);
		}
	}
}
INSTANTIATE_SERIALIZE_METHODS(OldXMLElement);

} // namespace openmsx
