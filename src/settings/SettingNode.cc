// $Id$

#include "SettingNode.hh"
#include "SettingsManager.hh"
#include "SettingListener.hh"

namespace openmsx {

// SettingNode implementation:

SettingNode::SettingNode(const string &name_, const string &description_)
	: name(name_), description(description_)
{
	SettingsManager::instance().registerSetting(name, this);
}

SettingNode::~SettingNode()
{
	SettingsManager::instance().unregisterSetting(name);
}

// SettingLeafNode implementation:

SettingLeafNode::SettingLeafNode(
	const string &name_, const string &description_)
	: SettingNode(name_, description_)
{
}

SettingLeafNode::~SettingLeafNode()
{
}

void SettingLeafNode::notify() const {
	list<SettingListener *>::const_iterator it;
	for (it = listeners.begin(); it != listeners.end(); ++it) {
		(*it)->update(this);
	}
}

void SettingLeafNode::addListener(SettingListener *listener) {
	listeners.push_back(listener);
}

void SettingLeafNode::removeListener(SettingListener *listener) {
	listeners.remove(listener);
}

} // namespace openmsx
