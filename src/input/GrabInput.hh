#ifndef __GRABINPUT_HH__
#define __GRABINPUT_HH__

#include "Settings.hh"

class GrabInputSetting : public BooleanSetting
{
	public:
		GrabInputSetting();
		bool checkUpdate(bool newValue);
};

#endif
