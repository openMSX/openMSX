// $Id$

#include "PNG.hh"
#include "SDLSurfacePtr.hh"
#include "MSXException.hh"
#include "File.hh"
#include "StringOp.hh"
#include "build-info.hh"
#include "Version.hh"
#include "vla.hh"
#include "cstdiop.hh"
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <png.h>
#include <SDL.h>

namespace openmsx {
namespace PNG {

static void handleError(png_structp png_ptr, png_const_charp error_msg)
{
	const char* operation = reinterpret_cast<const char*>(
		png_get_error_ptr(png_ptr)
		);
	throw MSXException(StringOp::Builder() <<
		"Error while " << operation << " PNG: " << error_msg
		);
}

static void handleWarning(png_structp png_ptr, png_const_charp warning_msg)
{
	const char* operation = reinterpret_cast<const char*>(
		png_get_error_ptr(png_ptr)
		);
	std::cerr << "Warning while " << operation << " PNG: "
		<< warning_msg << std::endl;
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
	PNGReadHandle()
		: ptr(0), info(0)
	{
	}

	~PNGReadHandle()
	{
		if (ptr) {
			png_destroy_read_struct(&ptr, info ? &info : NULL, NULL);
		}
	}

	png_structp ptr;
	png_infop info;
};

static void readData(png_structp ctx, png_bytep area, png_size_t size)
{
	File* file = reinterpret_cast<File*>(png_get_io_ptr(ctx));
	file->read(area, unsigned(size));
}

SDLSurfacePtr load(const std::string& filename)
{
	File file(filename);

	try {
		// Create the PNG loading context structure.
		PNGReadHandle png;
		png.ptr = png_create_read_struct(
			PNG_LIBPNG_VER_STRING,
			const_cast<char*>("decoding"), handleError, handleWarning
			);
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
		             &color_type, &interlace_type, NULL, NULL);

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

		// always convert grayscale to RGB
		//  together with all the above conversions, the resulting image will
		//  be either RGB or RGBA with 8 bits per component.
		png_set_gray_to_rgb(png.ptr);

		png_read_update_info(png.ptr, png.info);

		png_get_IHDR(png.ptr, png.info, &width, &height, &bit_depth,
		             &color_type, &interlace_type, NULL, NULL);

		// Allocate the SDL surface to hold the image.
		static const unsigned MAX_SIZE = 2048;
		if (width > MAX_SIZE) {
			throw MSXException(StringOp::Builder() <<
				"Attempted to create a surface with excessive width: "
				<< width << ", max " << MAX_SIZE);
		}
		if (height > MAX_SIZE) {
			throw MSXException(StringOp::Builder() <<
				"Attempted to create a surface with excessive height: "
				<< height << ", max " << MAX_SIZE);
		}
		int bpp = png.info->channels * 8;
		assert(bpp == 24 || bpp == 32);
		Uint32 redMask, grnMask, bluMask, alpMask;
		if (OPENMSX_BIGENDIAN) {
			int s = bpp == 32 ? 0 : 8;
			redMask = 0xFF000000 >> s;
			grnMask = 0x00FF0000 >> s;
			bluMask = 0x0000FF00 >> s;
			alpMask = 0x000000FF >> s;
		} else {
			redMask = 0x000000FF;
			grnMask = 0x0000FF00;
			bluMask = 0x00FF0000;
			alpMask = bpp == 32 ? 0xFF000000 : 0;
		}
		SDLSurfacePtr surface(SDL_CreateRGBSurface(
			SDL_SWSURFACE, width, height, bpp,
			redMask, grnMask, bluMask, alpMask));
		if (!surface.get()) {
			throw MSXException(StringOp::Builder() <<
				"Failed to allocate a "
				<< width << "x" << height << "x" << bpp << " surface: "
				<< SDL_GetError());
		}

		// Create the array of pointers to image data.
		VLA(png_bytep, row_pointers, height);
		for (png_uint_32 row = 0; row < height; ++row) {
			row_pointers[row] = reinterpret_cast<png_bytep>(
				surface.getLinePtr(row));
		}

		// Read the entire image in one go.
		png_read_image(png.ptr, row_pointers);

		// In some cases it can't read PNG's created by some popular programs
		// (ACDSEE), we do not want to process comments, so we omit png_read_end
		//png_read_end(png.ptr, png.info);

		return surface;
	} catch (MSXException& e) {
		throw MSXException(
			"Error while loading PNG file \"" + filename + "\": " +
			e.getMessage()
			);
	}
}


/* PNG save code by Darren Grant sdl@lokigames.com */
/* heavily modified for openMSX by Joost Damad joost@lumatec.be */

struct PNGWriteHandle {
	PNGWriteHandle()
		: ptr(0), info(0)
	{
	}

	~PNGWriteHandle()
	{
		if (ptr) {
			png_destroy_write_struct(&ptr, info ? &info : NULL);
		}
	}

