// $Id$

#include "SDLImage.hh"
#include "OutputSurface.hh"
#include "LocalFileReference.hh"
#include "MSXException.hh"
#include "vla.hh"
#include "build-info.hh"
#if PLATFORM_GP2X
#include "GP2XMMUHack.hh"
#endif
#include <cassert>
#include <cstdlib>
#include <SDL_image.h>
#include <SDL.h>

using std::string;

namespace openmsx {

static SDL_Surface* convertToDisplayFormat(SDL_Surface* input)
{
	// scan image, are all alpha values the same?
	const char* pixels = static_cast<const char*>(input->pixels);
	unsigned width = input->w;
	unsigned height = input->h;
	unsigned pitch = input->pitch;
	unsigned Amask = input->format->Amask;
	unsigned bytes = input->format->BytesPerPixel;
	unsigned pixel = *reinterpret_cast<const unsigned*>(pixels);
	unsigned alpha = pixel & Amask;

	bool constant = true;
	for (unsigned y = 0; y < height; ++y) {
		const char* p = pixels + y * pitch;
		for (unsigned x = 0; x < width; ++x) {
			if ((*reinterpret_cast<const unsigned*>(p) & Amask) != alpha) {
				constant = false;
				break;
			}
			p += bytes;
		}
		if (!constant) break;
	}

	// convert the background to the right format
	SDL_Surface* result;
	if (constant) {
		Uint8 r, g, b, a;
		SDL_GetRGBA(pixel, input->format, &r, &g, &b, &a);
		SDL_SetAlpha(input, SDL_SRCALPHA, a);
		result = SDL_DisplayFormat(input);
	} else {
		result = SDL_DisplayFormatAlpha(input);
	}
	return result;
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
static SDL_Surface* scaleImage32(
	SDL_Surface* input, int width, int height)
{
	// create a 32 bpp surface that will hold the scaled version
	SDL_Surface* result = SDL_CreateRGBSurface(
		SDL_SWSURFACE, abs(width), abs(height), 32, rmask, gmask, bmask, amask);
	SDL_Surface* tmp32 =
		SDL_ConvertSurface(input, result->format, SDL_SWSURFACE);
	zoomSurface(tmp32, result, width < 0, height < 0);
	SDL_FreeSurface(tmp32);
	return result;
}

static SDL_Surface* loadImage(const string& filename)
{
	SDL_Surface* picture = SDLImage::readImage(filename);
	SDL_Surface* result = convertToDisplayFormat(picture);
	SDL_FreeSurface(picture);
	return result;
}

static SDL_Surface* loadImage(const string& filename, double scaleFactor)
{
	if (scaleFactor == 1.0) {
		return loadImage(filename);
	}
	SDL_Surface* picture = SDLImage::readImage(filename);
	int width  = int(picture->w * scaleFactor);
	int height = int(picture->h * scaleFactor);
	if ((width == 0) || (height == 0)) {
		SDL_FreeSurface(picture);
		return NULL;
	}
	SDL_Surface* scaled = scaleImage32(picture, width, height);
	SDL_FreeSurface(picture);

	SDL_Surface* result = convertToDisplayFormat(scaled);
	SDL_FreeSurface(scaled);
	return result;
}

static SDL_Surface* loadImage(
	const string& filename, int width, int height)
{
	if ((width == 0) || (height == 0)) {
		return NULL;
	}
	SDL_Surface* picture = SDLImage::readImage(filename);
	SDL_Surface* scaled = scaleImage32(picture, width, height);
	SDL_FreeSurface(picture);

	SDL_Surface* result = convertToDisplayFormat(scaled);
	SDL_FreeSurface(scaled);
	return result;
}


// class SDLImage

SDL_Surface* SDLImage::readImage(const string& filename)
{
	LocalFileReference file(filename);
	SDL_RWops *src = SDL_RWFromFile(file.getFilename().c_str(), "rb");
	if (!src) {
		throw MSXException("Could not open image file \"" + filename + "\"");
	}
	SDL_Surface* result = IMG_LoadPNG_RW(src);
	SDL_RWclose(src);
	if (!result) {
		throw MSXException("File \"" + filename + "\" is not a valid PNG image");
	}
	return result;
}


SDLImage::SDLImage(const string& filename)
	: workImage(NULL), a(-1), flipX(false), flipY(false)
{
	image = loadImage(filename);
}

SDLImage::SDLImage(const std::string& filename, double scaleFactor)
	: workImage(NULL), a(-1), flipX(scaleFactor < 0), flipY(scaleFactor < 0)
{
	image = loadImage(filename, scaleFactor);
}

SDLImage::SDLImage(const string& filename, int width, int height)
	: workImage(NULL), a(-1), flipX(width < 0), flipY(height < 0)
{
	image = loadImage(filename, width, height);
}

SDLImage::SDLImage(int width, int height,
                   byte alpha, byte r, byte g, byte b)
	: workImage(NULL), flipX(width < 0), flipY(height < 0)
{
	if ((width == 0) || (height == 0)) {
		image = NULL;
		return;
	}
	SDL_Surface* videoSurface = SDL_GetVideoSurface();
	assert(videoSurface);
	const SDL_PixelFormat& format = *videoSurface->format;

	image = SDL_CreateRGBSurface(SDL_SWSURFACE | SDL_SRCALPHA,
		abs(width), abs(height), format.BitsPerPixel,
		format.Rmask, format.Gmask, format.Bmask, 0);
	if (!image) {
		throw MSXException("Couldn't allocate surface.");
	}
	if (r || g || b) {
		// crashes on zero-width surfaces, that's why we
		// check for it at the beginning of this method
		SDL_FillRect(image, NULL, SDL_MapRGB(
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
	, workImage(NULL), a(-1), flipX(false), flipY(false)
{
}

SDLImage::~SDLImage()
{
	if (image)     SDL_FreeSurface(image);
	if (workImage) SDL_FreeSurface(workImage);
}

void SDLImage::allocateWorkImage()
{
	int flags = SDL_SWSURFACE;
	if (PLATFORM_GP2X) {
		flags = SDL_HWSURFACE;
	}
	const SDL_PixelFormat* format = image->format;
	workImage = SDL_CreateRGBSurface(flags,
		image->w, image->h, format->BitsPerPixel,
		format->Rmask, format->Gmask, format->Bmask, 0);
	if (!workImage) {
		throw FatalError("Couldn't allocate SDLImage workimage");
	}
#if PLATFORM_GP2X
	GP2XMMUHack::instance().patchPageTables();
#endif
}

void SDLImage::draw(OutputSurface& output, int x, int y, byte alpha)
{
	if (!image) return;
	if (flipX) x -= image->w;
	if (flipY) y -= image->h;

	output.unlock();
	SDL_Surface* outputSurface = output.getSDLWorkSurface();
	SDL_Rect rect;
	rect.x = x;
	rect.y = y;
	if (alpha == 255) {
		SDL_BlitSurface(image, NULL, outputSurface, &rect);
	} else {
		if (a == -1) {
			if (!workImage) {
				allocateWorkImage();
			}
			rect.w = image->w;
			rect.h = image->h;
			SDL_BlitSurface(outputSurface, &rect, workImage, NULL);
			SDL_BlitSurface(image,         NULL,  workImage, NULL);
			SDL_SetAlpha(workImage, SDL_SRCALPHA, alpha);
			SDL_BlitSurface(workImage, NULL, outputSurface, &rect);
		} else {
			SDL_SetAlpha(image, SDL_SRCALPHA, (a * alpha) / 256);
			SDL_BlitSurface(image, NULL, outputSurface, &rect);
		}
	}
}

int SDLImage::getWidth() const
{
	return image ? image->w : 0;
}

int SDLImage::getHeight() const
{
	return image ? image->h : 0;
}

} // namespace openmsx
