// $Id$

#ifndef __FRAMESKIPSETTING_HH__
#define __FRAMESKIPSETTING_HH__

#include "Settings.hh"

class FrameSkipSetting : public Setting
{
	public:
		FrameSkipSetting();
		std::string getValueString() const;
		void setValueString(const std::string &valueString);

		bool isAutoFrameSkip() { return autoFrameSkip; }
		int  getFrameSkip()    { return frameSkip; }
		void setFrameSkip(int v) { frameSkip = v; }
		
	private:
		bool autoFrameSkip;
		int frameSkip;
};

#endif 

