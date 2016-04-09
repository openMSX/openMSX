#include "XMLLoader.hh"
#include "XMLElement.hh"
#include "XMLException.hh"
#include "File.hh"
#include "FileException.hh"
#include "MemBuffer.hh"
#include "rapidsax.hh"

namespace openmsx {
namespace XMLLoader {

class XMLElementParser : public rapidsax::NullHandler
{
public:
	// rapidsax handler interface
	void start(string_ref name);
	void attribute(string_ref name, string_ref value);
	void text(string_ref text);
	void stop();
	void doctype(string_ref text);

	string_ref getSystemID() const { return systemID; }
	XMLElement& getRoot() { return root; }

private:
	XMLElement root;
	std::vector<XMLElement*> current;
	string_ref systemID;
};

XMLElement load(string_ref filename, string_ref systemID)
{
	MemBuffer<char> buf;
	try {
		File file(filename);
		auto size = file.getSize();
		buf.resize(size + rapidsax::EXTRA_BUFFER_SPACE);
		file.read(buf.data(), size);
		buf[size] = 0;
	} catch (FileException& e) {
		throw XMLException(filename + ": failed to read: " + e.getMessage());
	}

	XMLElementParser handler;
	try {
		rapidsax::parse<rapidsax::trimWhitespace>(handler, buf.data());
	} catch (rapidsax::ParseError& e) {
		throw XMLException(filename + ": Document parsing failed: " + e.what());
	}
	auto& root = handler.getRoot();
	if (root.getName().empty()) {
		throw XMLException(filename +
			": Document doesn't contain mandatory root Element");
	}
	if (handler.getSystemID().empty()) {
		throw XMLException(filename + ": Missing systemID.\n"
			"You're probably using an old incompatible file format.");
	}
	if (handler.getSystemID() != systemID) {
		throw XMLException(filename + ": systemID doesn't match "
			"(expected " + systemID + ", got " + handler.getSystemID() + ")\n"
			"You're probably using an old incompatible file format.");
	}
	return std::move(root);
}

void XMLElementParser::start(string_ref name)
{
	XMLElement* newElem;
	if (!current.empty()) {
		newElem = &current.back()->addChild(name);
	} else {
		root.setName(name);
		newElem = &root;
	}
	current.push_back(newElem);
}

void XMLElementParser::attribute(string_ref name, string_ref value)
{
	current.back()->addAttribute(name, value);
}

void XMLElementParser::text(string_ref txt)
{
	current.back()->setData(txt);
}

void XMLElementParser::stop()
{
	current.pop_back();
}

void XMLElementParser::doctype(string_ref txt)
{
	auto pos1 = txt.find(" SYSTEM ");
	if (pos1 == string_ref::npos) return;
	char q = txt[pos1 + 8];
	if ((q != '"') && (q != '\'')) return;
	auto t = txt.substr(pos1 + 9);
	auto pos2 = t.find(q);
	if (pos2 == string_ref::npos) return;

	systemID = t.substr(0, pos2);
}

} // namespace XMLLoader
} // namespace openmsx