	png_structp ptr;
	png_infop info;
};

static void writeData(png_structp ctx, png_bytep area, png_size_t size)
{
	File* file = reinterpret_cast<File*>(png_get_io_ptr(ctx));
	file->write(area, unsigned(size));
}

static void flushData(png_structp ctx)
{
	File* file = reinterpret_cast<File*>(png_get_io_ptr(ctx));
	file->flush();
}

static void IMG_SavePNG_RW(int width, int height, const void** row_pointers,
                           const std::string& filename, bool color)
{
	try {
		File file(filename, File::TRUNCATE);

		PNGWriteHandle png;
		png.ptr = png_create_write_struct(
			PNG_LIBPNG_VER_STRING,
			const_cast<char*>("encoding"), handleError, handleWarning
			);
		if (png.ptr == NULL) {
			throw MSXException("Failed to allocate main struct");
		}

		// Allocate/initialize the image information data.  REQUIRED
		png.info = png_create_info_struct(png.ptr);
		if (png.info == NULL) {
			// Couldn't create image information for PNG file
			throw MSXException("Failed to allocate image info struct");
		}

		// Set up the output control.
		png_set_write_fn(png.ptr, &file, writeData, flushData);

		// Mark this image as being generated by openMSX and add creation time.
		png_text text[2];
		text[0].compression = PNG_TEXT_COMPRESSION_NONE;
		text[0].key  = const_cast<char*>("Software");
		text[0].text = const_cast<char*>(Version::FULL_VERSION.c_str());
		text[1].compression = PNG_TEXT_COMPRESSION_NONE;
		text[1].key  = const_cast<char*>("Creation Time");
		time_t now = time(NULL);
		struct tm* tm = localtime(&now);
		char timeStr[10 + 1 + 8 + 1];
		snprintf(timeStr, sizeof(timeStr), "%04d-%02d-%02d %02d:%02d:%02d",
				1900 + tm->tm_year, tm->tm_mon + 1, tm->tm_mday,
				tm->tm_hour, tm->tm_min, tm->tm_sec);
		text[1].text = timeStr;
		png_set_text(png.ptr, png.info, text, 2);

		png_set_IHDR(png.ptr, png.info, width, height, 8,
					color ? PNG_COLOR_TYPE_RGB : PNG_COLOR_TYPE_GRAY,
					PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE,
					PNG_FILTER_TYPE_BASE);

		// Write the file header information.  REQUIRED
		png_write_info(png.ptr, png.info);

		// Write out the entire image data in one call.
		png_write_image(
			png.ptr,
			reinterpret_cast<png_bytep*>(const_cast<void**>(row_pointers))
			);
		png_write_end(png.ptr, png.info);
	} catch (MSXException& e) {
		throw MSXException(
			"Error while writing PNG file \"" + filename + "\": " +
			e.getMessage()
			);
	}
}

void save(SDL_Surface* surface, const std::string& filename)
{
	SDL_PixelFormat frmt24;
	frmt24.palette = 0;
	frmt24.BitsPerPixel = 24;
	frmt24.BytesPerPixel = 3;
	frmt24.Rmask = OPENMSX_BIGENDIAN ? 0xFF0000 : 0x0000FF;
	frmt24.Gmask = 0x00FF00;
	frmt24.Bmask = OPENMSX_BIGENDIAN ? 0x0000FF : 0xFF0000;
	frmt24.Amask = 0;
	frmt24.Rshift = 0;
	frmt24.Gshift = 8;
	frmt24.Bshift = 16;
	frmt24.Ashift = 0;
	frmt24.Rloss = 0;
	frmt24.Gloss = 0;
	frmt24.Bloss = 0;
	frmt24.Aloss = 8;
	frmt24.colorkey = 0;
	frmt24.alpha = 0;
	SDLSurfacePtr surf24(SDL_ConvertSurface(surface, &frmt24, 0));

	// Create the array of pointers to image data
	VLA(const void*, row_pointers, surface->h);
	for (int i = 0; i < surface->h; ++i) {
		row_pointers[i] = surf24.getLinePtr(i);
	}

	IMG_SavePNG_RW(surface->w, surface->h, row_pointers, filename, true);
}

void save(unsigned width, unsigned height, const void** rowPointers,
          const SDL_PixelFormat& format, const std::string& filename)
{
	// this implementation creates 1 extra copy, can be optimized if required
	SDLSurfacePtr surface(SDL_CreateRGBSurface(
		SDL_SWSURFACE, width, height, format.BitsPerPixel,
		format.Rmask, format.Gmask, format.Bmask, format.Amask));
	for (unsigned y = 0; y < height; ++y) {
		memcpy(surface.getLinePtr(y),
		       rowPointers[y], width * format.BytesPerPixel);
	}
	save(surface.get(), filename);
}

void save(unsigned width, unsigned height,
          const void** rowPointers, const std::string& filename)
{
	IMG_SavePNG_RW(width, height, rowPointers, filename, true);
}

void saveGrayscale(unsigned width, unsigned height,
                   const void** rowPointers, const std::string& filename)
{
	IMG_SavePNG_RW(width, height, rowPointers, filename, false);
}

} // namespace PNG
} // namespace openmsx
