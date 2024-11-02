#include "PNG.hh"

#include "File.hh"
#include "MSXException.hh"
#include "PixelOperations.hh"
#include "Version.hh"

#include "cstdiop.hh"
#include "endian.hh"
#include "narrow.hh"
#include "one_of.hh"
#include "small_buffer.hh"
#include "view.hh"

#include <png.h>
#include <SDL.h>

#include <array>
#include <bit>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <limits>

namespace openmsx::PNG {

[[noreturn]] static void handleError(png_structp png_ptr, png_const_charp error_msg)
{
	const auto* operation = std::bit_cast<const char*>(
		png_get_error_ptr(png_ptr));
	throw MSXException("Error while ", operation, " PNG: ", error_msg);
}

static void handleWarning(png_structp png_ptr, png_const_charp warning_msg)
{
	const auto* operation = std::bit_cast<const char*>(
		png_get_error_ptr(png_ptr));
	std::cerr << "Warning while " << operation << " PNG: "
		<< warning_msg << '\n';
}

/*
The copyright notice below applies to the original PNG load code, which was
imported from SDL_image 1.2.10, file "IMG_png.c", function "IMG_LoadPNG_RW".
===============================================================================
        File: SDL_png.c
     Purpose: A PNG loader and saver for the SDL library
    Revision:
  Created by: Philippe Lavoie          (2 November 1998)
              lavoie@zeus.genie.uottawa.ca
 Modified by:

 Copyright notice:
          Copyright (C) 1998 Philippe Lavoie

          This library is free software; you can redistribute it and/or
          modify it under the terms of the GNU Library General Public
          License as published by the Free Software Foundation; either
          version 2 of the License, or (at your option) any later version.

          This library is distributed in the hope that it will be useful,
          but WITHOUT ANY WARRANTY; without even the implied warranty of
          MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
          Library General Public License for more details.

          You should have received a copy of the GNU Library General Public
          License along with this library; if not, write to the Free
          Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    Comments: The load and save routine are basically the ones you can find
             in the example.c file from the libpng distribution.

  Changes:
    1999-05-17: Modified to use the new SDL data sources - Sam Lantinga
    2009-12-29: Modified for use in openMSX - Maarten ter Huurne
                                              and Wouter Vermaelen

===============================================================================
*/

struct PNGReadHandle {
	PNGReadHandle() = default;
	PNGReadHandle(const PNGReadHandle&) = delete;
	PNGReadHandle(PNGReadHandle&&) = delete;
	PNGReadHandle& operator=(const PNGReadHandle&) = delete;
	PNGReadHandle& operator=(PNGReadHandle&&) = delete;
	~PNGReadHandle()
	{
		if (ptr) {
			png_destroy_read_struct(&ptr, info ? &info : nullptr, nullptr);
		}
	}

