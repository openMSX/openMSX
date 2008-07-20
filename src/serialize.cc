// $Id$

#include "serialize.hh"
#include "Base64.hh"
#include "HexDump.hh"
#include "XMLLoader.hh"
#include "XMLElement.hh"
#include <cstring>
#include <zlib.h>

using std::string;

namespace openmsx {

template<typename Derived>
OutputArchiveBase<Derived>::OutputArchiveBase()
	: lastId(0)
{
}

template<typename Derived>
void OutputArchiveBase<Derived>::serialize_blob(
	const char* tag, const void* data, unsigned len)
{
	string encoding;
	string tmp;
	if (true) {
		encoding = "hex";
		tmp = HexDump::encode(data, len);
	} else {
		encoding = "base64";
		tmp = Base64::encode(data, len);
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

template<typename Derived>
InputArchiveBase<Derived>::InputArchiveBase()
{
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

	string tmp2;
	if (encoding == "hex") {
		tmp2 = HexDump::decode(tmp);
	} else if (encoding == "base64") {
		tmp2 = Base64::decode(tmp);
	} else {
		assert(false); // TODO exception
	}
	assert(tmp2.size() == len); // TODO exception
	memcpy(data, tmp2.data(), len);
}

template<typename Derived>
void* InputArchiveBase<Derived>::getPointer(unsigned id)
{
	IdMap::const_iterator it = idMap.find(id);
	return it != idMap.end() ? it->second : NULL;
}

template<typename Derived>
void InputArchiveBase<Derived>::addPointer(unsigned id, const void* p)
{
	assert(idMap.find(id) == idMap.end());
	idMap[id] = const_cast<void*>(p);
}

template class InputArchiveBase<MemInputArchive>;
template class InputArchiveBase<XmlInputArchive>;

////

XmlOutputArchive::XmlOutputArchive(const string& filename)
	: current(new XMLElement("serial"))
{
	file = gzopen((filename + ".gz").c_str(), "wb9");
	assert(file); // TODO
}

XmlOutputArchive::~XmlOutputArchive()
{
	const char* header =
	    "<?xml version=\"1.0\" ?>\n"
	    "<!DOCTYPE openmsx-serialize SYSTEM 'openmsx-serialize.dtd'>\n";
	gzwrite(file, const_cast<char*>(header), strlen(header));
	string dump = current->dump();
	delete current;
	gzwrite(file, const_cast<char*>(dump.data()), dump.size());
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

void XmlOutputArchive::attribute(const string& name, const string& str)
{
	assert(current);
	assert(!current->hasAttribute(name));
	current->addAttribute(name, str);
}

void XmlOutputArchive::beginTag(const string& tag)
{
	XMLElement* elem = new XMLElement(tag);
	if (current) {
		current->addChild(std::auto_ptr<XMLElement>(elem));
	}
	current = elem;
}
void XmlOutputArchive::endTag(const string& tag)
{
	assert(current);
	assert(current->getName() == tag);
	(void)tag;
	current = current->getParent();
}

////

XmlInputArchive::XmlInputArchive(const string& filename)
	: elem(XMLLoader::loadXML(filename, "openmsx-serialize.dtd"))
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
	assert(elems.back()->getChildren().empty()); // throw
	t = elems.back()->getData();
}
void XmlInputArchive::load(bool& b)
{
	assert(elems.back()->getChildren().empty()); // throw
	string s = elems.back()->getData();
	if (s == "true") {
		b = true;
	} else if (s == "false") {
		b = false;
	} else {
		assert(false); // throw
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

void XmlInputArchive::beginTag(const string& tag)
{
	const XMLElement* child = elems.back()->findChild(tag);
	assert(child); // throw
	elems.push_back(child);
}
void XmlInputArchive::endTag(const string& tag)
{
	assert(elems.back()->getName() == tag); // throw
	XMLElement* elem = const_cast<XMLElement*>(elems.back());
	elem->setName(""); // mark this elem for later beginTag() calls
	elems.pop_back();
}

void XmlInputArchive::attribute(const string& name, string& t)
{
	assert(hasAttribute(name)); // throw
	t = elems.back()->getAttribute(name);
}
bool XmlInputArchive::hasAttribute(const string& name)
{
	return elems.back()->hasAttribute(name);
}
int XmlInputArchive::countChildren() const
{
	return elems.back()->getChildren().size();
}

} // namespace openmsx
