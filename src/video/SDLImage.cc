// $Id$

#include "SDLImage.hh"
#include "PNG.hh"
#include "OutputSurface.hh"
#include "PixelOperations.hh"
#include "MSXException.hh"
#include "vla.hh"
#include "unreachable.hh"
#include "build-info.hh"
#if PLATFORM_GP2X
#include "GP2XMMUHack.hh"
#endif
#include <cassert>
#include <cstdlib>
#include <SDL.h>

using std::string;

namespace openmsx {

template<typename Pixel>
static bool hasConstantAlpha(const SDL_Surface* surface, byte& alpha)
{
	unsigned Amask = surface->format->Amask;
	const char* data = static_cast<const char*>(surface->pixels);
	Pixel pixel0 = *reinterpret_cast<const Pixel*>(data);
	if (Amask != 0) {
		// There is an alpha layer. Scan image and compare alpha from
		// each pixel. Are they all the same?
		Pixel alpha0 = pixel0 & Amask;
		for (int y = 0; y < surface->h; ++y) {
			const char* p = data + y * surface->pitch;
			for (int x = 0; x < surface->w; ++x) {
				if ((*reinterpret_cast<const Pixel*>(p) & Amask)
				    != alpha0) {
					return false;
				}
				p += surface->format->BytesPerPixel;
			}
		}
	}
	byte dummyR, dummyG, dummyB;
	SDL_GetRGBA(pixel0, surface->format, &dummyR, &dummyG, &dummyB, &alpha);
	return true;
}
static bool hasConstantAlpha(const SDL_Surface* surface, byte& alpha)
{
	switch (surface->format->BytesPerPixel) {
	case 1:
		return hasConstantAlpha<byte>(surface, alpha);
	case 2:
		return hasConstantAlpha<word>(surface, alpha);
	case 3: case 4:
		return hasConstantAlpha<unsigned>(surface, alpha);
	default:
		UNREACHABLE; return false;
	}
}

static SDLSurfacePtr convertToDisplayFormat(SDL_Surface* input)
{
	byte alpha;
	if (hasConstantAlpha(input, alpha)) {
		SDL_SetAlpha(input, SDL_SRCALPHA, alpha);
		return SDLSurfacePtr(SDL_DisplayFormat(input));
	} else {
		return SDLSurfacePtr(SDL_DisplayFormatAlpha(input));
	}
}

static void zoomSurface(const SDL_Surface* src, SDL_Surface* dst,
                        bool flipX, bool flipY)
{
	assert(src->format->BitsPerPixel == 32);
	assert(dst->format->BitsPerPixel == 32);

	PixelOperations<unsigned> pixelOps(*dst->format);

	// For interpolation: assume source dimension is one pixel
	// smaller to avoid overflow on right and bottom edge.
	int sx = int(65536.0 * double(src->w - 1) / double(dst->w));
	int sy = int(65536.0 * double(src->h - 1) / double(dst->h));

	// Interpolating Zoom, Scan destination
	const unsigned* sp = static_cast<const unsigned*>(src->pixels);
	      unsigned* dp = static_cast<      unsigned*>(dst->pixels);
	int srcPitch = src->pitch / sizeof(unsigned);
	int dstPitch = dst->pitch / sizeof(unsigned);
	if (flipY) dp += (dst->h - 1) * dstPitch;
	for (int y = 0, csy = 0; y < dst->h; ++y, csy += sy) {
		sp += (csy >> 16) * srcPitch;
		const unsigned* c00 = sp;
		const unsigned* c10 = sp + srcPitch;
		csy &= 0xffff;
		if (!flipX) {
			// not horizontally mirrored
			for (int x = 0, csx = 0; x < dst->w; ++x, csx += sx) {
				int sstep = csx >> 16;
				c00 += sstep;
				c10 += sstep;
				csx &= 0xffff;
				// Interpolate RGBA
				unsigned t1 = pixelOps.lerp(c00[0], c00[1], (csx >> 8));
				unsigned t2 = pixelOps.lerp(c10[0], c10[1], (csx >> 8));
				dp[x] = pixelOps.lerp(t1 , t2 , (csy >> 8));
			}
		} else {
			// horizontally mirrored
			for (int x = dst->w - 1, csx = 0; x >= 0; --x, csx += sx) {
				int sstep = csx >> 16;
				c00 += sstep;
				c10 += sstep;
				csx &= 0xffff;
				// Interpolate RGBA
				unsigned t1 = pixelOps.lerp(c00[0], c00[1], (csx >> 8));
				unsigned t2 = pixelOps.lerp(c10[0], c10[1], (csx >> 8));
				dp[x] = pixelOps.lerp(t1 , t2 , (csy >> 8));
			}
		}
		dp += flipY ? -dstPitch : dstPitch;
	}
}

static SDLSurfacePtr create32BppSurface(int width, int height)
{
	static const Uint32 rmask = 0xff000000;
	static const Uint32 gmask = 0x00ff0000;
	static const Uint32 bmask = 0x0000ff00;
	static const Uint32 amask = 0x000000ff;

	SDLSurfacePtr result(SDL_CreateRGBSurface(
		SDL_SWSURFACE, abs(width), abs(height), 32, rmask, gmask, bmask, amask));
	if (!result.get()) {
		throw MSXException("Couldn't allocate surface.");
	}
	return result;
}

static SDLSurfacePtr scaleImage32(
	SDL_Surface* input, int width, int height)
{
	// create a 32 bpp surface that will hold the scaled version
	SDLSurfacePtr result = create32BppSurface(width, height);
	SDLSurfacePtr tmp32(
		SDL_ConvertSurface(input, result->format, SDL_SWSURFACE));
	zoomSurface(tmp32.get(), result.get(), width < 0, height < 0);
	return result;
}

static SDLSurfacePtr loadImage(const string& filename)
{
	// If the output surface is 32bpp, then always load the PNG as
	// 32bpp (even if it has no alpha channel).
	bool want32bpp = SDL_GetVideoSurface()->format->BitsPerPixel == 32;
	SDLSurfacePtr picture(PNG::load(filename, want32bpp));
	return convertToDisplayFormat(picture.get());
}

static SDLSurfacePtr loadImage(const string& filename, double scaleFactor)
{
	if (scaleFactor == 1.0) {
		return loadImage(filename);
	}
	bool want32bpp = true; // scaleImage32 needs 32bpp
	SDLSurfacePtr picture(PNG::load(filename, want32bpp));
	int width  = int(picture->w * scaleFactor);
	int height = int(picture->h * scaleFactor);
	BaseImage::checkSize(width, height);
	if ((width == 0) || (height == 0)) {
		return SDLSurfacePtr();
	}
	SDLSurfacePtr scaled(scaleImage32(picture.get(), width, height));
	return convertToDisplayFormat(scaled.get());
}

static SDLSurfacePtr loadImage(
	const string& filename, int width, int height)
{
	BaseImage::checkSize(width, height);
	if ((width == 0) || (height == 0)) {
		return SDLSurfacePtr();
	}
	bool want32bpp = true; // scaleImage32 needs 32bpp
	SDLSurfacePtr picture(PNG::load(filename, want32bpp));
	SDLSurfacePtr scaled(scaleImage32(picture.get(), width, height));
	return convertToDisplayFormat(scaled.get());
}


// 0 -- 1
// |    |
// 2 -- 3
static void unpackRGBA(unsigned rgba,
                       unsigned& r, unsigned&g, unsigned&b, unsigned& a)
{
	r = (((rgba >> 24) & 0xFF) << 16) + 0x8000;
	g = (((rgba >> 16) & 0xFF) << 16) + 0x8000;
	b = (((rgba >>  8) & 0xFF) << 16) + 0x8000;
	a = (((rgba >>  0) & 0xFF) << 16) + 0x8000;
}
static unsigned packRGBA(unsigned r, unsigned g, unsigned b, unsigned a)
{
	r >>= 16; g >>= 16; b >>= 16; a >>= 16;
	return (r << 24) | (g << 16) | (b << 8) | (a << 0);
}
static void setupInterp(unsigned r0, unsigned g0, unsigned b0, unsigned a0,
                        unsigned r1, unsigned g1, unsigned b1, unsigned a1,
                        unsigned length,
                        int& dr, int& dg, int& db, int& da)
{
	if (length == 1) {
		dr = dg = db = da = 0;
	} else {
		dr = int(r1 - r0) / int(length - 1);
		dg = int(g1 - g0) / int(length - 1);
		db = int(b1 - b0) / int(length - 1);
		da = int(a1 - a0) / int(length - 1);
	}
}
static void gradient(const unsigned* rgba, SDL_Surface& surface, unsigned borderSize)
{
	int width  = surface.w - 2 * borderSize;
	int height = surface.h - 2 * borderSize;
	if ((width <= 0) || (height <= 0)) return;

	unsigned r0, g0, b0, a0;
	unsigned r1, g1, b1, a1;
	unsigned r2, g2, b2, a2;
	unsigned r3, g3, b3, a3;
	unpackRGBA(rgba[0], r0, g0, b0, a0);
	unpackRGBA(rgba[1], r1, g1, b1, a1);
	unpackRGBA(rgba[2], r2, g2, b2, a2);
	unpackRGBA(rgba[3], r3, g3, b3, a3);

	int dr02, dg02, db02, da02;
	int dr13, dg13, db13, da13;
	setupInterp(r0, g0, b0, a0, r2, g2, b2, a2, height, dr02, dg02, db02, da02);
	setupInterp(r1, g1, b1, a1, r3, g3, b3, a3, height, dr13, dg13, db13, da13);

	unsigned* buffer = static_cast<unsigned*>(surface.pixels);
	buffer += borderSize;
	buffer += borderSize * (surface.pitch / sizeof(unsigned));
	for (int y = 0; y < height; ++y) {
		int dr, dg, db, da;
		setupInterp(r0, g0, b0, a0, r1, g1, b1, a1, width, dr, dg, db, da);

		unsigned r = r0, g = g0, b = b0, a = a0;
		for (int x = 0; x < width; ++x) {
			buffer[x] = packRGBA(r, g, b, a);
			r += dr; g += dg; b += db; a += da;
		}

		r0 += dr02; g0 += dg02; b0 += db02; a0 += da02;
		r1 += dr13; g1 += dg13; b1 += db13; a1 += da13;
		buffer += (surface.pitch / sizeof(unsigned));
	}
}

// class SDLImage

SDLImage::SDLImage(const string& filename)
	: image(loadImage(filename))
	, a(-1), flipX(false), flipY(false)
{
}

SDLImage::SDLImage(const std::string& filename, double scaleFactor)
	: image(loadImage(filename, scaleFactor))
	, a(-1), flipX(scaleFactor < 0), flipY(scaleFactor < 0)
{
}

SDLImage::SDLImage(const string& filename, int width, int height)
	: image(loadImage(filename, width, height))
	, a(-1), flipX(width < 0), flipY(height < 0)
{
}

SDLImage::SDLImage(int width, int height, unsigned rgba)
	: flipX(width < 0), flipY(height < 0)
{
	initSolid(width, height, rgba, 0, 0); // no border
}


SDLImage::SDLImage(int width, int height, const unsigned* rgba,
                   unsigned borderSize, unsigned borderRGBA)
	: flipX(width < 0), flipY(height < 0)
{
	if ((rgba[0] == rgba[1]) &&
	    (rgba[0] == rgba[2]) &&
	    (rgba[0] == rgba[3])) {
		initSolid   (width, height, rgba[0], borderSize, borderRGBA);
	} else {
		initGradient(width, height, rgba,    borderSize, borderRGBA);
	}
}

static unsigned convertColor(const SDL_PixelFormat& format, unsigned rgba)
{
	return SDL_MapRGBA(
#if SDL_VERSION_ATLEAST(1, 2, 12)
		&format,
#else
		// Work around const correctness bug in SDL 1.2.11 (bug #421).
		const_cast<SDL_PixelFormat*>(&format),
#endif
		(rgba >> 24) & 0xff,
		(rgba >> 16) & 0xff,
		(rgba >>  8) & 0xff,
		(rgba >>  0) & 0xff);
}

static void drawBorder(SDL_Surface& image, int size, unsigned rgba)
{
	if (size <= 0) return;

	unsigned color = convertColor(*image.format, rgba);
	bool onlyBorder = ((2 * size) >= image.w) ||
	                  ((2 * size) >= image.h);
	if (onlyBorder) {
		SDL_FillRect(&image, NULL, color);
	} else {
		// +--------------------+
		// |          1         |
		// +---+------------+---+
		// |   |            |   |
		// | 3 |            | 4 |
		// |   |            |   |
		// +---+------------+---+
		// |          2         |
		// +--------------------+
		SDL_Rect rect;
		rect.x = 0;
		rect.y = 0;
		rect.w = image.w;
		rect.h = size;
		SDL_FillRect(&image, &rect, color); // 1

		rect.y = image.h - size;
		SDL_FillRect(&image, &rect, color); // 2

		rect.y = size;
		rect.w = size;
		rect.h = image.h - 2 * size;
		SDL_FillRect(&image, &rect, color); // 3

		rect.x = image.w - size;
		SDL_FillRect(&image, &rect, color); // 4
	}
}

void SDLImage::initSolid(int width, int height, unsigned rgba,
                         unsigned borderSize, unsigned borderRGBA)
{
	checkSize(width, height);
	if ((width == 0) || (height == 0)) {
		// SDL_FillRect crashes on zero-width surfaces, so check for it
		return;
	}

	unsigned bgAlpha     = rgba       & 0xff;
	unsigned borderAlpha = borderRGBA & 0xff;
	if (bgAlpha == borderAlpha) {
		a = (bgAlpha == 255) ? 256 : bgAlpha;
	} else {
		a = -1;
	}

	// create surface of correct size without alpha channel
	SDL_Surface* videoSurface = SDL_GetVideoSurface();
	assert(videoSurface);
	const SDL_PixelFormat& format = *videoSurface->format;
	image.reset(SDL_CreateRGBSurface(SDL_SWSURFACE | SDL_SRCALPHA,
		abs(width), abs(height), format.BitsPerPixel,
		format.Rmask, format.Gmask, format.Bmask, 0));
	if (!image.get()) {
		throw MSXException("Couldn't allocate surface.");
	}

	if (a == -1) {
		// we need an alpha channel
		// TODO is it possible to immediately construct the surface
		//      with an alpha channel?
		// TODO What about alpha channel in 16bpp?
		SDLSurfacePtr tmp(image.release());
		image.reset(SDL_DisplayFormatAlpha(tmp.get()));
	}

	// draw interior
	SDL_FillRect(image.get(), NULL, convertColor(*image->format, rgba));

	drawBorder(*image, borderSize, borderRGBA);
}

void SDLImage::initGradient(int width, int height, const unsigned* rgba_,
                            unsigned borderSize, unsigned borderRGBA)
{
	checkSize(width, height);
	if ((width == 0) || (height == 0)) {
		return;
	}

	unsigned rgba[4];
	for (unsigned i = 0; i < 4; ++i) {
		rgba[i] = rgba_[i];
	}

	if (((rgba[0] & 0xff) == (rgba[1] & 0xff)) &&
	    ((rgba[0] & 0xff) == (rgba[2] & 0xff)) &&
	    ((rgba[0] & 0xff) == (rgba[3] & 0xff)) &&
	    ((rgba[0] & 0xff) == (borderRGBA & 0xff))) {
		a = rgba[0] & 0xff;
	} else {
		a = -1;
	}

	if (flipX) {
		std::swap(rgba[0], rgba[1]);
		std::swap(rgba[2], rgba[3]);
	}
	if (flipY) {
		std::swap(rgba[0], rgba[2]);
		std::swap(rgba[1], rgba[3]);
	}

	SDLSurfacePtr tmp32 = create32BppSurface(width, height);
	gradient(rgba, *tmp32, borderSize);
	drawBorder(*tmp32, borderSize, borderRGBA);

	if (a == -1) {
		image.reset(SDL_DisplayFormatAlpha(tmp32.get()));
	} else {
		image.reset(SDL_DisplayFormat     (tmp32.get()));
	}
}

SDLImage::SDLImage(SDLSurfacePtr image_)
	: image(image_)
	, a(-1), flipX(false), flipY(false)
{
}

void SDLImage::allocateWorkImage()
{
	int flags = SDL_SWSURFACE;
	if (PLATFORM_GP2X) {
		flags = SDL_HWSURFACE;
	}
	const SDL_PixelFormat* format = image->format;
	workImage.reset(SDL_CreateRGBSurface(flags,
		image->w, image->h, format->BitsPerPixel,
		format->Rmask, format->Gmask, format->Bmask, 0));
	if (!workImage.get()) {
		throw FatalError("Couldn't allocate SDLImage workimage");
	}
#if PLATFORM_GP2X
	GP2XMMUHack::instance().patchPageTables();
#endif
}

void SDLImage::draw(OutputSurface& output, int x, int y, byte alpha)
{
	if (!image.get()) return;
	if (flipX) x -= image->w;
	if (flipY) y -= image->h;

	output.unlock();
	SDL_Surface* outputSurface = output.getSDLWorkSurface();
	SDL_Rect rect;
	rect.x = x;
	rect.y = y;
	if (a == -1) {
		if (alpha == 255) {
			SDL_BlitSurface(image.get(), NULL, outputSurface, &rect);
		} else {
			if (!workImage.get()) {
				allocateWorkImage();
			}
			rect.w = image->w;
			rect.h = image->h;
			SDL_BlitSurface(outputSurface, &rect, workImage.get(), NULL);
			SDL_BlitSurface(image.get(),   NULL,  workImage.get(), NULL);
			SDL_SetAlpha(workImage.get(), SDL_SRCALPHA, alpha);
			SDL_BlitSurface(workImage.get(), NULL, outputSurface, &rect);
		}
	} else {
		SDL_SetAlpha(image.get(), SDL_SRCALPHA, (a * alpha) / 256);
		SDL_BlitSurface(image.get(), NULL, outputSurface, &rect);
	}
}

int SDLImage::getWidth() const
{
	return image.get() ? image->w : 0;
}

int SDLImage::getHeight() const
{
	return image.get() ? image->h : 0;
}

} // namespace openmsx
