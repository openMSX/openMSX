// $Id$

#ifndef SETTINGLISTENER_HH
#define SETTINGLISTENER_HH

namespace openmsx {

class Setting;

/** Interface for listening to setting changes.
  */
class SettingListener
{
public:
	/** Informs a listener of a change in a setting it subscribed to.
	  * @param setting The setting of which the value has changed.
	  */
	virtual void update(const Setting* setting) = 0;

protected:
	virtual ~SettingListener() {}
};

} // namespace openmsx

#endif
