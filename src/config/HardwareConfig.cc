// $Id$

#include <cassert>
#include "xmlx.hh"
#include "File.hh"
#include "FileContext.hh"
#include "HardwareConfig.hh"

namespace openmsx {

HardwareConfig::HardwareConfig()
{
}

HardwareConfig::~HardwareConfig()
{
}

HardwareConfig& HardwareConfig::instance()
{
	static HardwareConfig oneInstance;
	return oneInstance;
}

void HardwareConfig::loadHardware(FileContext& context, const string& filename)
{
	File file(context.resolve(filename));
	XMLDocument doc(file.getLocalName());

	// get url
	string url(file.getURL());
	unsigned pos = url.find_last_of('/');
	assert(pos != string::npos);	// protocol must contain a '/'
	url = url.substr(0, pos);
	PRT_DEBUG("Hardware config: url "<<url);
	
	// TODO read hwDesc from config ???
	string hwDesc;
	pos = url.find_last_of('/');
	if (pos == string::npos) {
		hwDesc = "noname";
	} else {
		hwDesc = url.substr(pos + 1);
	}

	// TODO get user name
	string userName;
	
	ConfigFileContext context2(url + '/', hwDesc, userName);
	handleDoc(doc, context2);
}

} // namespace openmsx
