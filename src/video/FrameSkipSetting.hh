// $Id$

#ifndef __FRAMESKIPSETTING_HH__
#define __FRAMESKIPSETTING_HH__

#include "Setting.hh"

#include <cassert>


namespace openmsx {

/*
TODO:
It makes sense so split auto and the skip amount:
- Simplifies the code.
- Makes it easier to support multiple frame skip mechanisms:
  "off", "auto", "fixed"
  The "off" strategy is not strictly necessary, but clearer for users.

However, this requires some changes in PixelRenderer, so I decided
to do it later.
*/

class FrameSkip {

public:
	FrameSkip(bool automatic, int frameSkip) {
		this->automatic = automatic;
		this->frameSkip = frameSkip;
	}

	/** Is the frameskip value determined by the emulator dynamically?
	  */
	bool isAutoFrameSkip() const {
		return automatic;
	}

	/** Gets the number of frames to skip for every frame displayed.
	  */
	int getFrameSkip() const {
		return frameSkip;
	}

	/** Gets a skip value relative to the current one.
	  * Should only be called if isAutoFrameSkip returns false.
	  */
	FrameSkip modify(int delta) const {
		int newFrameSkip = frameSkip + delta;
		if (newFrameSkip < 0) newFrameSkip = 0;
		return FrameSkip(automatic, newFrameSkip);
	}

	bool operator ==(const FrameSkip& other) const {
		return this->automatic == other.automatic
			&& this->frameSkip == other.frameSkip;
	}

	bool operator !=(const FrameSkip& other) const {
		return !(*this == other);
	}

private:
	bool automatic;
	int frameSkip;
};

class FrameSkipSetting : public Setting<FrameSkip>
{
public:
	FrameSkipSetting();
	virtual ~FrameSkipSetting();

	virtual string getValueString() const;
	virtual void setValueString(const string& valueString)
		throw(CommandException);
	void setValue(const FrameSkip& newValue);
};

} // namespace openmsx

#endif // __FRAMESKIPSETTING_HH__

