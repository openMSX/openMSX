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
	saver(this->self(), tmp);
	this->self().endTag(tag);
}

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
	loader(this->self(), tmp, make_tuple());
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
	const XMLElement::Children& children = e->getChildren();
	for (XMLElement::Children::const_iterator it = children.begin();
	     it != children.end(); ++it) {
		init(*it);
	}
	elems.push_back(e);
}

void XmlInputArchive::load(string& t)
{
	assert(elems[pos]->getChildren().empty());
	t = elems[pos]->getData();
}
void XmlInputArchive::load(bool& b)
{
	assert(elems[pos]->getChildren().empty());
	string s = elems[pos]->getData();
	if (s == "true") {
		b = true;
	} else if (s == "false") {
		b = false;
	} else {
		// throw
		assert(false);
	}
}
void XmlInputArchive::load(unsigned char& b)
{
	unsigned i;
	load(i);
	b = i;
}

void XmlInputArchive::beginTag(const string& tag)
{
	++pos;
	assert(elems[pos]->getName() == tag);
	(void)tag;
}
void XmlInputArchive::endTag(const string& tag)
{
	++pos;
	assert(elems[pos]->getName() == tag);
	(void)tag;
}

void XmlInputArchive::attribute(const string& name, string& t)
{
	assert(elems[pos]->hasAttribute(name));
	t = elems[pos]->getAttribute(name);
}
bool XmlInputArchive::hasAttribute(const string& name)
{
	return elems[pos]->hasAttribute(name);
}
int XmlInputArchive::countChildren() const
{
	return elems[pos]->getChildren().size();
}

} // namespace openmsx
