// $Id$

#ifndef __VOLUMESETTING_HH__
#define __VOLUMESETTING_HH__

#include "Settings.hh"

namespace openmsx {

class SoundDevice;


class VolumeSetting : private SettingListener
{
	public:
		VolumeSetting(const string &name, int initialValue,
		              SoundDevice *device);
		virtual ~VolumeSetting();

	private:
		virtual void update(const SettingLeafNode *setting);

		SoundDevice *device;
		IntegerSetting volumeSetting;
};

} // namespace openmsx

#endif

