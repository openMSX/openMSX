#ifndef XMLELEMENT_HH
#define XMLELEMENT_HH

#include "serialize_meta.hh"
#include <utility>
#include <string>
#include <string_view>
#include <vector>
#include <memory>

namespace openmsx {

class FileContext;

class XMLElement
{
public:
	//
	// Basic functions
	//

	// Construction.
	//  (copy, assign, move, destruct are default)
	XMLElement() = default;
	XMLElement(XMLElement&&) = default;
	XMLElement& operator=(const XMLElement&) = default;
	XMLElement& operator=(XMLElement&&) = default;

	//workaround msvc bug(?)
	//XMLElement(const XMLElement&) = default;
	XMLElement(const XMLElement& e)
		: name(e.name), data(e.data)
		, children(e.children), attributes(e.attributes) {}

	// needed because the template below hides this version
	XMLElement(XMLElement& x) : XMLElement(const_cast<const XMLElement&>(x)) {}

	template<typename String>
	explicit XMLElement(String&& name_)
		: name(std::forward<String>(name_)) {}

	template<typename String1, typename String2>
	XMLElement(String1&& name_, String2&& data_)
		: name(std::forward<String1>(name_)), data(std::forward<String2>(data_)) {}

	// name
	[[nodiscard]] std::string_view getName() const { return name; }
	void clearName() { name.clear(); }

	template<typename String>
	void setName(String&& name_) { name = std::forward<String>(name_); }

	// data
	[[nodiscard]] std::string_view getData() const { return data; }

	template<typename String>
	void setData(String&& data_) {
		assert(children.empty()); // no mixed-content elements
		data = std::forward<String>(data_);
	}

	// attribute
	template<typename String1, typename String2>
	void addAttribute(String1&& attrName, String2&& value) {
		assert(!hasAttribute(attrName));
		attributes.emplace_back(std::forward<String1>(attrName),
		                        std::forward<String2>(value));
	}

	template<typename String1, typename String2>
	void setAttribute(String1&& attrName, String2&& value) {
		auto it = getAttributeIter(attrName);
		if (it != end(attributes)) {
			it->second = std::forward<String2>(value);
		} else {
			attributes.emplace_back(std::forward<String1>(attrName),
			                        std::forward<String2>(value));
		}
	}

	void removeAttribute(std::string_view name);
	[[nodiscard]] bool hasAttribute(std::string_view name) const;
	[[nodiscard]] std::string_view getAttributeValue(std::string_view attrName) const;
	[[nodiscard]] std::string_view getAttributeValue(std::string_view attrName,
	                                                 std::string_view defaultValue) const;
	// Returns ptr to attribute value, or nullptr when not found.
	[[nodiscard]] const std::string* findAttribute(std::string_view attrName) const;

	// child
	using Children = std::vector<XMLElement>;

	//  note: returned XMLElement& is validated on the next addChild() call
	template<typename String>
	XMLElement& addChild(String&& childName) {
		return children.emplace_back(std::forward<String>(childName));
	}

	template<typename String1, typename String2>
	XMLElement& addChild(String1&& childName, String2&& childData) {
		return children.emplace_back(std::forward<String1>(childName),
		                             std::forward<String2>(childData));
	}

	[[nodiscard]] const Children& getChildren() const { return children; }
	[[nodiscard]] bool hasChildren() const { return !children.empty(); }

	//
	// Convenience functions
	//

	// attribute
	[[nodiscard]] bool getAttributeValueAsBool(std::string_view attrName,
	                                           bool defaultValue) const;
	[[nodiscard]] int getAttributeValueAsInt(std::string_view attrName,
	                                         int defaultValue) const;
	[[nodiscard]] bool findAttributeInt(std::string_view attrName,
	                                    unsigned& result) const;

	// child
	[[nodiscard]] const XMLElement* findChild(std::string_view childName) const;
	[[nodiscard]] XMLElement* findChild(std::string_view childName);
	[[nodiscard]] const XMLElement& getChild(std::string_view childName) const;
	[[nodiscard]] XMLElement& getChild(std::string_view childName);

	[[nodiscard]] const XMLElement* findNextChild(std::string_view name,
	                                              size_t& fromIndex) const;

	[[nodiscard]] std::vector<const XMLElement*> getChildren(std::string_view childName) const;

	template<typename String1, typename String2>
	XMLElement& getCreateChild(String1&& childName, String2&& defaultValue) {
		if (auto* result = findChild(childName)) {
			return *result;
		}
		return addChild(std::forward<String1>(childName),
		                std::forward<String2>(defaultValue));
	}

