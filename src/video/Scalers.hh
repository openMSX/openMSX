// $Id$

#ifndef __SCALERS_HH__
#define __SCALERS_HH__

#include <SDL/SDL.h>
#include "Blender.hh"

namespace openmsx {

/** Abstract base class for scalers.
  * A scaler is an algorithm that converts low-res graphics to hi-res graphics.
  */
class Scaler
{
public:
	/** Enumeration of Scalers known to openMSX.
	  */
	enum ScalerID {
		/** SimpleScaler. */
		SIMPLE,
		/** SaI2xScaler. */
		SAI2X,
		/** Number of elements in enum. */
		LAST
	};

	/** Returns an array containing an instance of every Scaler subclass,
	  * indexed by ScaledID.
	  */
	template <class Pixel>
	static Scaler **createScalers(Blender<Pixel> blender);

	/** Disposes of the scalers created by the createScalers method.
	  */
	static void disposeScalers(Scaler **scalers);

	/** Scales the given line.
	  * Pixels at even X coordinates are read and written in a 2x2 square
	  * with the original pixel at the top-left corner.
	  * The scaling algorithm should preserve the colour of the original pixel.
	  * @param surface Image to scale.
	  *   This image is both the source and the destination for the scale
	  *   operation.
	  * @param y Y-coordinate of the line to scale.
	  */
	virtual void scaleLine(SDL_Surface *surface, int y) = 0;

protected:
	Scaler();
};

/** Scaler which assigns the colour of the original pixel to all pixels in
  * the 2x2 square.
  */
class SimpleScaler: public Scaler
{
public:
	SimpleScaler();
	void scaleLine(SDL_Surface *surface, int y);
};

/** 2xSaI algorithm: edge-detection which produces a rounded look.
  * Algorithm was developed by Derek Liauw Kie Fa.
  */
template <class Pixel>
class SaI2xScaler: public Scaler
{
public:
	SaI2xScaler(Blender<Pixel> blender);
	void scaleLine(SDL_Surface *surface, int y);
private:
	Blender<Pixel> blender;
};

} // namespace openmsx

#endif // __SCALERS_HH__
