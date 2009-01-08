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
#include <SDL_image.h>
#include <SDL.h>
#include <cassert>

using std::string;

namespace openmsx {

SDLImage::SDLImage(OutputSurface& output_, const string& filename)
	: output(output_)
{
	image = loadImage(filename);
	init(filename);
}

SDLImage::SDLImage(OutputSurface& output_, const std::string& filename,
                   double scaleFactor)
	: output(output_)
{
	image = loadImage(filename, scaleFactor);
	init(filename);
}

SDLImage::SDLImage(OutputSurface& output_, const string& filename,
	           unsigned width, unsigned height)
	: output(output_)
{
	image = loadImage(filename, width, height);
	init(filename);
}

SDLImage::SDLImage(OutputSurface& output_, unsigned width, unsigned height,
                   byte alpha, byte r, byte g, byte b)
	: output(output_)
{
	const SDL_PixelFormat& format = output.getSDLFormat();
	image = SDL_CreateRGBSurface(SDL_SWSURFACE | SDL_SRCALPHA,
		width, height, format.BitsPerPixel,
		format.Rmask, format.Gmask, format.Bmask, 0);
	if (!image) {
		throw MSXException("Couldn't allocate surface.");
	}
	if (r || g || b) {
		SDL_FillRect(image, NULL,
			     SDL_MapRGB(&output.getSDLFormat(), r, g, b));
	}
	a = (alpha == 255) ? 256 : alpha;
	workImage = NULL;
}

SDLImage::SDLImage(OutputSurface& output_, SDL_Surface* image_)
	: output(output_)
	, image(image_)
{
	init("");
}

void SDLImage::init(const string& filename)
{
	assert(image);
	const SDL_PixelFormat* format = image->format;
	int flags = SDL_SWSURFACE;
	if (PLATFORM_GP2X) {
		flags = SDL_HWSURFACE;
	}
	workImage = SDL_CreateRGBSurface(flags,
		image->w, image->h, format->BitsPerPixel,
		format->Rmask, format->Gmask, format->Bmask, 0);
	if (!workImage) {
		SDL_FreeSurface(image);
		throw MSXException("Error loading image " + filename);
	}
#if PLATFORM_GP2X
	GP2XMMUHack::instance().patchPageTables();
#endif
}

SDLImage::~SDLImage()
{
	SDL_FreeSurface(image);
	if (workImage) SDL_FreeSurface(workImage);
}

void SDLImage::draw(unsigned x, unsigned y, byte alpha)
{
	output.unlock();
	SDL_Surface* outputSurface = output.getSDLWorkSurface();
	SDL_Rect rect;
	rect.x = x;
	rect.y = y;
	if (alpha == 255) {
		SDL_BlitSurface(image, NULL, outputSurface, &rect);
	} else {
		if (workImage) {
			rect.w = image->w;
			rect.h = image->h;
			SDL_BlitSurface(outputSurface, &rect, workImage, NULL);
			SDL_BlitSurface(image,         NULL,  workImage, NULL);
			SDL_SetAlpha(workImage, SDL_SRCALPHA, alpha);
			SDL_BlitSurface(workImage,    NULL,  outputSurface, &rect);
		} else {
			SDL_SetAlpha(image, SDL_SRCALPHA, (a * alpha) / 256);
			SDL_BlitSurface(image, NULL, outputSurface, &rect);
		}
	}
}

unsigned SDLImage::getWidth() const
{
	return image->w;
}

unsigned SDLImage::getHeight() const
{
	return image->h;
}



SDL_Surface* SDLImage::loadImage(const string& filename)
{
	SDL_Surface* picture = readImage(filename);
	SDL_Surface* result = convertToDisplayFormat(picture);
	SDL_FreeSurface(picture);
	return result;
}

SDL_Surface* SDLImage::loadImage(const string& filename, double scaleFactor)
{
	if (scaleFactor == 1.0) {
		return loadImage(filename);
	}
	SDL_Surface* picture = readImage(filename);
	SDL_Surface* scaled = scaleImage32(picture,
		unsigned(picture->w * scaleFactor),
		unsigned(picture->h * scaleFactor));
	SDL_FreeSurface(picture);

	SDL_Surface* result = convertToDisplayFormat(scaled);
	SDL_FreeSurface(scaled);
	return result;
}

SDL_Surface* SDLImage::loadImage(const string& filename,
                                 unsigned width, unsigned height)
{
	SDL_Surface* picture = readImage(filename);
	SDL_Surface* scaled = scaleImage32(picture, width, height);
	SDL_FreeSurface(picture);

	SDL_Surface* result = convertToDisplayFormat(scaled);
	SDL_FreeSurface(scaled);
	return result;
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

SDL_Surface* SDLImage::readImage(const string& filename)
{
	LocalFileReference file(filename);
	SDL_Surface* result = IMG_Load(file.getFilename().c_str());
	if (!result) {
		throw MSXException("File \"" + filename + "\" is not a valid image");
	}
	return result;
}

SDL_Surface* SDLImage::scaleImage32(
	SDL_Surface* input, unsigned width, unsigned height)
{
	// create a 32 bpp surface that will hold the scaled version
	SDL_Surface* result = SDL_CreateRGBSurface(
		SDL_SWSURFACE, width, height, 32, rmask, gmask, bmask, amask);
	SDL_Surface* tmp32 =
		SDL_ConvertSurface(input, result->format, SDL_SWSURFACE);
	zoomSurface(tmp32, result, true);
	SDL_FreeSurface(tmp32);
	return result;
}

SDL_Surface* SDLImage::convertToDisplayFormat(SDL_Surface* input)
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


// TODO: Replacing this by a 4-entry array would simplify the code.
struct ColorRGBA {
	Uint8 r;
	Uint8 g;
	Uint8 b;
	Uint8 a;
};

void SDLImage::zoomSurface(SDL_Surface* src, SDL_Surface* dst, bool smooth)
{
	int x, y, sx, sy, *csax, *csay, csx, csy, ex, ey, t1, t2, sstep;
	ColorRGBA *c00, *c01, *c10, *c11;
	ColorRGBA *sp, *csp, *dp;
	int dgap;

	// Variable setup
	if (smooth) {
		// For interpolation: assume source dimension is one pixel
		// smaller to avoid overflow on right and bottom edge.
		sx = int(65536.0 * double(src->w - 1) / double(dst->w));
		sy = int(65536.0 * double(src->h - 1) / double(dst->h));
	} else {
		sx = int(65536.0 * double(src->w) / double(dst->w));
		sy = int(65536.0 * double(src->h) / double(dst->h));
	}

	// Allocate memory for row increments
	VLA(int, sax, dst->w + 1);
	VLA(int, say, dst->h + 1);

	// Precalculate row increments
	csx = 0;
	csax = sax;
	for (x = 0; x <= dst->w; x++) {
		*csax = csx;
		csax++;
		csx &= 0xffff;
		csx += sx;
	}
	csy = 0;
	csay = say;
	for (y = 0; y <= dst->h; y++) {
		*csay = csy;
		csay++;
		csy &= 0xffff;
		csy += sy;
	}

	// Pointer setup
	sp = csp = static_cast<ColorRGBA*>(src->pixels);
	dp =       static_cast<ColorRGBA*>(dst->pixels);
	dgap = dst->pitch - dst->w * 4;

	// Switch between interpolating and non-interpolating code
	if (smooth) {
		// Interpolating Zoom
		// Scan destination
		csay = say;
		for (y = 0; y < dst->h; y++) {
			// Setup color source pointers
			c00 = csp;
			c01 = csp;
			c01++;
			c10 = reinterpret_cast<ColorRGBA*>(reinterpret_cast<Uint8*>(csp) + src->pitch);
			c11 = c10;
			c11++;
			csax = sax;
			for (x = 0; x < dst->w; x++) {
				// Interpolate colors
				ex = (*csax & 0xffff);
				ey = (*csay & 0xffff);
				t1 = ((((c01->r - c00->r) * ex) >> 16) + c00->r) & 0xff;
				t2 = ((((c11->r - c10->r) * ex) >> 16) + c10->r) & 0xff;
				dp->r = (((t2 - t1) * ey) >> 16) + t1;
				t1 = ((((c01->g - c00->g) * ex) >> 16) + c00->g) & 0xff;
				t2 = ((((c11->g - c10->g) * ex) >> 16) + c10->g) & 0xff;
				dp->g = (((t2 - t1) * ey) >> 16) + t1;
				t1 = ((((c01->b - c00->b) * ex) >> 16) + c00->b) & 0xff;
				t2 = ((((c11->b - c10->b) * ex) >> 16) + c10->b) & 0xff;
				dp->b = (((t2 - t1) * ey) >> 16) + t1;
				t1 = ((((c01->a - c00->a) * ex) >> 16) + c00->a) & 0xff;
				t2 = ((((c11->a - c10->a) * ex) >> 16) + c10->a) & 0xff;
				dp->a = (((t2 - t1) * ey) >> 16) + t1;

				// Advance source pointers
				csax++;
				sstep = (*csax >> 16);
				c00 += sstep;
				c01 += sstep;
				c10 += sstep;
				c11 += sstep;
				// Advance destination pointer
				dp++;
			}
			// Advance source pointer
			csay++;
			csp = reinterpret_cast<ColorRGBA*>(reinterpret_cast<Uint8*>(csp) + (*csay >> 16) * src->pitch);
			// Advance destination pointers
			dp = reinterpret_cast<ColorRGBA*>(reinterpret_cast<Uint8*>(dp) + dgap);
		}
	} else {
		// Non-Interpolating Zoom
		csay = say;
		for (y = 0; y < dst->h; y++) {
			sp = csp;
			csax = sax;
			for (x = 0; x < dst->w; x++) {
				// Draw
				*dp = *sp;
				// Advance source pointers
				csax++;
				sp += (*csax >> 16);
				// Advance destination pointer
				dp++;
			}
			// Advance source pointer
			csay++;
			csp = reinterpret_cast<ColorRGBA*>(reinterpret_cast<Uint8*>(csp) + (*csay >> 16) * src->pitch);
			// Advance destination pointers
			dp = reinterpret_cast<ColorRGBA*>(reinterpret_cast<Uint8*>(dp) + dgap);
		}
	}
}

} // namespace openmsx
