// $Id$

#ifndef __VDPSETTINGS_HH__
#define __VDPSETTINGS_HH__

#include <memory>
#include "EnumSetting.hh"
#include "BooleanSetting.hh"

using std::auto_ptr;

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
	BooleanSetting* getLimitSprites() { return limitSprites.get(); }

	/** CmdTiming [real, broken].
	  * This setting is intended for debugging only, not for users.
	  */
	EnumSetting<bool>* getCmdTiming() { return cmdTiming.get(); }

private:
	VDPSettings();
	~VDPSettings();

	auto_ptr<BooleanSetting> limitSprites;
	auto_ptr<EnumSetting<bool> > cmdTiming;
};

} // namespace openmsx

#endif // __VDPSETTINGS_HH__