	[[nodiscard]] std::string_view getChildData(std::string_view childName) const;
	[[nodiscard]] std::string_view getChildData(std::string_view childName,
	                                            std::string_view defaultValue) const;
	[[nodiscard]] bool getChildDataAsBool(std::string_view childName,
	                                      bool defaultValue = false) const;
	[[nodiscard]] int getChildDataAsInt(std::string_view childName,
	                                    int defaultValue) const;

	template<typename String1, typename String2>
	void setChildData(String1&& childName, String2&& value) {
		if (auto* child = findChild(childName)) {
			child->setData(std::forward<String2>(value));
		} else {
			addChild(std::forward<String1>(childName),
			         std::forward<String2>(value));
		}
	}

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	// For backwards compatibility with older savestates
	static std::unique_ptr<FileContext> getLastSerializedFileContext();

private:
	using Attribute = std::pair<std::string, std::string>;
	using Attributes = std::vector<Attribute>;
	[[nodiscard]] Attributes::iterator getAttributeIter(std::string_view attrName);
	[[nodiscard]] Attributes::const_iterator getAttributeIter(std::string_view attrName) const;

	std::string name;
	std::string data;
	Children children;
	Attributes attributes;
};
SERIALIZE_CLASS_VERSION(XMLElement, 2);

} // namespace openmsx

//-----------------------------------------------------------------------------
// Work in progress:
// - the code above this box is the old implementation
// - the code below is the new implementation
// During a (short) transition period they are both present. Later the code
// above will be deleted.
//
// Both old and new code want to have a class named XMLElement. For now the
// version in the new code is called 'NewXMLElement'. Once the old version is
// removed, we can rename 'NewXMLElement' to plain 'XMLElement'.
//
// There's also (now already) a class called 'OldXMLElement'. This is a greatly
// simplified version of the original 'XMLElement' class. And this (will be)
// used to implement loading of old savestate files.
//-----------------------------------------------------------------------------

#include "MemBuffer.hh"
#include "serialize_meta.hh"
#include <cassert>
#include <cstddef>
#include <iterator>
#include <memory_resource>
#include <string>
#include <string_view>
#include <vector>

namespace openmsx {

struct OldXMLElement; // for backwards compatible savestates

// The classes XMLDocument, XMLElement and XMLAttribute together form an
// in-memory representation of XML files (or at least the subset of the features
// needed for openMSX).
//
// This representation is optimized for fast parsing and to be compact in
// memory. This is achieved by:
// - Keeping a buffer to the actual xml file, and modifying that buffer.
//   For example the strings are modified in place (resolve escape sequences
//   and add zero-terminators) and the actual XML classes refer to this buffer.
// - The 'XMLElement' and 'XMLAttribute' objects are allocated from a monotonic
//   allocator.
// - Both the file buffer and the monotonic allocator are owned by the
//   XMLDocument class.

// Modifying the information (e.g. after it has been parsed from a file) is
// possible. But this is not the main intended use-case. For example it is
// possible to assign a new value-string to an attribute or an xml element. That
// string will be allocated from the monotonic allocator, but the memory for the
// old string will not be freed. So it's probably not a good idea to use these
// classes in a heavy-modification scenario.


// XMLAttribute is a name-value pair. This class only refers to the name and
// value strings, it does not own these strings. The owner could be either
// XMLDocument (in the file buffer or in the monotonic allocator) or it could
// be a string with static lifetime (e.g. a string-literal).
//
// XMLAttributes are organized in a (single-)linked list. So this class also
// contains a pointer to the next element in the list. The last element in the
// list contains a nullptr.
class XMLAttribute
{
public:
	XMLAttribute(const char* name_, const char* value_)
		: name(name_), value(value_) {}
	[[nodiscard]] std::string_view getName() const { return name; }
	[[nodiscard]] std::string_view getValue() const { return value; }
	void setValue(const char* value_) { value = value_; }

	XMLAttribute* setNextAttribute(XMLAttribute* attribute) {
		assert(!nextAttribute);
		nextAttribute = attribute;
		return attribute;
	}

private:
	const char* name;
	const char* value;
	XMLAttribute* nextAttribute = nullptr;

	friend class NewXMLElement;
	friend class XMLDocument;
	friend class XMLDocumentHandler;
};

// This XMLElement class represents a single XML-element (or XML-node). It has a
// name, attributes, a value (could be empty) and zero or more children. The
// value and the children are mutually exclusive, in other words: elements with
// children cannot have a non-empty value.
//
// String-ownership is the same as with XMLAttribute.
//
// Attributes are organized in a (single-)linked list. This class points to the
// first attribute (possibly nullptr when there are no attributes) and that
// attribute points to the next and so on.
//
// Hierarchy is achieved via two pointers. This class has a pointer to its first
// child and to its next sibling. Thus getting all children of a specfic
// elements requires to first follow the 'firstChild' pointer, and from there on
// follow the 'nextSibling' pointers (and stop when any of these pointers in
// nullptr). XMLElement objects do not have a pointer to their parent.
class NewXMLElement
{
	// iterator classes for children and attributes
	// TODO c++20: use iterator + sentinel instead of 2 x iterator
	struct ChildIterator {
		using difference_type = ptrdiff_t;
		using value_type = const NewXMLElement;
		using pointer = value_type*;
		using reference = value_type&;
		using iterator_category = std::forward_iterator_tag;

