// $Id$

#ifndef __SETTINGLISTENER_HH__
#define __SETTINGLISTENER_HH__

namespace openmsx {

class SettingLeafNode;

/** Interface for listening to setting changes.
  */
class SettingListener
{
public:
	/** Informs a listener of a change in a setting it subscribed to.
	  * @param setting The setting of which the value has changed.
	  */
	virtual void update(const SettingLeafNode* setting) = 0;
};

} // namespace openmsx

#endif //__SETTINGLISTENER_HH__
