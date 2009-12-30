// $Id$

#include "SDLImage.hh"
#include "PNG.hh"
#include "OutputSurface.hh"
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

static void zoomSurface(SDL_Surface* src, SDL_Surface* dst, bool flipX, bool flipY)
{
	// For interpolation: assume source dimension is one pixel
	// smaller to avoid overflow on right and bottom edge.
	int sx = int(65536.0 * double(src->w - 1) / double(dst->w));
	int sy = int(65536.0 * double(src->h - 1) / double(dst->h));

	// Interpolating Zoom, Scan destination
	Uint8* sp = static_cast<Uint8*>(src->pixels);
	Uint8* dp = static_cast<Uint8*>(dst->pixels);
	if (flipY) dp += (dst->h - 1) * dst->pitch;
	for (int y = 0, csy = 0; y < dst->h; ++y, csy += sy) {
		sp += (csy >> 16) * src->pitch;
		Uint8* c00 = sp;
		Uint8* c10 = sp + src->pitch;
		Uint8* c01 = c00 + 4;
		Uint8* c11 = c10 + 4;
		csy &= 0xffff;
		if (!flipX) {
			// not horizontally mirrored
			for (int x = 0, csx = 0; x < dst->w; ++x, csx += sx) {
				int sstep = csx >> 16;
				c00 += 4 * sstep;
				c01 += 4 * sstep;
				c10 += 4 * sstep;
				c11 += 4 * sstep;
				csx &= 0xffff;
				// Interpolate RGBA
				for (int i = 0; i < 4; ++i) {
					int t1 = (((c01[i] - c00[i]) * csx) >> 16) + c00[i];
					int t2 = (((c11[i] - c10[i]) * csx) >> 16) + c10[i];
					dp[4 * x + i] = (((t2 - t1) * csy) >> 16) + t1;
				}
			}
		} else {
			// not horizontally mirrored
			for (int x = dst->w - 1, csx = 0; x >= 0; --x, csx += sx) {
				int sstep = csx >> 16;
				c00 += 4 * sstep;
				c01 += 4 * sstep;
				c10 += 4 * sstep;
				c11 += 4 * sstep;
				csx &= 0xffff;
				// Interpolate RGBA
				for (int i = 0; i < 4; ++i) {
					int t1 = (((c01[i] - c00[i]) * csx) >> 16) + c00[i];
					int t2 = (((c11[i] - c10[i]) * csx) >> 16) + c10[i];
					dp[4 * x + i] = (((t2 - t1) * csy) >> 16) + t1;
				}
			}
		}
		dp += flipY ? -dst->pitch : dst->pitch;
	}
}

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	static const Uint32 rmask = 0xff000000;
	static const Uint32 gmask = 0x00ff0000;
	static const Uint32 bmask = 0x0000ff00;
	static const Uint32 amask = 0x000000ff;
#else
	static const Uint32 rmask = 0x000000ff;
	static const Uint32 gmask = 0x0000ff00;
	static const Uint32 bmask = 0x00ff0000;
	static const Uint32 amask = 0xff000000;
#endif
static SDLSurfacePtr scaleImage32(
	SDL_Surface* input, int width, int height)
{
	// create a 32 bpp surface that will hold the scaled version
	SDLSurfacePtr result(SDL_CreateRGBSurface(
		SDL_SWSURFACE, abs(width), abs(height), 32, rmask, gmask, bmask, amask));
	SDLSurfacePtr tmp32(
		SDL_ConvertSurface(input, result->format, SDL_SWSURFACE));
	zoomSurface(tmp32.get(), result.get(), width < 0, height < 0);
	return result;
}

static SDLSurfacePtr loadImage(const string& filename)
{
	SDLSurfacePtr picture(PNG::load(filename));
	return convertToDisplayFormat(picture.get());
}

static SDLSurfacePtr loadImage(const string& filename, double scaleFactor)
{
	if (scaleFactor == 1.0) {
		return loadImage(filename);
	}
	SDLSurfacePtr picture(PNG::load(filename));
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
	SDLSurfacePtr picture(PNG::load(filename));
	SDLSurfacePtr scaled(scaleImage32(picture.get(), width, height));
	return convertToDisplayFormat(scaled.get());
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

SDLImage::SDLImage(int width, int height,
                   byte alpha, byte r, byte g, byte b)
	: flipX(width < 0), flipY(height < 0)
{
	checkSize(width, height);
	if ((width == 0) || (height == 0)) {
		return;
	}
	SDL_Surface* videoSurface = SDL_GetVideoSurface();
	assert(videoSurface);
	const SDL_PixelFormat& format = *videoSurface->format;

	image.reset(SDL_CreateRGBSurface(SDL_SWSURFACE | SDL_SRCALPHA,
		abs(width), abs(height), format.BitsPerPixel,
		format.Rmask, format.Gmask, format.Bmask, 0));
	if (!image.get()) {
		throw MSXException("Couldn't allocate surface.");
	}
	if (r || g || b) {
		// crashes on zero-width surfaces, that's why we
		// check for it at the beginning of this method
		SDL_FillRect(image.get(), NULL, SDL_MapRGB(
#if SDL_VERSION_ATLEAST(1, 2, 12)
			&format,
#else
			// Work around const correctness bug in SDL 1.2.11 (bug #421).
			const_cast<SDL_PixelFormat*>(&format),
#endif
			r, g, b));
	}
	a = (alpha == 255) ? 256 : alpha;
}

SDLImage::SDLImage(SDL_Surface* image_)
	: image(image_)
	, a(-1), flipX(false), flipY(false)
{
}

SDLImage::~SDLImage()
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
	if (alpha == 255) {
		SDL_BlitSurface(image.get(), NULL, outputSurface, &rect);
	} else {
		if (a == -1) {
			if (!workImage.get()) {
				allocateWorkImage();
			}
			rect.w = image->w;
			rect.h = image->h;
			SDL_BlitSurface(outputSurface, &rect, workImage.get(), NULL);
			SDL_BlitSurface(image.get(),   NULL,  workImage.get(), NULL);
			SDL_SetAlpha(workImage.get(), SDL_SRCALPHA, alpha);
			SDL_BlitSurface(workImage.get(), NULL, outputSurface, &rect);
		} else {
			SDL_SetAlpha(image.get(), SDL_SRCALPHA, (a * alpha) / 256);
			SDL_BlitSurface(image.get(), NULL, outputSurface, &rect);
		}
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
