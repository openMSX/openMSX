// $Id$

#include "VolumeSetting.hh"
#include "SoundDevice.hh"

#include <cassert>


namespace openmsx {

VolumeSetting::VolumeSetting(const string &name, int initialValue,
                             SoundDevice *device_)
	: device(device_),
	  volumeSetting(name + "_volume", "the volume of this sound chip",
	                initialValue, 0, 32767)
{
	volumeSetting.addListener(this);
}

VolumeSetting::~VolumeSetting()
{
	volumeSetting.removeListener(this);
}

void VolumeSetting::update(const SettingLeafNode *setting)
{
	assert(setting == &volumeSetting);
	device->setVolume(volumeSetting.getValue());
}

} // namespace openmsx
