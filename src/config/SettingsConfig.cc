// $Id$

#include "xmlx.hh"
#include "File.hh"
#include "FileContext.hh"
#include "SettingsConfig.hh"

namespace openmsx {

SettingsConfig::SettingsConfig()
{
}

SettingsConfig::~SettingsConfig()
{
}

SettingsConfig& SettingsConfig::instance()
{
	static SettingsConfig oneInstance;
	return oneInstance;
}

void SettingsConfig::loadSetting(FileContext& context, const string& filename)
{
	File file(context.resolve(filename));
	XMLDocument doc(file.getLocalName());
	handleDoc(*this, doc, context);
}

} // namespace openmsx
