// $Id$

#include <stdarg.h>

#include "V9990BitmapConverter.hh"
#include "GLUtil.hh"

namespace openmsx {

// Force template instantiation
template class V9990BitmapConverter<word, Renderer::ZOOM_256>;
template class V9990BitmapConverter<word, Renderer::ZOOM_REAL>;
template class V9990BitmapConverter<unsigned int, Renderer::ZOOM_256>;
template class V9990BitmapConverter<unsigned int, Renderer::ZOOM_REAL>;

#ifdef COMPONENT_GL
// On some systems, "GLuint" is not equivalent to "unsigned int",
// so BitmapConverter must be instantiated separately for those systems.
// But on systems where it is equivalent, it's an error to expand
// the same template twice.
// The following piece of template metaprogramming expands
// V9990BitmapConverter<GLuint, Renderer::ZOOM_REAL> to an empty class if
// "GLuint" is equivalent to "unsigned int"; otherwise it is expanded to
// the actual V9990BitmapConverter implementation.
//
// See BitmapConverter.cc
class NoExpansion {};
// ExpandFilter::ExpandType = (Type == unsigned int ? NoExpansion : Type)
template <class Type> class ExpandFilter {
	typedef Type ExpandType;
};
template <> class ExpandFilter<unsigned int> {
	typedef NoExpansion ExpandType;
};
template <Renderer::Zoom zoom> class V9990BitmapConverter<NoExpansion, zoom> {};
template class V9990BitmapConverter<
	ExpandFilter<GLuint>::ExpandType, Renderer::ZOOM_REAL >;
#endif // COMPONENT_GL

template <class Pixel, Renderer::Zoom zoom>
V9990BitmapConverter<Pixel, zoom>::V9990BitmapConverter(
	SDL_PixelFormat fmt,
	Pixel *palette64, Pixel *palette256, Pixel *palette32768)
	: format(fmt)
{
	this->palette64    = palette64;
	this->palette256   = palette256;
	this->palette32768 = palette32768;
}

template <class Pixel, Renderer::Zoom zoom>
V9990BitmapConverter<Pixel, zoom>::~V9990BitmapConverter()
{
}

// - Rasterizers -------------------------------------------------------

template <class Pixel, Renderer::Zoom zoom>
void V9990BitmapConverter<Pixel, zoom>::rasterBYUV(
		Pixel *pixelPtr, const byte *vramPtr, int nrPixels)
{
	int r, g, b;
	int y, u, v;

	while(nrPixels > 0) {
		v = vramPtr[0] & 0x07 + ((vramPtr[1] & 0x07) << 3);
		u = vramPtr[2] & 0x07 + ((vramPtr[3] & 0x07) << 3);
		for(int i=0; i < 4; i++) {
			y = (*vramPtr++ & 0xF8) >> 3;
			r = y + u;
			g = (5*y - 2*u - v) / 4;
			b = y + v;
			*pixelPtr++ = palette32768[(((r << 5) + g) << 5) + b];
		}
		nrPixels -= 4;
	}
}

template <class Pixel, Renderer::Zoom zoom>
void V9990BitmapConverter<Pixel, zoom>::rasterBYUVP(
		Pixel *pixelPtr, const byte *vramPtr, int nrPixels)
{
	// TODO Use palette
	int r, g, b;
	int y, j, k;

	while(nrPixels > 0) {
		k = vramPtr[0] & 0x07 + ((vramPtr[1] & 0x07) << 3);
		j = vramPtr[2] & 0x07 + ((vramPtr[3] & 0x07) << 3);
		for(int i=0; i < 4; i++) {
			y = (*vramPtr++ & 0xF8) >> 3;
			r = y + j;
			g = y + k;
			b = (5*y - 2*j - k) / 4;
			*pixelPtr++ = palette32768[(((r << 5) + g) << 5) + b];
		}
		nrPixels -= 4;
	}
}

template <class Pixel, Renderer::Zoom zoom>
void V9990BitmapConverter<Pixel, zoom>::rasterBYJK(
		Pixel *pixelPtr, const byte *vramPtr, int nrPixels)
{
	int r, g, b;
	int y, j, k;

	while(nrPixels > 0) {
		k = vramPtr[0] & 0x07 + ((vramPtr[1] & 0x07) << 3);
		j = vramPtr[2] & 0x07 + ((vramPtr[3] & 0x07) << 3);
		for(int i=0; i < 4; i++) {
			y = (*vramPtr++ & 0xF8) >> 3;
			r = y + j;
			g = y + k;
			b = (5*y - 2*j - k) / 4;
			*pixelPtr++ = palette32768[(((r << 5) + g) << 5) + b];
		}
		nrPixels -= 4;
	}
}

template <class Pixel, Renderer::Zoom zoom>
void V9990BitmapConverter<Pixel, zoom>::rasterBYJKP(
		Pixel *pixelPtr, const byte *vramPtr, int nrPixels)
{
	// TODO Use palette
	int r, g, b;
	int y, j, k;

	while(nrPixels > 0) {
		k = vramPtr[0] & 0x07 + ((vramPtr[1] & 0x07) << 3);
		j = vramPtr[2] & 0x07 + ((vramPtr[3] & 0x07) << 3);
		for(int i=0; i < 4; i++) {
			y = (*vramPtr++ & 0xF8) >> 3;
			r = y + j;
			g = y + k;
			b = (5*y - 2*j - k) / 4;
			*pixelPtr++ = palette32768[(((r << 5) + g) << 5) + b];
		}
		nrPixels -= 4;
	}
}

template <class Pixel, Renderer::Zoom zoom>
void V9990BitmapConverter<Pixel, zoom>::rasterBD16(
		Pixel *pixelPtr, const byte *vramPtr, int nrPixels)
{
	while(nrPixels--) {
		int r = (vramPtr[0] >> 4) & 0x03 | (vramPtr[1] << 2) & 0x1C;
		int g = (vramPtr[1] >> 2) & 0x1F;
		int b = vramPtr[0] & 0x1F;
		*pixelPtr++ = palette32768[(((r << 5) + g) << 5) + b];
		vramPtr += 2;
	}
}

template <class Pixel, Renderer::Zoom zoom>
void V9990BitmapConverter<Pixel, zoom>::rasterBD8(
		Pixel *pixelPtr, const byte *vramPtr, int nrPixels)
{
	while(nrPixels--) {
		*pixelPtr++ = palette256[*vramPtr++];
	}
}

template <class Pixel, Renderer::Zoom zoom>
void V9990BitmapConverter<Pixel, zoom>::rasterBP6(
		Pixel *pixelPtr, const byte *vramPtr, int nrPixels)
{
	while(nrPixels--) {
		*pixelPtr++ = palette64[*vramPtr++ & 0x3F];
	}
}

template <class Pixel, Renderer::Zoom zoom>
void V9990BitmapConverter<Pixel, zoom>::rasterBP4(
		Pixel *pixelPtr, const byte *vramPtr, int nrPixels)
{
	while(nrPixels > 0) {
		*pixelPtr++ = palette64[(*vramPtr & 0xF0) >> 4];
		*pixelPtr++ = palette64[*vramPtr++ & 0x0F];
		nrPixels -= 2;
	}
}

template <class Pixel, Renderer::Zoom zoom>
void V9990BitmapConverter<Pixel, zoom>::rasterBP2(
		Pixel *pixelPtr, const byte *vramPtr, int nrPixels)
{
	while(nrPixels > 0) {
		*pixelPtr++ = palette64[(*vramPtr & 0xC0) >> 6];
		*pixelPtr++ = palette64[(*vramPtr & 0x30) >> 4];
		*pixelPtr++ = palette64[(*vramPtr & 0x0C) >> 2];
		*pixelPtr++ = palette64[*vramPtr++ & 0x03];
		nrPixels -= 4;
	}
}

template <class Pixel, Renderer::Zoom zoom>
void V9990BitmapConverter<Pixel, zoom>::rasterP(
		Pixel *pixelPtr, const byte *vramPtr, int nrPixels)
{
	rasterBP4(pixelPtr, vramPtr, nrPixels);
}

// -- Blenders --------------------------------------------------------

template <class Pixel, Renderer::Zoom zoom>
inline unsigned int V9990BitmapConverter<Pixel, zoom>::red(Pixel pix) {
	return (pix & format.Rmask) >> format.Rshift;
}

template <class Pixel, Renderer::Zoom zoom>
inline unsigned int V9990BitmapConverter<Pixel, zoom>::green(Pixel pix) {
	return (pix & format.Gmask) >> format.Gshift;
}

template <class Pixel, Renderer::Zoom zoom>
inline unsigned int V9990BitmapConverter<Pixel, zoom>::blue(Pixel pix) {
	return (pix & format.Bmask) >> format.Bshift;
}

template <class Pixel, Renderer::Zoom zoom>
Pixel V9990BitmapConverter<Pixel, zoom>::blendPixels(Pixel *source,
                                                     int     nrPixels,
                                                     ...)
{
	unsigned int r = 0;
	unsigned int g = 0;
	unsigned int b = 0;
	unsigned int total = 0;
	unsigned int weight = 0;

	va_list list;
	va_start(list, nrPixels);
	
	while(nrPixels--) {
		weight =va_arg(list, unsigned int);
		r += red  (*source) * weight;
		g += green(*source) * weight;
		b += blue (*source) * weight;
		source++;
		total += weight;
	}
	va_end(list);
	r /= total;
	g /= total;
	b /= total;
	
	return (Pixel)(((r << format.Rshift) & format.Rmask) |
	               ((g << format.Gshift) & format.Gmask) |
				   ((b << format.Bshift) & format.Bmask));
}


template <class Pixel, Renderer::Zoom zoom>
void V9990BitmapConverter<Pixel, zoom>::blend_1on3(Pixel *inPixels,
                                                   Pixel *outPixels,
                                                   int    nrPixels)
{
	while(nrPixels--) {
		*outPixels++ = *inPixels;
		*outPixels++ = *inPixels;
		*outPixels++ = *inPixels++;
	}
}

template <class Pixel, Renderer::Zoom zoom>
void V9990BitmapConverter<Pixel, zoom>::blend_1on2(Pixel *inPixels,
                                                   Pixel *outPixels,
                                                   int    nrPixels)
{
	while(nrPixels--) {
		*outPixels++ = *inPixels;
		*outPixels++ = *inPixels++;
	}
}

template <class Pixel, Renderer::Zoom zoom>
void V9990BitmapConverter<Pixel, zoom>::blend_1on1(Pixel *inPixels,
                                                   Pixel *outPixels,
                                                   int    nrPixels)
{
	while(nrPixels --) {
		*outPixels++ = *inPixels++;
	}
}

template <class Pixel, Renderer::Zoom zoom>
void V9990BitmapConverter<Pixel, zoom>::blend_2on1(Pixel *inPixels,
                                                   Pixel *outPixels,
                                                   int    nrPixels)
{
	while(nrPixels > 0) {
		*outPixels++ = blendPixels(inPixels, 2, 1, 1);
		inPixels -= 2;
		nrPixels -= 2;
	}
}

template <class Pixel, Renderer::Zoom zoom>
void V9990BitmapConverter<Pixel, zoom>::blend_4on1(Pixel *inPixels,
                                                   Pixel *outPixels,
                                                   int    nrPixels)
{
	while(nrPixels > 0) {
		*outPixels++ = blendPixels(inPixels, 4, 1, 1, 1, 1);
		inPixels -= 4;
		nrPixels -= 4;
	}
}

template <class Pixel, Renderer::Zoom zoom>
void V9990BitmapConverter<Pixel, zoom>::blend_2on3(Pixel *inPixels,
                                                   Pixel *outPixels,
                                                   int    nrPixels)
{
	while(nrPixels > 0) {
		*outPixels++ = *inPixels;
		*outPixels++ = blendPixels(inPixels++, 2, 1, 1);
		*outPixels++ = *inPixels++;
		nrPixels -= 2;
	}
}

template <class Pixel, Renderer::Zoom zoom>
void V9990BitmapConverter<Pixel, zoom>::blend_4on3(Pixel *inPixels,
                                                   Pixel *outPixels,
                                                   int    nrPixels)
{
	while(nrPixels > 0) {
		*outPixels++ = blendPixels(inPixels++, 2, 3, 1); 
		*outPixels++ = blendPixels(inPixels++, 2, 1, 1);
		*outPixels++ = blendPixels(inPixels++, 2, 1, 3);
		 inPixels++;
		nrPixels -= 4;
	}
}

template <class Pixel, Renderer::Zoom zoom>
void V9990BitmapConverter<Pixel, zoom>::blend_8on3(Pixel *inPixels,
                                                   Pixel *outPixels,
                                                   int    nrPixels)
{
	while(nrPixels > 0) {
		*outPixels++ = blendPixels(inPixels, 3, 3, 3, 2);
		inPixels += 2;
		*outPixels++ = blendPixels(inPixels, 4, 1, 3, 3, 1);
		inPixels += 3;
		*outPixels++ = blendPixels(inPixels, 3, 2, 3, 3);
		inPixels += 3;
		nrPixels -= 8;
	}
}

template <class Pixel, Renderer::Zoom zoom>
void V9990BitmapConverter<Pixel, zoom>::blend_none(Pixel *inPixels,
                                                   Pixel *outPixels,
                                                   int    nrPixels)
{
	while(nrPixels--) {
		*outPixels++ = (Pixel) 0;
	}
}

template <class Pixel, Renderer::Zoom zoom>
void V9990BitmapConverter<Pixel, zoom>::setDisplayMode(V9990DisplayMode mode)
{
	// Select pixel blender
	if(zoom == Renderer::ZOOM_256) {
		switch(mode) {
			case P1:
			case B1: blendMethod = &V9990BitmapConverter::blend_1on1; break;
			case P2:
			case B3: blendMethod = &V9990BitmapConverter::blend_2on1; break;
			case B7: blendMethod = &V9990BitmapConverter::blend_4on1; break;
			case B0: blendMethod = &V9990BitmapConverter::blend_2on3; break;
			case B2: blendMethod = &V9990BitmapConverter::blend_4on3; break;
			case B4: blendMethod = &V9990BitmapConverter::blend_8on3; break;
			case B5:
			case B6: blendMethod = &V9990BitmapConverter::blend_none; break;
			default:
					 assert(false);
		}
	} else {
		switch(mode) {
			case P1:
			case B1: blendMethod = &V9990BitmapConverter::blend_1on2; break;
			case P2:
			case B3: blendMethod = &V9990BitmapConverter::blend_1on1; break;
			case B7: blendMethod = &V9990BitmapConverter::blend_2on1; break;
			case B0: blendMethod = &V9990BitmapConverter::blend_1on3; break;
			case B2: blendMethod = &V9990BitmapConverter::blend_2on3; break;
			case B4: blendMethod = &V9990BitmapConverter::blend_4on3; break;
			case B5:
			case B6: blendMethod = &V9990BitmapConverter::blend_none; break;
			default:
					 assert(false);
		}
	}
}

template <class Pixel, Renderer::Zoom zoom>
void V9990BitmapConverter<Pixel, zoom>::setColorMode(V9990ColorMode mode)
{
	switch(mode) {
		case PP:	rasterMethod = &V9990BitmapConverter::rasterP;     break;
		case BYUV:	rasterMethod = &V9990BitmapConverter::rasterBYUV;  break;
		case BYUVP:	rasterMethod = &V9990BitmapConverter::rasterBYUVP; break;
		case BYJK:	rasterMethod = &V9990BitmapConverter::rasterBYJK;  break;
		case BYJKP:	rasterMethod = &V9990BitmapConverter::rasterBYJKP; break;
		case BD16:	rasterMethod = &V9990BitmapConverter::rasterBD16;  break;
		case BD8:	rasterMethod = &V9990BitmapConverter::rasterBD8;   break;
		case BP6:	rasterMethod = &V9990BitmapConverter::rasterBP6;   break;
		case BP4:	rasterMethod = &V9990BitmapConverter::rasterBP4;   break;
		case BP2:	rasterMethod = &V9990BitmapConverter::rasterBP2;   break;
		default: assert (false);
	}
}

template <class Pixel, Renderer::Zoom zoom>
void V9990BitmapConverter<Pixel, zoom>::convertLine(Pixel *linePtr, const byte* vramPtr, int nrPixels)
{
	if(rasterMethod == NULL || blendMethod == NULL) return;

	Pixel *tmp = (Pixel *)malloc(nrPixels * sizeof(Pixel));
	if(tmp != NULL) {
		(this->*rasterMethod)(tmp, vramPtr, nrPixels);
		(this->*blendMethod)(tmp, linePtr, nrPixels);
		free(tmp);
	}
}

} // namespace openmsx