	png_structp ptr = nullptr;
	png_infop info = nullptr;
};

static void readData(png_structp ctx, png_bytep area, png_size_t size)
{
	auto* file = std::bit_cast<File*>(png_get_io_ptr(ctx));
	file->read(std::span{area, size});
}

SDLSurfacePtr load(const std::string& filename, bool want32bpp)
{
	File file(filename);

	try {
		// Create the PNG loading context structure.
		PNGReadHandle png;
		png.ptr = png_create_read_struct(
			PNG_LIBPNG_VER_STRING,
			const_cast<char*>("decoding"), handleError, handleWarning);
		if (!png.ptr) {
			throw MSXException("Failed to allocate main struct");
		}

		// Allocate/initialize the memory for image information.
		png.info = png_create_info_struct(png.ptr);
		if (!png.info) {
			throw MSXException("Failed to allocate image info struct");
		}

		// Set up the input control.
		png_set_read_fn(png.ptr, &file, readData);

		// Read PNG header info.
		png_read_info(png.ptr, png.info);
		png_uint_32 width, height;
		int bit_depth, color_type, interlace_type;
		png_get_IHDR(png.ptr, png.info, &width, &height, &bit_depth,
		             &color_type, &interlace_type, nullptr, nullptr);

		// Tell libpng to strip 16 bit/color files down to 8 bits/color.
		png_set_strip_16(png.ptr);

		// Extract multiple pixels with bit depths of 1, 2, and 4 from a single
		// byte into separate bytes (useful for paletted and grayscale images).
		png_set_packing(png.ptr);

		// The following enables:
		// - transformation of grayscale images of less than 8 to 8 bits
		// - changes paletted images to RGB
		// - adds a full alpha channel if there is transparency information in a tRNS chunk
		png_set_expand(png.ptr);

		if (want32bpp) {
			png_set_filler(png.ptr, 0xff, PNG_FILLER_AFTER);
		}

		// always convert grayscale to RGB
		//  together with all the above conversions, the resulting image will
		//  be either RGB or RGBA with 8 bits per component.
		png_set_gray_to_rgb(png.ptr);

		png_read_update_info(png.ptr, png.info);

		png_get_IHDR(png.ptr, png.info, &width, &height, &bit_depth,
		             &color_type, &interlace_type, nullptr, nullptr);

		// Allocate the SDL surface to hold the image.
		constexpr unsigned MAX_SIZE = 2048;
		if (width > MAX_SIZE) {
			throw MSXException(
				"Attempted to create a surface with excessive width: ",
				width, ", max ", MAX_SIZE);
		}
		if (height > MAX_SIZE) {
			throw MSXException(
				"Attempted to create a surface with excessive height: ",
				height, ", max ", MAX_SIZE);
		}
		int bpp = png_get_channels(png.ptr, png.info) * 8;
		assert(bpp == one_of(24, 32));
		PixelOperations pixelOps;
		SDLSurfacePtr surface(width, height, bpp,
		                      pixelOps.getRmask(), pixelOps.getGmask(), pixelOps.getBmask(),
		                      ((bpp == 32) ? pixelOps.getAmask() : 0));

		// Create the array of pointers to image data.
		small_buffer<png_bytep, 1080> rowPointers(view::transform(xrange(height),
			[&](auto y) { return std::bit_cast<png_bytep>(surface.getLinePtr(y)); }));

		// Read the entire image in one go.
		png_read_image(png.ptr, rowPointers.data());

		// In some cases it can't read PNGs created by some popular programs
		// (ACDSEE), we do not want to process comments, so we omit png_read_end
		//png_read_end(png.ptr, png.info);

		return surface;
	} catch (MSXException& e) {
		throw MSXException(
			"Error while loading PNG file \"", filename, "\": ",
			e.getMessage());
	}
}


/* PNG save code by Darren Grant sdl@lokigames.com */
/* heavily modified for openMSX by Joost Damad joost@lumatec.be */

struct PNGWriteHandle {
	PNGWriteHandle() = default;
	PNGWriteHandle(const PNGWriteHandle&) = delete;
	PNGWriteHandle(PNGWriteHandle&&) = delete;
	PNGWriteHandle& operator=(const PNGWriteHandle&) = delete;
	PNGWriteHandle& operator=(PNGWriteHandle&&) = delete;
	~PNGWriteHandle()
	{
		if (ptr) {
			png_destroy_write_struct(&ptr, info ? &info : nullptr);
		}
	}

