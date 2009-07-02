// $Id$

#include "serialize.hh"
#include "Base64.hh"
#include "HexDump.hh"
#include "XMLLoader.hh"
#include "XMLElement.hh"
#include "XMLException.hh"
#include "StringOp.hh"
#include <cstring>
#include <zlib.h>

using std::string;

namespace openmsx {

template<typename Derived>
void ArchiveBase<Derived>::attribute(const char* name, const char* value)
{
	string valueStr(value);
	self().attribute(name, valueStr);
}
template class ArchiveBase<MemOutputArchive>;
template class ArchiveBase<XmlOutputArchive>;

////

OutputArchiveBase2::OutputArchiveBase2()
	: lastId(0)
{
}

unsigned OutputArchiveBase2::generateID1(const void* p)
{
	#ifdef linux
	assert("Can't serialize ID of object located on the stack" &&
	       !addressOnStack(p));
	#endif
	++lastId;
	assert(polyIdMap.find(p) == polyIdMap.end());
	polyIdMap[p] = lastId;
	return lastId;
}
unsigned OutputArchiveBase2::generateID2(
	const void* p, const std::type_info& typeInfo)
{
	#ifdef linux
	assert("Can't serialize ID of object located on the stack" &&
	       !addressOnStack(p));
	#endif
	++lastId;
	IdKey key = std::make_pair(p, TypeInfo(typeInfo));
	assert(idMap.find(key) == idMap.end());
	idMap[key] = lastId;
	return lastId;
}

unsigned OutputArchiveBase2::getID1(const void* p)
{
	PolyIdMap::const_iterator it = polyIdMap.find(p);
	return it != polyIdMap.end() ? it->second : 0;
}
unsigned OutputArchiveBase2::getID2(
	const void* p, const std::type_info& typeInfo)
{
	IdKey key = std::make_pair(p, TypeInfo(typeInfo));
	IdMap::const_iterator it = idMap.find(key);
	return it != idMap.end() ? it->second : 0;
}


template<typename Derived>
void OutputArchiveBase<Derived>::serialize_blob(
	const char* tag, const void* data, unsigned len)
{
	string encoding;
	string tmp;
	if (false) {
		// useful for debugging
		encoding = "hex";
		tmp = HexDump::encode(data, len);
	} else if (false) {
		encoding = "base64";
		tmp = Base64::encode(data, len);
	} else {
		encoding = "gz-base64";
		uLongf dstLen = len + len / 1000 + 12 + 1; // worst-case
		std::vector<char> buf(dstLen);
		if (compress2(reinterpret_cast<Bytef*>(&buf[0]), &dstLen,
		              reinterpret_cast<const Bytef*>(data), len, 9)
		    != Z_OK) {
			throw MSXException("Error while compressing blob.");
		}
		tmp = Base64::encode(&buf[0], dstLen);
	}
	this->self().beginTag(tag);
	this->self().attribute("encoding", encoding);
	Saver<string> saver;
	saver(this->self(), tmp, false);
	this->self().endTag(tag);
}

template class OutputArchiveBase<MemOutputArchive>;
template class OutputArchiveBase<XmlOutputArchive>;

////

void* InputArchiveBase2::getPointer(unsigned id)
{
	IdMap::const_iterator it = idMap.find(id);
	return it != idMap.end() ? it->second : NULL;
}

void InputArchiveBase2::addPointer(unsigned id, const void* p)
{
	assert(idMap.find(id) == idMap.end());
	idMap[id] = const_cast<void*>(p);
}

template<typename Derived>
void InputArchiveBase<Derived>::serialize_blob(
	const char* tag, void* data, unsigned len)
{
	this->self().beginTag(tag);
	string encoding;
	this->self().attribute("encoding", encoding);

	string tmp;
	Loader<string> loader;
	loader(this->self(), tmp, make_tuple(), -1);
	this->self().endTag(tag);

	if (encoding == "gz-base64") {
		tmp = Base64::decode(tmp);
		uLongf dstLen = len;
		if ((uncompress(reinterpret_cast<Bytef*>(data), &dstLen,
		                reinterpret_cast<const Bytef*>(tmp.data()), uLong(tmp.size()))
		     != Z_OK) ||
		    (dstLen != len)) {
			throw MSXException("Error while decompressing blob.");
		}
	} else if ((encoding == "hex") || (encoding == "base64")) {
		if (encoding == "hex") {
			tmp = HexDump::decode(tmp);
		} else {
			tmp = Base64::decode(tmp);
		}
		if (tmp.size() != len) {
			throw XMLException(
				"Length of decoded blob: " +
				StringOp::toString(tmp.size()) +
				" not identical to expected value: " +
				StringOp::toString(len));
		}
		memcpy(data, tmp.data(), len);
	} else {
		throw XMLException("Unsupported encoding \"" + encoding + "\" for blob");
	}
}

template class InputArchiveBase<MemInputArchive>;
template class InputArchiveBase<XmlInputArchive>;

////

void MemOutputArchive::save(const std::string& s)
{
	unsigned size = unsigned(s.size());
	save(size);
	put(s.data(), size);
}

////

void MemInputArchive::load(std::string& s)
{
	unsigned length;
	load(length);
	s.resize(length);
	if (length) {
		get(&s[0], length);
	}
}

////

XmlOutputArchive::XmlOutputArchive(const string& filename)
	: current(new XMLElement("serial"))
{
	file = gzopen(filename.c_str(), "wb9");
	if (!file) {
		throw XMLException("Could not open compressed file \"" + filename + "\"");
	}
}

XmlOutputArchive::~XmlOutputArchive()
{
	const char* header =
	    "<?xml version=\"1.0\" ?>\n"
	    "<!DOCTYPE openmsx-serialize SYSTEM 'openmsx-serialize.dtd'>\n";
	gzwrite(file, const_cast<char*>(header), unsigned(strlen(header)));
	string dump = current->dump();
	delete current;
	gzwrite(file, const_cast<char*>(dump.data()), unsigned(dump.size()));
	gzclose(file);
}

void XmlOutputArchive::save(const string& str)
{
	assert(current);
	assert(current->getData().empty());
	current->setData(str);
}
void XmlOutputArchive::save(bool b)
{
	assert(current);
	assert(current->getData().empty());
	current->setData(b ? "true" : "false");
}
void XmlOutputArchive::save(unsigned char b)
{
	save(unsigned(b));
}
void XmlOutputArchive::save(signed char c)
{
	save(int(c));
}
void XmlOutputArchive::save(int i)
{
	saveImpl(i);
}
void XmlOutputArchive::save(unsigned u)
{
	saveImpl(u);
}
void XmlOutputArchive::save(unsigned long long ull)
{
	saveImpl(ull);
}

void XmlOutputArchive::attribute(const char* name, const string& str)
{
	assert(current);
	assert(!current->hasAttribute(name));
	current->addAttribute(name, str);
}

void XmlOutputArchive::beginTag(const char* tag)
{
	XMLElement* elem = new XMLElement(tag);
	if (current) {
		current->addChild(std::auto_ptr<XMLElement>(elem));
	}
	current = elem;
}
void XmlOutputArchive::endTag(const char* tag)
{
	assert(current);
	assert(current->getName() == tag);
	(void)tag;
	current = current->getParent();
}

////

XmlInputArchive::XmlInputArchive(const string& filename)
	: elem(XMLLoader::load(filename, "openmsx-serialize.dtd"))
	, pos(0)
{
	init(elem.get());
}

void XmlInputArchive::init(const XMLElement* e)
{
	elems.push_back(e);
}

void XmlInputArchive::load(string& t)
{
	if (!elems.back()->getChildren().empty()) {
		throw XMLException("No child tags expected for string types");
	}
	t = elems.back()->getData();
}
void XmlInputArchive::load(bool& b)
{
	if (!elems.back()->getChildren().empty()) {
		throw XMLException("No child tags expected for boolean types");
	}
	string s = elems.back()->getData();
	if (s == "true") {
		b = true;
	} else if (s == "false") {
		b = false;
	} else {
		throw XMLException("Bad value found for boolean: " + s);
	}
}
void XmlInputArchive::load(unsigned char& b)
{
	unsigned i;
	load(i);
	b = i;
}
void XmlInputArchive::load(signed char& c)
{
	int i;
	load(i);
	c = i;
}
void XmlInputArchive::load(int& i)
{
	loadImpl(i);
}
void XmlInputArchive::load(unsigned& u)
{
	loadImpl(u);
}
void XmlInputArchive::load(unsigned long long& ull)
{
	loadImpl(ull);
}

void XmlInputArchive::beginTag(const char* tag_)
{
	string tag(tag_);
	const XMLElement* child = elems.back()->findChild(tag);
	if (!child) {
		throw XMLException("No child tag found in begin tag \"" + tag + "\"");
	}
	elems.push_back(child);
}
void XmlInputArchive::endTag(const char* tag)
{
	if (elems.back()->getName() != tag) {
		throw XMLException("End tag \"" + elems.back()->getName() + "\" not equal to begin tag \"" + tag + "\"");
	}
	XMLElement* elem = const_cast<XMLElement*>(elems.back());
	elem->setName(""); // mark this elem for later beginTag() calls
	elems.pop_back();
}

void XmlInputArchive::attribute(const char* name, string& t)
{
	if (!hasAttribute(name)) {
		throw XMLException("Missing attribute \"" + string(name) + "\"");
	}
	t = elems.back()->getAttribute(name);
}
bool XmlInputArchive::hasAttribute(const char* name)
{
	return elems.back()->hasAttribute(name);
}
int XmlInputArchive::countChildren() const
{
	return int(elems.back()->getChildren().size());
}

} // namespace openmsx