		const NewXMLElement* elem;

		const NewXMLElement& operator*() const { return *elem; }
		ChildIterator operator++() { elem = elem->nextSibling; return *this; }
		bool operator==(const ChildIterator& i) { return elem == i.elem; }
		bool operator!=(const ChildIterator& i) { return !(*this == i); }
	};
	struct ChildRange {
		const NewXMLElement* elem;
		ChildIterator begin() const { return {elem->firstChild}; }
		ChildIterator end()   const { return {nullptr}; }
	};

	struct NamedChildIterator {
		using difference_type = ptrdiff_t;
		using value_type = const NewXMLElement*;
		using pointer = value_type*;
		using reference = value_type&;
		using iterator_category = std::forward_iterator_tag;

		const NewXMLElement* elem;
		const std::string_view name;

		NamedChildIterator(const NewXMLElement* elem_, std::string_view name_)
			: elem(elem_), name(name_)
		{
			while (elem && elem->getName() != name) {
				elem = elem->nextSibling;
			}
		}

		const NewXMLElement* operator*() const { return elem; }
		NamedChildIterator operator++() {
			do {
				elem = elem->nextSibling;
			} while (elem && elem->getName() != name);
			return *this;
		}
		bool operator==(const NamedChildIterator& i) { return elem == i.elem; }
		bool operator!=(const NamedChildIterator& i) { return !(*this == i); }
	};
	struct NamedChildRange {
		const NewXMLElement* elem;
		std::string_view name;
		NamedChildIterator begin() const { return {elem->firstChild, name}; }
		NamedChildIterator end()   const { return {nullptr, std::string_view{}}; }
	};

	struct AttributeIterator {
		using difference_type = ptrdiff_t;
		using value_type = const XMLAttribute;
		using pointer = value_type*;
		using reference = value_type&;
		using iterator_category = std::forward_iterator_tag;

		const XMLAttribute* attr;

		const XMLAttribute& operator*() const { return *attr; }
		AttributeIterator operator++() { attr = attr->nextAttribute; return *this; }
		bool operator==(const AttributeIterator& i) { return attr == i.attr; }
		bool operator!=(const AttributeIterator& i) { return !(*this == i); }
	};
	struct AttributeRange {
		const NewXMLElement* elem;
		AttributeIterator begin() const { return {elem->firstAttribute}; }
		AttributeIterator end()   const { return {nullptr}; }
	};

public:
	NewXMLElement(const char* name_) : name(name_) {}
	NewXMLElement(const char* name_, const char* data_) : name(name_), data(data_) {}

	[[nodiscard]] std::string_view getName() const { return name; }
	void clearName() { name = ""; } // hack to 'remove' child from findChild()

	[[nodiscard]] std::string_view getData() const {
		return data ? std::string_view(data) : std::string_view();
	}
	NewXMLElement* setData(const char* data_) {
		data = data_;
		return this;
	}

	[[nodiscard]] bool hasChildren() const { return firstChild; }
	[[nodiscard]] const NewXMLElement* getFirstChild() const { return firstChild; }
	[[nodiscard]] const NewXMLElement* findChild(std::string_view childName) const;
	[[nodiscard]] const NewXMLElement* findChild(std::string_view childName, const NewXMLElement*& hint) const;
	[[nodiscard]] const NewXMLElement& getChild(std::string_view childName) const;

	[[nodiscard]] std::string_view getChildData(std::string_view childName) const;
	[[nodiscard]] std::string_view getChildData(std::string_view childName,
	                                            std::string_view defaultValue) const;
	[[nodiscard]] bool getChildDataAsBool(std::string_view childName, bool defaultValue) const;
	[[nodiscard]] int getChildDataAsInt(std::string_view childName, int defaultValue) const;

	[[nodiscard]] size_t numChildren() const;
	ChildRange getChildren() const { return {this}; }
	NamedChildRange getChildren(std::string_view childName) const { return {this, childName}; }