	png_structp ptr = nullptr;
	png_infop info = nullptr;
};

static void writeData(png_structp ctx, png_bytep area, png_size_t size)
{
	auto* file = std::bit_cast<File*>(png_get_io_ptr(ctx));
	file->write(std::span{area, size});
}

static void flushData(png_structp ctx)
{
	auto* file = std::bit_cast<File*>(png_get_io_ptr(ctx));
	file->flush();
}

static void IMG_SavePNG_RW(size_t width, std::span<const void*> rowPointers,
                           const std::string& filename, bool color)
{
	auto height = rowPointers.size();
	assert(width  <= std::numeric_limits<png_uint_32>::max());
	assert(height <= std::numeric_limits<png_uint_32>::max());
	try {
		File file(filename, File::OpenMode::TRUNCATE);

		PNGWriteHandle png;
		png.ptr = png_create_write_struct(
			PNG_LIBPNG_VER_STRING,
			const_cast<char*>("encoding"), handleError, handleWarning);
		if (!png.ptr) {
			throw MSXException("Failed to allocate main struct");
		}

		// Allocate/initialize the image information data.  REQUIRED
		png.info = png_create_info_struct(png.ptr);
		if (!png.info) {
			// Couldn't create image information for PNG file
			throw MSXException("Failed to allocate image info struct");
		}

		// Set up the output control.
		png_set_write_fn(png.ptr, &file, writeData, flushData);

		// Mark this image as being generated by openMSX and add creation time.
		std::string version = Version::full();
		std::array<png_text, 2> text;
		text[0].compression = PNG_TEXT_COMPRESSION_NONE;
		text[0].key  = const_cast<char*>("Software");
		text[0].text = const_cast<char*>(version.c_str());
		text[1].compression = PNG_TEXT_COMPRESSION_NONE;
		text[1].key  = const_cast<char*>("Creation Time");

		// A buffer size of 20 characters is large enough till the year
		// 9999. But the compiler doesn't understand calendars and
		// warns that the snprintf output could be truncated (e.g.
		// because the year is -2147483647). To silence this warning
		// (and also to work around the windows _snprintf stuff) we add
		// some extra buffer space.
		static constexpr size_t size = (10 + 1 + 8 + 1) + 44;
		time_t now = time(nullptr);
		const struct tm* tm = localtime(&now);
		std::array<char, size> timeStr;
		snprintf(timeStr.data(), sizeof(timeStr), "%04d-%02d-%02d %02d:%02d:%02d",
		         1900 + tm->tm_year, tm->tm_mon + 1, tm->tm_mday,
		         tm->tm_hour, tm->tm_min, tm->tm_sec);
		text[1].text = timeStr.data();

		png_set_text(png.ptr, png.info, text.data(), narrow<int>(text.size()));

		png_set_IHDR(png.ptr, png.info,
		             narrow<png_uint_32>(width), narrow<png_uint_32>(height),
		             8,
		             color ? PNG_COLOR_TYPE_RGB : PNG_COLOR_TYPE_GRAY,
		             PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE,
		             PNG_FILTER_TYPE_BASE);

		// Write the file header information.  REQUIRED
		png_write_info(png.ptr, png.info);

		// Write out the entire image data in one call.
		png_write_image(
			png.ptr,
			std::bit_cast<png_bytep*>(const_cast<void**>(rowPointers.data())));
		png_write_end(png.ptr, png.info);
	} catch (MSXException& e) {
		throw MSXException(
			"Error while writing PNG file \"", filename, "\": ",
			e.getMessage());
	}
}

static void save(SDL_Surface* image, const std::string& filename)
{
	SDLAllocFormatPtr frmt24(SDL_AllocFormat(
		Endian::BIG ? SDL_PIXELFORMAT_BGR24 : SDL_PIXELFORMAT_RGB24));
	SDLSurfacePtr surf24(SDL_ConvertSurface(image, frmt24.get(), 0));

	// Create the array of pointers to image data
	small_buffer<const void*, 1080> rowPointers(view::transform(xrange(image->h),
		[&](auto y) { return surf24.getLinePtr(y); }));

	IMG_SavePNG_RW(image->w, rowPointers, filename, true);
}

void saveRGBA(size_t width, std::span<const uint32_t*> rowPointers,
              const std::string& filename)
{
	// this implementation creates 1 extra copy, can be optimized if required
	auto height = narrow<unsigned>(rowPointers.size());
	static constexpr int bpp = 32;
	PixelOperations pixelOps;
	SDLSurfacePtr surface(
		narrow<unsigned>(width), height, bpp,
		pixelOps.getRmask(), pixelOps.getGmask(),
		pixelOps.getBmask(), pixelOps.getAmask());
	for (auto y : xrange(height)) {
		memcpy(surface.getLinePtr(y),
		       rowPointers[y], width * sizeof(uint32_t));
	}
	save(surface.get(), filename);
}

void saveGrayscale(size_t width, std::span<const uint8_t*> rowPointers_,
                   const std::string& filename)
{
	std::span rowPointers{std::bit_cast<const void**>(rowPointers_.data()),
	                      rowPointers_.size()};
	IMG_SavePNG_RW(width, rowPointers, filename, false);
}

} // namespace openmsx::PNG
