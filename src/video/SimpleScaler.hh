// $Id$

#ifndef __SIMPLESCALER_HH__
#define __SIMPLESCALER_HH__

#include "Scaler.hh"
#include "SettingListener.hh"


namespace openmsx {

class IntegerSetting;

/** Scaler which assigns the colour of the original pixel to all pixels in
  * the 2x2 square.
  */
template <class Pixel>
class SimpleScaler: public Scaler<Pixel>, private SettingListener
{
public:
	SimpleScaler();
	virtual ~SimpleScaler();
	virtual void scaleBlank(
		Pixel colour,
		SDL_Surface* dst, int dstY, int endDstY );
	virtual void scale256(
		SDL_Surface* src, int srcY, int endSrcY,
		SDL_Surface* dst, int dstY );
	virtual void scale512(
		SDL_Surface* src, int srcY, int endSrcY,
		SDL_Surface* dst, int dstY );
private:
	// SettingListener interface:
	virtual void update(const SettingLeafNode* setting);
	
	IntegerSetting* scanlineAlphaSetting;
	/** Current alpha value, range [0..255]. */
	int scanlineAlpha;
};

} // namespace openmsx

#endif // __SIMPLESCALER_HH__
