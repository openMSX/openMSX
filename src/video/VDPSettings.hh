// $Id$

#ifndef __VDPSETTINGS_HH__
#define __VDPSETTINGS_HH__

#include "Settings.hh"

namespace openmsx {

/** Singleton containing all settings for the VDP.
  */
class VDPSettings
{
public:
	static VDPSettings& instance();

	/** Limit number of sprites per line?
	  * If true, limit number of sprites per line as real VDP does.
	  * If false, display all sprites.
	  * For accurate emulation, this setting should be on.
	  * Turning it off can improve games with a lot of flashing sprites,
	  * such as Aleste.
	  */
	BooleanSetting* getLimitSprites() { return limitSprites; }

	/** CmdTiming [real, broken].
	  * This setting is intended for debugging only, not for users.
	  */
	EnumSetting<bool>* getCmdTiming() { return cmdTiming; }

private:
	VDPSettings();
	~VDPSettings();

	BooleanSetting* limitSprites;
	EnumSetting<bool>* cmdTiming;
};

} // namespace openmsx

#endif // __VDPSETTINGS_HH__

