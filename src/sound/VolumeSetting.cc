// $Id$

#include "VolumeSetting.hh"
#include "SoundDevice.hh"


VolumeSetting::VolumeSetting(const std::string &name, int initialValue,
                             SoundDevice *device_)
	: device(device_),
	  volumeSetting(name + "_volume", "the volume of this sound chip",
	                initialValue, 0, 32767)
{
	volumeSetting.registerListener(this);
}

VolumeSetting::~VolumeSetting()
{
	volumeSetting.unregisterListener(this);
}

void VolumeSetting::notify(Setting *setting)
{
	device->setVolume(volumeSetting.getValue());
}
