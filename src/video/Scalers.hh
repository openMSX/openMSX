// $Id$

#ifndef __SCALERS_HH__
#define __SCALERS_HH__

#include <vector>
#include <SDL/SDL.h>
#include "Blender.hh"

using std::vector;

namespace openmsx {

/** Enumeration of Scalers known to openMSX.
*/
enum ScalerID {
	/** SimpleScaler. */
	SCALER_SIMPLE,
	/** SaI2xScaler. */
	SCALER_SAI2X,
};

/** Abstract base class for scalers.
  * A scaler is an algorithm that converts low-res graphics to hi-res graphics.
  */
template <class Pixel>
class Scaler
{
public:
	/** Instantiates a Scaler.
	  * @param id Identifies the scaler algorithm.
	  * @param blender Pixel blender that can be used by the scaler algorithm
	  *   to interpolate pixels.
	  * @return A Scaler object, owned by the caller.
	  */
	static Scaler* createScaler(ScalerID id, Blender<Pixel> blender);

	/** Scales the given line.
	  * Pixels at even X coordinates are read and written in a 2x2 square
	  * with the original pixel at the top-left corner.
	  * The scaling algorithm should preserve the colour of the original pixel.
	  * @param surface Image to scale.
	  *   This image is both the source and the destination for the scale
	  *   operation.
	  * @param y Y-coordinate of the line to scale.
	  */
	virtual void scaleLine256(SDL_Surface* surface, int y) = 0;

	/** Scales the given line.
	  * Pixels on even lines are read and the odd lines are written.
	  * The scaling algorithm should preserve the content of the even lines.
	  * @param surface Image to scale.
	  *   This image is both the source and the destination for the scale
	  *   operation.
	  * @param y Y-coordinate of the line to scale.
	  */
	virtual void scaleLine512(SDL_Surface* surface, int y) = 0;

protected:
	Scaler();
};

/** Scaler which assigns the colour of the original pixel to all pixels in
  * the 2x2 square.
  */
template <class Pixel>
class SimpleScaler: public Scaler<Pixel>
{
public:
	SimpleScaler();
	void scaleLine256(SDL_Surface* surface, int y);
	void scaleLine512(SDL_Surface* surface, int y);
private:
	/** Copies the line; implements both scaleLine256 and scaleLine512.
	  */
	inline void copyLine(SDL_Surface* surface, int y);
};

/** 2xSaI algorithm: edge-detection which produces a rounded look.
  * Algorithm was developed by Derek Liauw Kie Fa.
  */
template <class Pixel>
class SaI2xScaler: public Scaler<Pixel>
{
public:
	SaI2xScaler(Blender<Pixel> blender);
	void scaleLine256(SDL_Surface* surface, int y);
	void scaleLine512(SDL_Surface* surface, int y);
//private:
protected:
	Blender<Pixel> blender;
};

} // namespace openmsx

#endif // __SCALERS_HH__
