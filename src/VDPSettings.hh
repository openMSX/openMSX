// $Id$

#ifndef __VDPSETTINGS_HH__
#define __VDPSETTINGS_HH__

#include "Settings.hh"


/** Singleton containing all settings for the VDP.
  */
class VDPSettings
{
private:
	VDPSettings();
	~VDPSettings();

	EnumSetting<bool> *cmdTiming;

public:
	/** Get singleton instance.
	  */
	static VDPSettings *instance() {
		static VDPSettings oneInstance;
		return &oneInstance;
	}

	/** CmdTiming [real, broken].
	  * This setting is intended for debugging only, not for users.
	  */
	EnumSetting<bool> *getCmdTiming() { return cmdTiming; }

};

#endif // __VDPSETTINGS_HH__

