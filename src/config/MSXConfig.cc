// $Id$

#include <cassert>
#include "MSXConfig.hh"
#include "Config.hh"
#include "Device.hh"

namespace openmsx {

MSXConfig::MSXConfig()
{
}

MSXConfig::~MSXConfig()
{
	list<XML::Document*>::iterator doc;
	for (doc = docs.begin(); doc != docs.end(); doc++) {
		delete (*doc);
	}
	list<Config*>::iterator i;
	for (i = configs.begin(); i != configs.end(); i++) {
		delete (*i);
	}
	list<Device*>::iterator j;
	for (j = devices.begin(); j != devices.end(); j++) {
		delete (*j);
	}
}

MSXConfig& MSXConfig::instance()
{
	static MSXConfig oneInstance;
	return oneInstance;
}

void MSXConfig::loadHardware(FileContext &context,
                             const string &filename)
	throw(FileException, ConfigException)
{
	File file(context.resolve(filename));
	XML::Document *doc = new XML::Document(file.getLocalName());

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

void MSXConfig::loadSetting(FileContext &context,
                            const string &filename)
	throw(FileException, ConfigException)
{
	File file(context.resolve(filename));
	XML::Document *doc = new XML::Document(file.getLocalName());
	SettingFileContext context2(file.getURL());
	handleDoc(doc, context2);
}

void MSXConfig::loadStream(FileContext &context,
                           const ostringstream &stream)
	throw(ConfigException)
{
	XML::Document* doc = new XML::Document(stream);
	handleDoc(doc, context);
}

void MSXConfig::handleDoc(XML::Document* doc, FileContext &context)
	throw(ConfigException)
{
	docs.push_back(doc);
	// TODO update/append Devices/Configs
	for (list<XML::Element*>::const_iterator i = doc->root->children.begin();
	     i != doc->root->children.end();
	     ++i) {
		if (((*i)->name == "config") || ((*i)->name == "device")) {
			string id((*i)->getAttribute("id"));
			if (id == "") {
				throw ConfigException(
					"<config> or <device> is missing 'id'");
			}
			if (hasConfigWithId(id)) {
				throw ConfigException(
				    "<config> or <device> with duplicate 'id':" + id);
			}
			if ((*i)->name == "config") {
				configs.push_back(new Config(*i, context));
			} else if ((*i)->name == "device") {
				devices.push_back(new Device(*i, context));
			}
		}
	}
}

bool MSXConfig::hasConfigWithId(const string &id)
{
	return findConfigById(id);
}

Config* MSXConfig::getConfigById(const string &id)
	throw(ConfigException)
{
	Config* result = findConfigById(id);
	if (!result) {
		throw ConfigException("<config> or <device> with id:" + id +
		                      " not found");
	}
	return result;
}

Config* MSXConfig::findConfigById(const string &id)
{
	for (list<Config*>::const_iterator i = configs.begin();
	     i != configs.end();
	     ++i) {
		if ((*i)->getId() == id) {
			return (*i);
		}
	}
	for (list<Device*>::const_iterator i = devices.begin();
	     i != devices.end();
	     ++i) {
		if ((*i)->getId() == id) {
			return (*i);
		}
	}
	return NULL;
}

Device* MSXConfig::getDeviceById(const string &id)
	throw(ConfigException)
{
	for (list<Device*>::const_iterator i = devices.begin();
	     i != devices.end();
	     ++i) {
		if ((*i)->getId() == id) {
			return (*i);
		}
	}
	throw ConfigException("<device> with id:" + id + " not found");
}

void MSXConfig::initDeviceIterator()
{
	device_iterator = devices.begin();
}

Device* MSXConfig::getNextDevice()
{
	if (device_iterator != devices.end()) {
		Device* t= (*device_iterator);
		++device_iterator;
		return t;
	}
	return 0;
}

} // namespace openmsx
