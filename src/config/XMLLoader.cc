#include "XMLLoader.hh"
#include "XMLElement.hh"
#include "XMLException.hh"
#include "File.hh"
#include "FileException.hh"
#include "MemBuffer.hh"
#include "rapidsax.hh"

using std::string;
using std::string_view;

namespace openmsx::XMLLoader {

class XMLElementParser : public rapidsax::NullHandler
{
public:
	// rapidsax handler interface
	void start(string_view name);
	void attribute(string_view name, string_view value);
	void text(string_view txt);
	void stop();
	void doctype(string_view txt);

	string_view getSystemID() const { return systemID; }
	XMLElement& getRoot() { return root; }

private:
	XMLElement root;
	std::vector<XMLElement*> current;
	string_view systemID;
};

XMLElement load(string_view filename, string_view systemID)
{
	MemBuffer<char> buf;
	try {
		File file(filename);
		auto size = file.getSize();
		buf.resize(size + rapidsax::EXTRA_BUFFER_SPACE);
		file.read(buf.data(), size);
		buf[size] = 0;
	} catch (FileException& e) {
		throw XMLException(filename, ": failed to read: ", e.getMessage());
	}

	XMLElementParser handler;
	try {
		rapidsax::parse<rapidsax::trimWhitespace>(handler, buf.data());
	} catch (rapidsax::ParseError& e) {
		throw XMLException(filename, ": Document parsing failed: ", e.what());
	}
	auto& root = handler.getRoot();
	if (root.getName().empty()) {
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
	return std::move(root);
}

void XMLElementParser::start(string_view name)
{
	XMLElement* newElem;
	if (!current.empty()) {
		newElem = &current.back()->addChild(string(name));
	} else {
		root.setName(string(name));
		newElem = &root;
	}
	current.push_back(newElem);
}

void XMLElementParser::attribute(string_view name, string_view value)
{
	if (current.back()->hasAttribute(name)) {
		throw XMLException(
			"Found duplicate attribute \"", name, "\" in <",
			current.back()->getName(), ">.");
	}
	current.back()->addAttribute(string(name), string(value));
}

void XMLElementParser::text(string_view txt)
{
	if (current.back()->hasChildren()) {
		// no mixed-content elements
		throw XMLException(
			"Mixed text+subtags in <", current.back()->getName(),
			">: \"", txt, "\".");
	}
	current.back()->setData(string(txt));
}

void XMLElementParser::stop()
{
	current.pop_back();
}

void XMLElementParser::doctype(string_view txt)
{
	auto pos1 = txt.find(" SYSTEM ");
	if (pos1 == string_view::npos) return;
	if ((pos1 + 8) >= txt.size()) return;
	char q = txt[pos1 + 8];
	if ((q != '"') && (q != '\'')) return;
	auto t = txt.substr(pos1 + 9);
	auto pos2 = t.find(q);
	if (pos2 == string_view::npos) return;

	systemID = t.substr(0, pos2);
}

} // namespace openmsx::XMLLoader
