// $Id$

#include "Scaler.hh"
#include "SimpleScaler.hh"
#include "SaI2xScaler.hh"
#include "Scale2xScaler.hh"
#include "HQ2xScaler.hh"
#include <cstring>


namespace openmsx {

// Force template instantiation.
template class Scaler<word>;
template class Scaler<unsigned int>;

template <class Pixel>
auto_ptr<Scaler<Pixel> > Scaler<Pixel>::createScaler(
	ScalerID id, SDL_PixelFormat* format)
{
	switch(id) {
	case SCALER_SIMPLE:
		return auto_ptr<Scaler<Pixel> >(new SimpleScaler<Pixel>());
	case SCALER_SAI2X:
		return auto_ptr<Scaler<Pixel> >(new SaI2xScaler<Pixel>(format));
	case SCALER_SCALE2X:
		return auto_ptr<Scaler<Pixel> >(new Scale2xScaler<Pixel>());
	case SCALER_HQ2X:
		return auto_ptr<Scaler<Pixel> >(new HQ2xScaler<Pixel>());
	default:
		assert(false);
		return auto_ptr<Scaler<Pixel> >();
	}
}

template <class Pixel>
void Scaler<Pixel>::copyLine(
	SDL_Surface* src, int srcY, SDL_Surface* dst, int dstY )
{
	assert(src->w == dst->w);
	memcpy(
		(byte*)linePtr(dst, dstY),
		(byte*)linePtr(src, srcY),
		dst->w * sizeof(Pixel)
		);
}

template <class Pixel>
Scaler<Pixel>::Scaler()
{
}

template <class Pixel>
void Scaler<Pixel>::scaleBlank(
	Pixel colour,
	SDL_Surface* dst, int dstY, int endDstY
) {
	SDL_Rect rect;
	rect.x = 0;
	rect.w = dst->w;
	rect.y = dstY;
	rect.h = endDstY - dstY;
	// Note: SDL_FillRect is generally not allowed on locked surfaces.
	//       However, we're using a software surface, which doesn't
	//       have locking.
	assert(!SDL_MUSTLOCK(dst));
	// Note: return code ignored.
	SDL_FillRect(dst, &rect, colour);
}

template <class Pixel>
void Scaler<Pixel>::scale256(
	SDL_Surface* src, int srcY, int endSrcY,
	SDL_Surface* dst, int dstY )
{
	const int WIDTH = 320; // TODO: Specify this in a clean way.
	assert(dst->w == WIDTH * 2);
	while (srcY < endSrcY) {
		const Pixel* srcLine = linePtr(src, srcY++);
		Pixel* dstUpper = linePtr(dst, dstY++);
		Pixel* dstLower =
			dstY == dst->h ? dstUpper : linePtr(dst, dstY++);
		for (int x = 0; x < WIDTH; x++) {
			dstUpper[x * 2] = dstUpper[x * 2 + 1] =
			dstLower[x * 2] = dstLower[x * 2 + 1] =
				srcLine[x];
		}
	}
}

template <class Pixel>
void Scaler<Pixel>::scale512(
	SDL_Surface* src, int srcY, int endSrcY,
	SDL_Surface* dst, int dstY )
{
	while (srcY < endSrcY) {
		copyLine(src, srcY, dst, dstY++);
		if (dstY == dst->h) break;
		copyLine(src, srcY, dst, dstY++);
		srcY++;
	}
}

} // namespace openmsx

