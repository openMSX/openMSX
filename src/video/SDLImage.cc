#include "SDLImage.hh"
#include "PNG.hh"
#include "OutputSurface.hh"
#include "PixelOperations.hh"
#include "MSXException.hh"
#include <cassert>
#include <cstdlib>
#include <SDL.h>

using std::string;
using namespace gl;

namespace openmsx {

static bool hasConstantAlpha(const SDL_Surface& surface, byte& alpha)
{
	unsigned amask = surface.format->Amask;
	if (amask == 0) {
		// If there's no alpha layer, the surface has a constant
		// opaque alpha value.
		alpha = SDL_ALPHA_OPAQUE;
		return true;
	}

	// There is an alpha layer, surface must be 32bpp.
	assert(surface.format->BitsPerPixel == 32);
	assert(surface.format->Aloss == 0);

	// Compare alpha from each pixel. Are they all the same?
	auto* data = reinterpret_cast<const unsigned*>(surface.pixels);
	unsigned alpha0 = data[0] & amask;
	for (int y = 0; y < surface.h; ++y) {
		auto* p = data + y * (surface.pitch / sizeof(unsigned));
		for (int x = 0; x < surface.w; ++x) {
			if ((p[x] & amask) != alpha0) return false;
		}
	}

	// The alpha value of each pixel is constant, get that value.
	alpha = alpha0 >> surface.format->Ashift;
	return true;
}

static SDLSurfacePtr convertToDisplayFormat(SDLSurfacePtr input)
{
	auto& inFormat  = *input->format;
	auto& outFormat = *SDL_GetVideoSurface()->format;
	assert((inFormat.BitsPerPixel == 24) || (inFormat.BitsPerPixel == 32));

	byte alpha;
	if (hasConstantAlpha(*input, alpha)) {
		Uint32 flags = (alpha == SDL_ALPHA_OPAQUE) ? 0 : SDL_SRCALPHA;
		SDL_SetAlpha(input.get(), flags, alpha);
		if ((inFormat.BitsPerPixel == outFormat.BitsPerPixel) &&
		    (inFormat.Rmask == outFormat.Rmask) &&
		    (inFormat.Gmask == outFormat.Gmask) &&
		    (inFormat.Bmask == outFormat.Bmask)) {
			// Already in the correct format.
			return input;
		}
		// 32bpp should rarely need this conversion (only for exotic
		// pixel formats, not one of RGBA BGRA ARGB ABGR).
		return SDLSurfacePtr(SDL_DisplayFormat(input.get()));
	} else {
		assert(inFormat.Amask != 0);
		assert(inFormat.BitsPerPixel == 32);
		if (outFormat.BitsPerPixel != 32) {
			// We need an alpha channel, so leave the image in 32bpp format.
			return input;
		}
		if ((inFormat.Rmask == outFormat.Rmask) &&
		    (inFormat.Gmask == outFormat.Gmask) &&
		    (inFormat.Bmask == outFormat.Bmask)) {
			// Both input and output are 32bpp and both have already
			// the same pixel format (should almost always be the
			// case for 32bpp output)
			return input;
		}
		// An exotic 32bpp pixel format (not one of RGBA, BGRA, ARGB,
		// ABGR). Convert to display format with alpha channel.
		return SDLSurfacePtr(SDL_DisplayFormatAlpha(input.get()));
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
	int sx = int(65536.0f * float(src->w - 1) / float(dst->w));
	int sy = int(65536.0f * float(src->h - 1) / float(dst->h));

	// Interpolating Zoom, Scan destination
	auto sp = static_cast<const unsigned*>(src->pixels);
	auto dp = static_cast<      unsigned*>(dst->pixels);
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

static void getRGBAmasks32(Uint32& rmask, Uint32& gmask, Uint32& bmask, Uint32& amask)
{
	auto& format = *SDL_GetVideoSurface()->format;
	if ((format.BitsPerPixel == 32) && (format.Rloss == 0) &&
	    (format.Gloss == 0) && (format.Bloss == 0)) {
		rmask = format.Rmask;
		gmask = format.Gmask;
		bmask = format.Bmask;
		// on a display surface Amask is often 0, so instead
		// we use the bits that are not yet used for RGB
		//amask = format.Amask;
		amask = ~(rmask | gmask | bmask);
		assert((amask == 0x000000ff) || (amask == 0x0000ff00) ||
		       (amask == 0x00ff0000) || (amask == 0xff000000));
	} else {
		// ARGB8888 (this seems to be the 'default' format in SDL)
		amask = 0xff000000;
		rmask = 0x00ff0000;
		gmask = 0x0000ff00;
		bmask = 0x000000ff;
	}
}

static SDLSurfacePtr scaleImage32(SDLSurfacePtr input, ivec2 size)
{
	// create a 32 bpp surface that will hold the scaled version
	auto& format = *input->format;
	assert(format.BitsPerPixel == 32);
	SDLSurfacePtr result(abs(size[0]), abs(size[1]), 32,
		format.Rmask, format.Gmask, format.Bmask, format.Amask);
	zoomSurface(input.get(), result.get(), size[0] < 0, size[1] < 0);
	return result;
}

static SDLSurfacePtr loadImage(const string& filename)
{
	// If the output surface is 32bpp, then always load the PNG as
	// 32bpp (even if it has no alpha channel).
	bool want32bpp = SDL_GetVideoSurface()->format->BitsPerPixel == 32;
	return convertToDisplayFormat(PNG::load(filename, want32bpp));
}

static SDLSurfacePtr loadImage(const string& filename, float scaleFactor)
{
	if (scaleFactor == 1.0f) {
		return loadImage(filename);
	}
	bool want32bpp = true; // scaleImage32 needs 32bpp
	SDLSurfacePtr picture(PNG::load(filename, want32bpp));
	ivec2 size = trunc(vec2(picture->w, picture->h) * scaleFactor);
	BaseImage::checkSize(size);
	if ((size[0] == 0) || (size[1] == 0)) {
		return SDLSurfacePtr();
	}
	return convertToDisplayFormat(
		scaleImage32(std::move(picture), size));
}

static SDLSurfacePtr loadImage(
	const string& filename, ivec2 size)
{
	BaseImage::checkSize(size);
	if ((size[0] == 0) || (size[1] == 0)) {
		return SDLSurfacePtr();
	}
	bool want32bpp = true; // scaleImage32 needs 32bpp
	return convertToDisplayFormat(
		scaleImage32(PNG::load(filename, want32bpp), size));
}

// Helper functions to draw a gradient
//  Extract R,G,B,A components to 8.16 bit fixed point.
//  Note the order R,G,B,A is arbitrary, the actual pixel value may have the
//  components in a different order.
static void unpackRGBA(unsigned rgba,
                       unsigned& r, unsigned&g, unsigned&b, unsigned& a)
{
	r = (((rgba >> 24) & 0xFF) << 16) + 0x8000;
	g = (((rgba >> 16) & 0xFF) << 16) + 0x8000;
	b = (((rgba >>  8) & 0xFF) << 16) + 0x8000;
	a = (((rgba >>  0) & 0xFF) << 16) + 0x8000;
}
// Setup outer loop (vertical) interpolation parameters.
//  For each component there is a pair of (initial,delta) values. These values
//  are 8.16 bit fixed point, delta is signed.
static void setupInterp1(unsigned rgba0, unsigned rgba1, unsigned length,
                         unsigned& r0, unsigned& g0, unsigned& b0, unsigned& a0,
                         int& dr, int& dg, int& db, int& da)
{
	unpackRGBA(rgba0, r0, g0, b0, a0);
	if (length == 1) {
		dr = dg = db = da = 0;
	} else {
		unsigned r1, g1, b1, a1;
		unpackRGBA(rgba1, r1, g1, b1, a1);
		dr = int(r1 - r0) / int(length - 1);
		dg = int(g1 - g0) / int(length - 1);
		db = int(b1 - b0) / int(length - 1);
		da = int(a1 - a0) / int(length - 1);
	}
}
// Setup inner loop (horizontal) interpolation parameters.
// - Like above we also output a pair of (initial,delta) values for each
//   component. But we pack two components in one 32-bit value. This leaves only
//   16 bits per component, so now the values are 8.8 bit fixed point.
// - To avoid carry/borrow from the lower to the upper pack, we make the lower
//   component always a positive number and output a boolean to indicate whether
//   we should add or subtract the delta from the initial value.
// - The 8.8 fixed point calculations in the inner loop are less accurate than
//   the 8.16 calculations in the outer loop. This could result in not 100%
//   accurate gradients. Though only on very wide images and the error is
//   so small that it will hardly be visible (if at all).
// - Packing 2 components in one value is not beneficial in the outer loop
//   because in this routine we need the individual components of the values
//   that are calculated by setupInterp1(). (It would also make the code even
//   more complex).
static void setupInterp2(unsigned r0, unsigned g0, unsigned b0, unsigned a0,
                         unsigned r1, unsigned g1, unsigned b1, unsigned a1,
                         unsigned length,
                         unsigned&  rb, unsigned&  ga,
                         unsigned& drb, unsigned& dga,
                         bool&   subRB, bool&   subGA)
{
	// Pack the initial values for the components R,B and G,A into
	// a vector-type: two 8.16 scalars -> one [8.8 ; 8.8] vector
	rb = ((r0 << 8) & 0xffff0000) |
	     ((b0 >> 8) & 0x0000ffff);
	ga = ((g0 << 8) & 0xffff0000) |
	     ((a0 >> 8) & 0x0000ffff);
	subRB = subGA = false;
	if (length == 1) {
		drb = dga = 0;
	} else {
		// calculate delta values
		int dr = int(r1 - r0) / int(length - 1);
		int dg = int(g1 - g0) / int(length - 1);
		int db = int(b1 - b0) / int(length - 1);
		int da = int(a1 - a0) / int(length - 1);
		if (db < 0) { // make sure db is positive
			dr = -dr;
			db = -db;
			subRB = true;
		}
		if (da < 0) { // make sure da is positive
			dg = -dg;
			da = -da;
			subGA = true;
		}
		// also pack two 8.16 delta values in one [8.8 ; 8.8] vector
		drb = ((unsigned(dr) << 8) & 0xffff0000) |
		      ((unsigned(db) >> 8) & 0x0000ffff);
		dga = ((unsigned(dg) << 8) & 0xffff0000) |
		      ((unsigned(da) >> 8) & 0x0000ffff);
	}
}
// Pack two [8.8 ; 8.8] vectors into one pixel.
static unsigned packRGBA(unsigned rb, unsigned ga)
{
	return (rb & 0xff00ff00) | ((ga & 0xff00ff00) >> 8);
}

// Draw a gradient on the given surface. This is a bilinear interpolation
// between 4 RGBA colors. One color for each corner, in this order:
//    0 -- 1
//    |    |
//    2 -- 3
static void gradient(const unsigned* rgba, SDL_Surface& surface, unsigned borderSize)
{
	int width  = surface.w - 2 * borderSize;
	int height = surface.h - 2 * borderSize;
	if ((width <= 0) || (height <= 0)) return;

	unsigned r0, g0, b0, a0;
	unsigned r1, g1, b1, a1;
	int dr02, dg02, db02, da02;
	int dr13, dg13, db13, da13;
	setupInterp1(rgba[0], rgba[2], height, r0, g0, b0, a0, dr02, dg02, db02, da02);
	setupInterp1(rgba[1], rgba[3], height, r1, g1, b1, a1, dr13, dg13, db13, da13);

	auto buffer = static_cast<unsigned*>(surface.pixels);
	buffer += borderSize;
	buffer += borderSize * (surface.pitch / sizeof(unsigned));
	for (int y = 0; y < height; ++y) {
		unsigned  rb,  ga;
		unsigned drb, dga;
		bool   subRB, subGA;
		setupInterp2(r0, g0, b0, a0, r1, g1, b1, a1, width,
		             rb, ga, drb, dga, subRB, subGA);

		// Depending on the subRB/subGA booleans, we need to add or
		// subtract the delta to/from the initial value. There are
		// 2 booleans so 4 combinations:
		if (!subRB) {
			if (!subGA) {
				for (int x = 0; x < width; ++x) {
					buffer[x] = packRGBA(rb, ga);
					rb += drb; ga += dga;
				}
			} else {
				for (int x = 0; x < width; ++x) {
					buffer[x] = packRGBA(rb, ga);
					rb += drb; ga -= dga;
				}
			}
		} else {
			if (!subGA) {
				for (int x = 0; x < width; ++x) {
					buffer[x] = packRGBA(rb, ga);
					rb -= drb; ga += dga;
				}
			} else {
				for (int x = 0; x < width; ++x) {
					buffer[x] = packRGBA(rb, ga);
					rb -= drb; ga -= dga;
				}
			}
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

SDLImage::SDLImage(const std::string& filename, float scaleFactor)
	: image(loadImage(filename, scaleFactor))
	, a(-1), flipX(scaleFactor < 0.0f), flipY(scaleFactor < 0.0f)
{
}

SDLImage::SDLImage(const string& filename, ivec2 size)
	: image(loadImage(filename, size))
	, a(-1), flipX(size[0] < 0), flipY(size[1] < 0)
{
}

SDLImage::SDLImage(ivec2 size, unsigned rgba)
	: flipX(size[0] < 0), flipY(size[1] < 0)
{
	initSolid(size, rgba, 0, 0); // no border
}


SDLImage::SDLImage(ivec2 size, const unsigned* rgba,
                   unsigned borderSize, unsigned borderRGBA)
	: flipX(size[0] < 0), flipY(size[1] < 0)
{
	if ((rgba[0] == rgba[1]) &&
	    (rgba[0] == rgba[2]) &&
	    (rgba[0] == rgba[3])) {
		initSolid   (size, rgba[0], borderSize, borderRGBA);
	} else {
		initGradient(size, rgba,    borderSize, borderRGBA);
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
		SDL_FillRect(&image, nullptr, color);
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

void SDLImage::initSolid(ivec2 size, unsigned rgba,
                         unsigned borderSize, unsigned borderRGBA)
{
	checkSize(size);
	if ((size[0] == 0) || (size[1] == 0)) {
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

	// Figure out required bpp and color masks.
	Uint32 rmask, gmask, bmask, amask;
	unsigned bpp;
	if (a == -1) {
		// We need an alpha channel.
		//  The SDL documentation doesn't specify this, but I've
		//  checked the implemenation (SDL-1.2.15):
		//  SDL_DisplayFormatAlpha() always returns a 32bpp surface,
		//  also when the current display surface is 16bpp.
		bpp = 32;
		getRGBAmasks32(rmask, gmask, bmask, amask);
	} else {
		// No alpha channel, copy format of the display surface.
		SDL_Surface* videoSurface = SDL_GetVideoSurface();
		assert(videoSurface);
		auto& format = *videoSurface->format;
		bpp   = format.BitsPerPixel;
		rmask = format.Rmask;
		gmask = format.Gmask;
		bmask = format.Bmask;
		amask = 0;
	}

	// Create surface with correct size/masks.
	image = SDLSurfacePtr(abs(size[0]), abs(size[1]), bpp,
	                      rmask, gmask, bmask, amask);

	// draw interior
	SDL_FillRect(image.get(), nullptr, convertColor(*image->format, rgba));

	drawBorder(*image, borderSize, borderRGBA);
}

void SDLImage::initGradient(ivec2 size, const unsigned* rgba_,
                            unsigned borderSize, unsigned borderRGBA)
{
	checkSize(size);
	if ((size[0] == 0) || (size[1] == 0)) {
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

	bool needAlphaChannel = a == -1;
	Uint32 rmask, gmask, bmask, amask;
	getRGBAmasks32(rmask, gmask, bmask, amask);
	if (!needAlphaChannel) amask = 0;
	SDLSurfacePtr tmp32(abs(size[0]), abs(size[1]), 32,
	                    rmask, gmask, bmask, amask);
	for (auto& c : rgba) {
		c = convertColor(*tmp32->format, c);
	}
	gradient(rgba, *tmp32, borderSize);
	drawBorder(*tmp32, borderSize, borderRGBA);

	auto& outFormat = *SDL_GetVideoSurface()->format;
	if ((outFormat.BitsPerPixel == 32) || needAlphaChannel) {
		if (outFormat.BitsPerPixel == 32) {
			// for 32bpp the format must match
			SDL_PixelFormat& inFormat  = *tmp32->format;
			(void)&inFormat;
			assert(inFormat.Rmask == outFormat.Rmask);
			assert(inFormat.Gmask == outFormat.Gmask);
			assert(inFormat.Bmask == outFormat.Bmask);
			// don't compare Amask
		} else {
			// For 16bpp with alpha channel, also create a 32bpp
			// image surface. See also comments in initSolid().
		}
		image = std::move(tmp32);
	} else {
		image.reset(SDL_DisplayFormat(tmp32.get()));
	}
}

SDLImage::SDLImage(SDLSurfacePtr image_)
	: image(std::move(image_))
	, a(-1), flipX(false), flipY(false)
{
}

void SDLImage::allocateWorkImage()
{
	int flags = SDL_SWSURFACE;
	auto& format = *image->format;
	workImage.reset(SDL_CreateRGBSurface(flags,
		image->w, image->h, format.BitsPerPixel,
		format.Rmask, format.Gmask, format.Bmask, 0));
	if (!workImage) {
		throw FatalError("Couldn't allocate SDLImage workimage");
	}
}

void SDLImage::draw(OutputSurface& output, gl::ivec2 pos, byte r, byte g, byte b, byte alpha)
{
	assert(r == 255); (void)r;
	assert(g == 255); (void)g;
	assert(b == 255); (void)b;

	if (!image) return;
	if (flipX) pos[0] -= image->w;
	if (flipY) pos[1] -= image->h;

	output.unlock();
	SDL_Surface* outputSurface = output.getSDLSurface();
	SDL_Rect rect;
	rect.x = pos[0];
	rect.y = pos[1];
	if (a == -1) {
		if (alpha == 255) {
			SDL_BlitSurface(image.get(), nullptr, outputSurface, &rect);
		} else {
			if (!workImage) allocateWorkImage();
			rect.w = image->w;
			rect.h = image->h;
			SDL_BlitSurface(outputSurface, &rect, workImage.get(), nullptr);
			SDL_BlitSurface(image.get(),   nullptr,  workImage.get(), nullptr);
			SDL_SetAlpha(workImage.get(), SDL_SRCALPHA, alpha);
			SDL_BlitSurface(workImage.get(), nullptr, outputSurface, &rect);
		}
	} else {
		SDL_SetAlpha(image.get(), SDL_SRCALPHA, (a * alpha) / 256);
		SDL_BlitSurface(image.get(), nullptr, outputSurface, &rect);
	}
}

ivec2 SDLImage::getSize() const
{
	return image ? ivec2(image->w, image->h) : ivec2();
}

} // namespace openmsx
