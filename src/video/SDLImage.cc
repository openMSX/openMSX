// $Id$

#include "SDLImage.hh"
#include "File.hh"
#include "CliCommOutput.hh"
#include <SDL_image.h>
#include <SDL.h>

namespace openmsx {

SDLImage::SDLImage(SDL_Surface* output, const string& filename)
	: outputScreen(output)
{
	image = loadImage(filename);
}

SDLImage::~SDLImage()
{
	if (image) SDL_FreeSurface(image);
}

void SDLImage::draw(unsigned x, unsigned y, unsigned char alpha)
{
	if (!image) return;

	SDL_Rect destRect;
	destRect.x = x;
	destRect.y = y;
	SDL_SetAlpha(image, SDL_SRCALPHA, alpha);
	SDL_BlitSurface(image, NULL, outputScreen, &destRect);
}


SDL_Surface* SDLImage::loadImage(
	const string& filename,
	unsigned char defaultAlpha)
{
	SDL_Surface* picture = readImage(filename);
	if (picture == NULL) {
		return NULL;
	}

	SDL_Surface* result = convertToDisplayFormat(picture, defaultAlpha);
	SDL_FreeSurface(picture);
	return result;
}

SDL_Surface* SDLImage::loadImage(
	const string& filename,
	unsigned width, unsigned height,
	unsigned char defaultAlpha)
{
	SDL_Surface* picture = readImage(filename);
	if (picture == NULL) {
		return NULL;
	}

	SDL_Surface* scaled = scaleImage32(picture, width, height);
	SDL_FreeSurface(picture);

	SDL_Surface* result = convertToDisplayFormat(scaled, defaultAlpha);
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
	// If file does exist, but cannot be read as an image,
	// IMG_Load returns NULL.
	try {
		File file(filename);
		SDL_Surface* result = IMG_Load(file.getLocalName().c_str());
		if (result == NULL) {
			CliCommOutput::instance().printWarning("File \"" +
			        file.getURL() + "\" is not a valid image");
		}
		return result;
	} catch (FileException& e) {
		CliCommOutput::instance().printWarning("Could not open file \"" +
		        filename + "\": " + e.getMessage());
		return NULL;
	}
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

SDL_Surface* SDLImage::convertToDisplayFormat(
	SDL_Surface* input, Uint8 defaultAlpha)
{
	// scan image, are all alpha values the same?
	const char* pixels = (char*)input->pixels;
	unsigned width = input->w;
	unsigned height = input->h;
	unsigned pitch = input->pitch;
	unsigned Amask = input->format->Amask;
	unsigned bytes = input->format->BytesPerPixel;
	unsigned alpha = *((Uint32*)pixels) & Amask;

	bool constant = true;
	for (unsigned y = 0; y < height; ++y) {
		const char* p = pixels + y * pitch;
		for (unsigned x = 0; x < width; ++x) {
			if ((*((unsigned*)p) & Amask) != alpha) {
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
		SDL_SetAlpha(input, SDL_SRCALPHA, defaultAlpha);
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

int SDLImage::zoomSurface(SDL_Surface* src, SDL_Surface* dst, bool smooth)
{
	int x, y, sx, sy, *sax, *say, *csax, *csay, csx, csy, ex, ey, t1, t2, sstep;
	ColorRGBA *c00, *c01, *c10, *c11;
	ColorRGBA *sp, *csp, *dp;
	int dgap;

	// Variable setup
	if (smooth) {
		// For interpolation: assume source dimension is one pixel
		// smaller to avoid overflow on right and bottom edge.
		sx = (int) (65536.0 * (double)(src->w - 1) / (double)dst->w);
		sy = (int) (65536.0 * (double)(src->h - 1) / (double)dst->h);
	} else {
		sx = (int) (65536.0 * (double)src->w / (double)dst->w);
		sy = (int) (65536.0 * (double)src->h / (double)dst->h);
	}

	// Allocate memory for row increments
	if ((sax = (int *) malloc((dst->w + 1) * sizeof(Uint32))) == NULL) {
		return -1;
	}
	if ((say = (int *) malloc((dst->h + 1) * sizeof(Uint32))) == NULL) {
		free(sax);
		return -1;
	}

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
	sp = csp = (ColorRGBA *) src->pixels;
	dp = (ColorRGBA *) dst->pixels;
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
			c10 = (ColorRGBA *) ((Uint8 *) csp + src->pitch);
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
			csp = (ColorRGBA *) ((Uint8 *) csp + (*csay >> 16) * src->pitch);
			// Advance destination pointers
			dp = (ColorRGBA *) ((Uint8 *) dp + dgap);
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
			csp = (ColorRGBA *) ((Uint8 *) csp + (*csay >> 16) * src->pitch);
			// Advance destination pointers
			dp = (ColorRGBA *) ((Uint8 *) dp + dgap);
		}
	}

	// Remove temp arrays
	free(sax);
	free(say);

	return 0;
}

} // namespace openmsx
