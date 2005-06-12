// $Id$

#include "HardwareConfig.hh"
#include "XMLLoader.hh"
#include "File.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include <cassert>
#include <memory.h>

using std::auto_ptr;
using std::string;

namespace openmsx {

HardwareConfig::HardwareConfig()
	: XMLElement("hardware")
{
	setFileContext(auto_ptr<FileContext>(new SystemFileContext()));
}

HardwareConfig::~HardwareConfig()
{
}

HardwareConfig& HardwareConfig::instance()
{
	static HardwareConfig oneInstance;
	return oneInstance;
}

void HardwareConfig::loadHardware(XMLElement& root, const string& path,
                                  const string& hwName)
{
	SystemFileContext context;
	string filename;
	try {
		filename = context.resolve(
			path + '/' + hwName + ".xml");
	} catch (FileException& e) {
		filename = context.resolve(
			path + '/' + hwName + "/hardwareconfig.xml");
	}
	try {
		File file(filename);
		auto_ptr<XMLElement> doc(XMLLoader::loadXML(
			file.getLocalName(), "msxconfig2.dtd", &idMap));

		// get url
		string url(file.getURL());
		string::size_type pos = url.find_last_of('/');
		assert(pos != string::npos);	// protocol must contain a '/'
		url = url.substr(0, pos);
		PRT_DEBUG("Hardware config: url "<<url);

		// TODO get user name
		string userName;

		ConfigFileContext context2(url + '/', hwName, userName);
		while (!doc->getChildren().empty()) {
			auto_ptr<XMLElement> elem(doc->removeChild(
				*doc->getChildren().front()));
			elem->setFileContext(auto_ptr<FileContext>(context2.clone()));
			root.addChild(elem);
		}

	} catch (XMLException& e) {
		throw FatalError(
			"Loading of hardware configuration failed: " + e.getMessage() );
	}
}

string HardwareConfig::makeUnique(const string& str)
{
	return XMLLoader::makeUnique(str, idMap);
}

} // namespace openmsx
