// $Id$

#ifndef __VOLUMESETTING_HH__
#define __VOLUMESETTING_HH__

#include "Settings.hh"

class SoundDevice;


class VolumeSetting : public IntegerSetting
{
	public:
		VolumeSetting(const std::string &name, int initialValue,
		              SoundDevice *device);
		virtual ~VolumeSetting();

		virtual bool checkUpdate(int newValue);

	private:
		SoundDevice *device;
};

#endif

