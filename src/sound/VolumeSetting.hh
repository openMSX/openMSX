// $Id$

#ifndef __VOLUMESETTING_HH__
#define __VOLUMESETTING_HH__

#include "Settings.hh"

class SoundDevice;


class VolumeSetting : private SettingListener 
{
	public:
		VolumeSetting(const std::string &name, int initialValue,
		              SoundDevice *device);
		virtual ~VolumeSetting();

	private:
		virtual void notify(Setting *setting);
		
		SoundDevice *device;
		IntegerSetting volumeSetting;
};

#endif