	[[nodiscard]] const XMLAttribute* findAttribute(std::string_view attrName) const;
	[[nodiscard]] const XMLAttribute& getAttribute(std::string_view attrName) const;
	[[nodiscard]] std::string_view getAttributeValue(std::string_view attrName) const;
	[[nodiscard]] std::string_view getAttributeValue(std::string_view attrName,
	                                                 std::string_view defaultValue) const;
	[[nodiscard]] bool getAttributeValueAsBool(std::string_view attrName,
	                                           bool defaultValue) const;
	[[nodiscard]] int getAttributeValueAsInt(std::string_view attrName,
	                                         int defaultValue) const;
	[[nodiscard]] XMLAttribute** findAttributePointer(std::string_view attrName);
	static void removeAttribute(XMLAttribute** attrPtr);
	[[nodiscard]] size_t numAttributes() const;
	AttributeRange getAttributes() const { return {this}; }

	NewXMLElement* setFirstChild(NewXMLElement* child) {
		assert(!firstChild);
		firstChild = child;
		return child;
	}
	NewXMLElement* setNextSibling(NewXMLElement* sibling) {
		assert(!nextSibling);
		nextSibling = sibling;
		return sibling;
	}
	XMLAttribute* setFirstAttribute(XMLAttribute* attribute) {
		assert(!firstAttribute);
		firstAttribute = attribute;
		return attribute;
	}

private:
	const char* name;
	const char* data = nullptr;
	NewXMLElement* firstChild = nullptr;
	NewXMLElement* nextSibling = nullptr;
	XMLAttribute* firstAttribute = nullptr;

	friend class XMLDocument;
	friend class XMLDocumentHandler;
};

// This class mainly exists to manage ownership over the objects involved in a
// full XML document. These are the XMLElement and XMLAttribute objects but also
// the name/value/attribute strings. This class also has a pointer to the root
// element.
//
// Because of the way how ownership is handled, most modifying operations are
// part of this class API. For example there's a method 'setChildData()' which
// searches for a child with a specific name, if not found such a child is
// created, then the child-data is set to a new value. Because this operation
// possibly requires to allocate a new XMLElement (and the allocator is part of
// this class) this method is part of this class rather than the XMLElement
// class.
class XMLDocument
{
public:
	// singleton-like document, mainly used as owner for static XML snippets
	// (so with lifetime the whole openMSX session)
	static XMLDocument& getStaticDocument() {
		static XMLDocument doc;
		return doc;
	}

	// Create an empty XMLDocument (root == nullptr).  All constructor
	// arguments are delegated to the monotonic allocator constructor.
	template<typename ...Args>
	XMLDocument(Args&& ...args)
		: allocator(std::forward<Args>(args)...) {}

	// Load/parse an xml file. Requires that the document is still empty.
	void load(const std::string& filename, std::string_view systemID);

	[[nodiscard]] const NewXMLElement* getRoot() const { return root; }
	void setRoot(NewXMLElement* root_) { assert(!root); root = root_; }

	[[nodiscard]] NewXMLElement* allocateElement(const char* name);
	[[nodiscard]] NewXMLElement* allocateElement(const char* name, const char* data);
	[[nodiscard]] XMLAttribute* allocateAttribute(const char* name, const char* value);
	[[nodiscard]] const char* allocateString(std::string_view str);

	[[nodiscard]] NewXMLElement* getOrCreateChild(NewXMLElement& parent, const char* childName, const char* childData);
	NewXMLElement* setChildData(NewXMLElement& parent, const char* childName, const char* childData);
	void setAttribute(NewXMLElement& elem, const char* attrName, const char* attrValue);

	template<typename Range, typename UnaryOp>
	void generateList(NewXMLElement& parent, const char* itemName, Range&& range, UnaryOp op) {
		NewXMLElement** next = &parent.firstChild;
		assert(!*next);
		for (auto& r : range) {
			auto* elem = allocateElement(itemName);
			op(elem, r);
			*next = elem;
			next = &elem->nextSibling;
		}
	}

	void load(OldXMLElement& elem); // bw compat

	void serialize(MemInputArchive&  ar, unsigned version);
	void serialize(MemOutputArchive& ar, unsigned version);
	void serialize(XmlInputArchive&  ar, unsigned version);
	void serialize(XmlOutputArchive& ar, unsigned version);

private:
	NewXMLElement* loadElement(MemInputArchive& ar);
	NewXMLElement* clone(const NewXMLElement& inElem);
	NewXMLElement* clone(const OldXMLElement& elem);

private:
	NewXMLElement* root = nullptr;
	MemBuffer<char> buf;
	std::pmr::monotonic_buffer_resource allocator;

	friend class XMLDocumentHandler;
};


// For backwards-compatibility with old savestates
//   needed with HardwareConfig-version <= 5.
class FileContext;
struct OldXMLElement
{
	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	// For backwards compatibility with version=1 savestates
	static std::unique_ptr<FileContext> getLastSerializedFileContext();

	std::string name;
	std::string data;
	std::vector<OldXMLElement> children;
	std::vector<std::pair<std::string, std::string>> attributes;
};
SERIALIZE_CLASS_VERSION(OldXMLElement, 2);

} // namespace openmsx

#endif
