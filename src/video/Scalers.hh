// $Id$

#ifndef __SCALERS_HH__
#define __SCALERS_HH__

#include <SDL/SDL.h>
#include "Blender.hh"


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
	  * @param src Source: the image to be scaled.
	  *   It should be 256 pixels wide.
	  * @param srcY Y-coordinate of the source line.
	  * @param dst Destination: image to store the scaled output in.
	  *   It should be 512 pixels wide and twice as high as the source image.
	  * @param dstY Y-coordinate of the destination line.
	  */
	virtual void scaleLine256(
		SDL_Surface* src, int srcY, SDL_Surface* dst, int dstY ) = 0;

	/** Scales the given line.
	  * @param src Source: the image to be scaled.
	  *   It should be 512 pixels wide.
	  * @param srcY Y-coordinate of the source line.
	  * @param dst Destination: image to store the scaled output in.
	  *   It should be 512 pixels wide and twice as high as the source image.
	  * @param dstY Y-coordinate of the destination line.
	  */
	virtual void scaleLine512(
		SDL_Surface* src, int srcY, SDL_Surface* dst, int dstY ) = 0;

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
	void scaleLine256(SDL_Surface* src, int srcY, SDL_Surface* dst, int dstY);
	void scaleLine512(SDL_Surface* src, int srcY, SDL_Surface* dst, int dstY);
private:
	/** Copies the line; implements both scaleLine256 and scaleLine512.
	  */
	inline void copyLine(
		SDL_Surface* src, int srcY, SDL_Surface* dst, int dstY );
};

/** 2xSaI algorithm: edge-detection which produces a rounded look.
  * Algorithm was developed by Derek Liauw Kie Fa.
  */
template <class Pixel>
class SaI2xScaler: public Scaler<Pixel>
{
public:
	SaI2xScaler(Blender<Pixel> blender);
	void scaleLine256(SDL_Surface* src, int srcY, SDL_Surface* dst, int dstY);
	void scaleLine512(SDL_Surface* src, int srcY, SDL_Surface* dst, int dstY);
//private:
protected:
	Blender<Pixel> blender;
};

} // namespace openmsx

#endif // __SCALERS_HH__
