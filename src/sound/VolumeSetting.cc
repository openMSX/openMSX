// $Id$

#include "VolumeSetting.hh"
#include "SoundDevice.hh"


VolumeSetting::VolumeSetting(const std::string &name, int initialValue,
                             SoundDevice *device_)
	: IntegerSetting(name + "_volume", "the volume of this sound chip",
	                 initialValue, 0, 32767), device(device_)
{
}

VolumeSetting::~VolumeSetting()
{
}

bool VolumeSetting::checkUpdate(int newValue)
{
	device->setVolume(newValue);
	return true;
}
