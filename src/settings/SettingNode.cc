// $Id$

#include "SettingNode.hh"
#include "SettingListener.hh"

namespace openmsx {

// SettingNode implementation:

SettingNode::SettingNode(const string& name_, const string& description_)
	: name(name_), description(description_)
{
}

SettingNode::~SettingNode()
{
}

// SettingLeafNode implementation:

SettingLeafNode::SettingLeafNode(const string& name, const string& description)
	: SettingNode(name, description)
{
}

SettingLeafNode::~SettingLeafNode()
{
}

void SettingLeafNode::notify() const
{
	for (list<SettingListener*>::const_iterator it = listeners.begin();
	     it != listeners.end(); ++it) {
		(*it)->update(this);
	}
}

void SettingLeafNode::addListener(SettingListener*listener)
{
	listeners.push_back(listener);
}

void SettingLeafNode::removeListener(SettingListener*listener)
{
	listeners.remove(listener);
}

} // namespace openmsx
