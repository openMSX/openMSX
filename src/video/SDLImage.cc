#include "SDLImage.hh"
#include "PNG.hh"
#include "SDLOutputSurface.hh"
#include "checked_cast.hh"
#include "ranges.hh"
#include "xrange.hh"
#include <cassert>
#include <cstdlib>
#include <cmath>
#include <SDL.h>

using namespace gl;

namespace openmsx {

static SDLSurfacePtr getTempSurface(ivec2 size_)
{
	int displayIndex = 0;
	SDL_DisplayMode currentMode;
	if (SDL_GetCurrentDisplayMode(displayIndex, &currentMode) != 0) {
		// Error. Can this happen? Anything we can do?
		assert(false);
	}
	int bpp;
	Uint32 rmask, gmask, bmask, amask;
	SDL_PixelFormatEnumToMasks(
		currentMode.format, &bpp, &rmask, &gmask, &bmask, &amask);
	if (bpp == 32) {
		if (amask == 0) {
			amask = ~(rmask | gmask | bmask);
		}
	} else { // TODO should we also check {R,G,B}_loss == 0?
		// Use ARGB8888 as a fallback
		amask = 0xff000000;
		rmask = 0x00ff0000;
		gmask = 0x0000ff00;
		bmask = 0x000000ff;
	}

	return {unsigned(abs(size_[0])), unsigned(abs(size_[1])), 32,
	        rmask, gmask, bmask, amask};
}

// Helper functions to draw a gradient
//  Extract R,G,B,A components to 8.16 bit fixed point.
//  Note the order R,G,B,A is arbitrary, the actual pixel value may have the
//  components in a different order.
struct UnpackedRGBA {
	unsigned r, g, b, a;
};
static constexpr UnpackedRGBA unpackRGBA(unsigned rgba)
{
	unsigned r = (((rgba >> 24) & 0xFF) << 16) + 0x8000;
	unsigned g = (((rgba >> 16) & 0xFF) << 16) + 0x8000;
	unsigned b = (((rgba >>  8) & 0xFF) << 16) + 0x8000;
	unsigned a = (((rgba >>  0) & 0xFF) << 16) + 0x8000;
	return {r, g, b, a};
}
// Setup outer loop (vertical) interpolation parameters.
//  For each component there is a pair of (initial,delta) values. These values
//  are 8.16 bit fixed point, delta is signed.
struct Interp1Result {
	unsigned r0, g0, b0, a0;
	int dr, dg, db, da;
};
static constexpr Interp1Result setupInterp1(unsigned rgba0, unsigned rgba1, unsigned length)
{
	auto [r0, g0, b0, a0] = unpackRGBA(rgba0);
	if (length == 1) {
		return {r0, g0, b0, a0,
		        0,  0,  0,  0};
	} else {
		auto [r1, g1, b1, a1] = unpackRGBA(rgba1);
		int dr = int(r1 - r0) / int(length - 1);
		int dg = int(g1 - g0) / int(length - 1);
		int db = int(b1 - b0) / int(length - 1);
		int da = int(a1 - a0) / int(length - 1);
		return {r0, g0, b0, a0,
		        dr, dg, db, da};
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
struct Interp2Result {
	unsigned rb, ga;
	unsigned drb, dga;
	bool subRB, subGA;
};
static constexpr Interp2Result setupInterp2(
	unsigned r0, unsigned g0, unsigned b0, unsigned a0,
	unsigned r1, unsigned g1, unsigned b1, unsigned a1,
	unsigned length)
{
	// Pack the initial values for the components R,B and G,A into
	// a vector-type: two 8.16 scalars -> one [8.8 ; 8.8] vector
	unsigned rb = ((r0 << 8) & 0xffff0000) |
	              ((b0 >> 8) & 0x0000ffff);
	unsigned ga = ((g0 << 8) & 0xffff0000) |
	              ((a0 >> 8) & 0x0000ffff);
	if (length == 1) {
		return {rb, ga, 0, 0, false, false};
	} else {
		// calculate delta values
		bool subRB = false;
		bool subGA = false;
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
		unsigned drb = ((unsigned(dr) << 8) & 0xffff0000) |
		               ((unsigned(db) >> 8) & 0x0000ffff);
		unsigned dga = ((unsigned(dg) << 8) & 0xffff0000) |
		               ((unsigned(da) >> 8) & 0x0000ffff);
		return {rb, ga, drb, dga, subRB, subGA};
	}
}
// Pack two [8.8 ; 8.8] vectors into one pixel.
static constexpr unsigned packRGBA(unsigned rb, unsigned ga)
{
	return (rb & 0xff00ff00) | ((ga & 0xff00ff00) >> 8);
}

// Draw a gradient on the given surface. This is a bilinear interpolation
// between 4 RGBA colors. One color for each corner, in this order:
//    0 -- 1
//    |    |
//    2 -- 3
static constexpr void gradient(std::span<const unsigned, 4> rgba, SDL_Surface& surface, unsigned borderSize)
{
	int width  = surface.w - 2 * borderSize;
	int height = surface.h - 2 * borderSize;
	if ((width <= 0) || (height <= 0)) return;

	auto [r0, g0, b0, a0, dr02, dg02, db02, da02] = setupInterp1(rgba[0], rgba[2], height);
	auto [r1, g1, b1, a1, dr13, dg13, db13, da13] = setupInterp1(rgba[1], rgba[3], height);

	auto* buffer = static_cast<unsigned*>(surface.pixels);
	buffer += borderSize;
	buffer += borderSize * (surface.pitch / sizeof(unsigned));
	repeat(height, [&, r0=r0, g0=g0, b0=b0, a0=a0, dr02=dr02, dg02=dg02, db02=db02, da02=da02,
	                   r1=r1, g1=g1, b1=b1, a1=a1, dr13=dr13, dg13=dg13, db13=db13, da13=da13] () mutable {
		auto [rb,  ga, drb, dga, subRB, subGA] = setupInterp2(r0, g0, b0, a0, r1, g1, b1, a1, width);

		// Depending on the subRB/subGA booleans, we need to add or
		// subtract the delta to/from the initial value. There are
		// 2 booleans so 4 combinations:
		if (!subRB) {
			if (!subGA) {
				for (auto x : xrange(width)) {
					buffer[x] = packRGBA(rb, ga);
					rb += drb; ga += dga;
				}
			} else {
				for (auto x : xrange(width)) {
					buffer[x] = packRGBA(rb, ga);
					rb += drb; ga -= dga;
				}
			}
		} else {
			if (!subGA) {
				for (auto x : xrange(width)) {
					buffer[x] = packRGBA(rb, ga);
					rb -= drb; ga += dga;
				}
			} else {
				for (auto x : xrange(width)) {
					buffer[x] = packRGBA(rb, ga);
					rb -= drb; ga -= dga;
				}
			}
		}

		r0 += dr02; g0 += dg02; b0 += db02; a0 += da02;
		r1 += dr13; g1 += dg13; b1 += db13; a1 += da13;
		buffer += (surface.pitch / sizeof(unsigned));
	});
}

// class SDLImage

SDLImage::SDLImage(OutputSurface& output, const std::string& filename)
	: texture(loadImage(output, filename))
	, flipX(false), flipY(false)
{
}

// TODO get rid of this constructor
//  instead allow to draw the same SDLImage to different sizes
SDLImage::SDLImage(OutputSurface& output, const std::string& filename, float scaleFactor)
	: texture(loadImage(output, filename))
	, flipX(scaleFactor < 0.0f), flipY(scaleFactor < 0.0f)
{
	size = trunc(vec2(size) * std::abs(scaleFactor)); // scale image size
}

// TODO get rid of this constructor, see above
SDLImage::SDLImage(OutputSurface& output, const std::string& filename, ivec2 size_)
	: texture(loadImage(output, filename))
	, flipX(size_[0] < 0), flipY(size_[1] < 0)
{
	size = size_; // replace image size
}

SDLImage::SDLImage(OutputSurface& output, ivec2 size_, unsigned rgba)
	: flipX(size_[0] < 0), flipY(size_[1] < 0)
{
	initSolid(output, size_, rgba, 0, 0); // no border
}


SDLImage::SDLImage(OutputSurface& output, ivec2 size_, std::span<const unsigned, 4> rgba,
                   unsigned borderSize, unsigned borderRGBA)
	: flipX(size_[0] < 0), flipY(size_[1] < 0)
{
	if ((rgba[0] == rgba[1]) &&
	    (rgba[0] == rgba[2]) &&
	    (rgba[0] == rgba[3])) {
		initSolid   (output, size_, rgba[0], borderSize, borderRGBA);
	} else {
		initGradient(output, size_, rgba,    borderSize, borderRGBA);
	}
}

SDLImage::SDLImage(OutputSurface& output, SDLSurfacePtr image)
	: texture(toTexture(output, *image))
	, flipX(false), flipY(false)
{
}

SDLTexturePtr SDLImage::toTexture(OutputSurface& output, SDL_Surface& surface)
{
	SDLTexturePtr result(SDL_CreateTextureFromSurface(
		checked_cast<SDLOutputSurface&>(output).getSDLRenderer(), &surface));
	SDL_SetTextureBlendMode(result.get(), SDL_BLENDMODE_BLEND);
	SDL_QueryTexture(result.get(), nullptr, nullptr, &size[0], &size[1]);
	return result;
}

SDLTexturePtr SDLImage::loadImage(OutputSurface& output, const std::string& filename)
{
	bool want32bpp = true;
	return toTexture(output, *PNG::load(filename, want32bpp));
}


static unsigned convertColor(const SDL_PixelFormat& format, unsigned rgba)
{
	return SDL_MapRGBA(
		&format,
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

void SDLImage::initSolid(OutputSurface& output, ivec2 size_, unsigned rgba,
                         unsigned borderSize, unsigned borderRGBA)
{
	checkSize(size_);
	if ((size_[0] == 0) || (size_[1] == 0)) {
		// SDL_FillRect crashes on zero-width surfaces, so check for it
		return;
	}

	SDLSurfacePtr tmp32 = getTempSurface(size_);

	// draw interior
	SDL_FillRect(tmp32.get(), nullptr, convertColor(*tmp32->format, rgba));

	drawBorder(*tmp32, borderSize, borderRGBA);

	texture = toTexture(output, *tmp32);
}

void SDLImage::initGradient(OutputSurface& output, ivec2 size_, std::span<const unsigned, 4> rgba_,
                            unsigned borderSize, unsigned borderRGBA)
{
	checkSize(size_);
	if ((size_[0] == 0) || (size_[1] == 0)) {
		return;
	}

	std::array<unsigned, 4> rgba;
	ranges::copy(rgba_, rgba);

	if (flipX) {
		std::swap(rgba[0], rgba[1]);
		std::swap(rgba[2], rgba[3]);
	}
	if (flipY) {
		std::swap(rgba[0], rgba[2]);
		std::swap(rgba[1], rgba[3]);
	}

	SDLSurfacePtr tmp32 = getTempSurface(size_);
	for (auto& c : rgba) {
		c = convertColor(*tmp32->format, c);
	}
	gradient(rgba, *tmp32, borderSize);
	drawBorder(*tmp32, borderSize, borderRGBA);

	texture = toTexture(output, *tmp32);
}

void SDLImage::draw(OutputSurface& output, gl::ivec2 pos, uint8_t r, uint8_t g, uint8_t b, uint8_t alpha)
{
	assert(r == 255); (void)r; // SDL2 supports this now, but do we need it?
	assert(g == 255); (void)g;
	assert(b == 255); (void)b;

	if (!texture) return;

	auto [x, y] = pos;
	auto [w, h] = size;
	if (flipX) x -= w;
	if (flipY) y -= h;

	auto* renderer = checked_cast<SDLOutputSurface&>(output).getSDLRenderer();
	SDL_SetTextureAlphaMod(texture.get(), alpha);
	SDL_Rect dst = {x, y, w, h};
	SDL_RenderCopy(renderer, texture.get(), nullptr, &dst);
}

} // namespace openmsx
