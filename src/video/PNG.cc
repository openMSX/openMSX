// $Id$

#include "PNG.hh"
#include "CommandException.hh"
#include "File.hh"
#include "FileOperations.hh"
#include "build-info.hh"
#include "Version.hh"
#include "vla.hh"
#include "cstdiop.hh"
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <png.h>
#include <SDL.h>

namespace openmsx {
namespace PNG {

/*
The copyright notice below applies to IMG_LoadPNG_RW(), which was imported
from SDL_image 1.2.10, file "IMG_png.c".
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

===============================================================================
*/

// Load a PNG type image from an SDL datasource.
static void png_read_data(png_structp ctx, png_bytep area, png_size_t size)
{
	SDL_RWops* src = reinterpret_cast<SDL_RWops*>(png_get_io_ptr(ctx));
	SDL_RWread(src, area, size, 1);
}
static SDL_Surface* IMG_LoadPNG_RW(SDL_RWops* src)
{
	int start = SDL_RWtell(src);

	// Initialize the data we will clean up when we're done.
	const char* error = NULL;
	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;
	SDL_Surface* volatile surface = NULL;

	// Create the PNG loading context structure.
	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png_ptr == NULL) {
		error = "Couldn't allocate memory for PNG file or incompatible PNG dll";
		goto done;
	}

	// Allocate/initialize the memory for image information.  REQUIRED.
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		error = "Couldn't create image information for PNG file";
		goto done;
	}

	// Set error handling if you are using setjmp/longjmp method (this is
	// the normal method of doing things with libpng).  REQUIRED unless you
	// set up your own error handlers in png_create_read_struct() earlier.
	if (setjmp(png_ptr->jmpbuf)) {
		error = "Error reading the PNG file.";
		goto done;
	}

	// Set up the input control.
	png_set_read_fn(png_ptr, src, png_read_data);

	// Read PNG header info.
	png_read_info(png_ptr, info_ptr);
	png_uint_32 width, height;
	int bit_depth, color_type, interlace_type;
	png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth,
		&color_type, &interlace_type, NULL, NULL);

	// Tell libpng to strip 16 bit/color files down to 8 bits/color.
	png_set_strip_16(png_ptr);

	// Extract multiple pixels with bit depths of 1, 2, and 4 from a single
	// byte into separate bytes (useful for paletted and grayscale images).
	png_set_packing(png_ptr);

	// The following enables:
	// - transformation of grayscale images of less than 8 to 8 bits
	// - changes paletted images to RGB
	// - adds a full alpha channel if there is transparency information in a tRNS chunk
	png_set_expand(png_ptr);

	// always convert grayscale to RGB
	//  together with all the above conversions, the resulting image will
	//  be either RGB or RGBA with 8 bits per component.
	png_set_gray_to_rgb(png_ptr);

	png_read_update_info(png_ptr, info_ptr);

	png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth,
		&color_type, &interlace_type, NULL, NULL);

	// Allocate the SDL surface to hold the image.
	Uint32 Rmask, Gmask, Bmask, Amask;
	if (OPENMSX_BIGENDIAN) {
		int s = (info_ptr->channels == 4) ? 0 : 8;
		Rmask = 0xFF000000 >> s;
		Gmask = 0x00FF0000 >> s;
		Bmask = 0x0000FF00 >> s;
		Amask = 0x000000FF >> s;
	} else {
		Rmask = 0x000000FF;
		Gmask = 0x0000FF00;
		Bmask = 0x00FF0000;
		Amask = (info_ptr->channels == 4) ? 0xFF000000 : 0;
	}
	surface = SDL_AllocSurface(SDL_SWSURFACE, width, height,
		bit_depth * info_ptr->channels, Rmask, Gmask, Bmask, Amask);
	if (surface == NULL) {
		error = "Out of memory";
		goto done;
	}

	// Create the array of pointers to image data.
	VLA(png_bytep,  row_pointers, height);
	for (png_uint_32 row = 0; row < height; ++row) {
		row_pointers[row] = static_cast<png_bytep>(
			static_cast<Uint8*>(surface->pixels) + row * surface->pitch);
	}

	// Read the entire image in one go.
	png_read_image(png_ptr, row_pointers);

	// and we're done!  (png_read_end() can be omitted if no processing of
	// post-IDAT text/time/etc. is desired)
	// In some cases it can't read PNG's created by some popular programs (ACDSEE),
	// we do not want to process comments, so we omit png_read_end
	//png_read_end(png_ptr, info_ptr);

done: // Clean up and return.
	if (png_ptr) {
		png_destroy_read_struct(&png_ptr, info_ptr ? &info_ptr : NULL, NULL);
	}
	if (error) {
		SDL_RWseek(src, start, RW_SEEK_SET);
		if (surface) {
			SDL_FreeSurface(surface);
			surface = NULL;
		}
		SDL_SetError(error);
	}
	return surface;
}

SDL_Surface* load(const std::string& filename)
{
	File file(filename);
	SDL_RWops* src = SDL_RWFromConstMem(file.mmap(), file.getSize());
	if (!src) {
		throw MSXException(
			"Failed to create SDL_RWops for mmapped file \"" + filename + "\""
			);
	}
	SDL_Surface* result = IMG_LoadPNG_RW(src);
	SDL_RWclose(src);
	if (!result) {
		throw MSXException("File \"" + filename + "\" is not a valid PNG image");
	}
	return result;
}

/* PNG save code by Darren Grant sdl@lokigames.com */
/* heavily modified for openMSX by Joost Damad joost@lumatec.be */

static bool IMG_SavePNG_RW(int width, int height, const void** row_pointers,
                           const std::string& filename, bool color)
{
	// initialize these before the goto
	time_t t = time(NULL);
	struct tm* tm = localtime(&t);
	png_bytep* ptrs = reinterpret_cast<png_bytep*>(const_cast<void**>(row_pointers));

	FILE* fp = FileOperations::openFile(filename, "wb");
	if (!fp) {
		return false;
	}
	png_infop info_ptr = 0;

	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
	        NULL, NULL, NULL);
	if (png_ptr == NULL) {
		// Couldn't allocate memory for PNG file
		goto error;
	}
	png_ptr->io_ptr = fp;

	// Allocate/initialize the image information data.  REQUIRED
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		// Couldn't create image information for PNG file
		goto error;
	}

	// Set error handling.
	if (setjmp(png_ptr->jmpbuf)) {
		// Error writing the PNG file
		goto error;
	}

	/* Set the image information here.  Width and height are up to 2^31,
	 * bit_depth is one of 1, 2, 4, 8, or 16, but valid values also depend on
	 * the color_type selected. color_type is one of PNG_COLOR_TYPE_GRAY,
	 * PNG_COLOR_TYPE_GRAY_ALPHA, PNG_COLOR_TYPE_PALETTE, PNG_COLOR_TYPE_RGB,
	 * or PNG_COLOR_TYPE_RGB_ALPHA.  interlace is either PNG_INTERLACE_NONE or
	 * PNG_INTERLACE_ADAM7, and the compression_type and filter_type MUST
	 * currently be PNG_COMPRESSION_TYPE_BASE and PNG_FILTER_TYPE_BASE. REQUIRED
	 */

	// WARNING Joost: for now always convert to 8bit/color (==24bit) image
	// because that is by far the easiest thing to do

	// First mark this image as being generated by openMSX and add creation time
	png_text text[2];
	text[0].compression = PNG_TEXT_COMPRESSION_NONE;
	text[0].key  = const_cast<char*>("Software");
	text[0].text = const_cast<char*>(Version::FULL_VERSION.c_str());
	text[1].compression = PNG_TEXT_COMPRESSION_NONE;
	text[1].key  = const_cast<char*>("Creation Time");
	char timeStr[10 + 1 + 8 + 1];
	snprintf(timeStr, sizeof(timeStr), "%04d-%02d-%02d %02d:%02d:%02d",
	         1900 + tm->tm_year, tm->tm_mon + 1, tm->tm_mday,
	         tm->tm_hour, tm->tm_min, tm->tm_sec);
	text[1].text = timeStr;
	png_set_text(png_ptr, info_ptr, text, 2);

	png_set_IHDR(png_ptr, info_ptr, width, height, 8,
	             color ? PNG_COLOR_TYPE_RGB : PNG_COLOR_TYPE_GRAY,
	             PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE,
	             PNG_FILTER_TYPE_BASE);

	// Write the file header information.  REQUIRED
	png_write_info(png_ptr, info_ptr);

	// write out the entire image data in one call
	png_write_image(png_ptr, ptrs);
	png_write_end(png_ptr, info_ptr);

	free(info_ptr->palette);
	png_destroy_write_struct(&png_ptr, &info_ptr);

	fclose(fp);
	return true;

error:
	free(info_ptr->palette);
	png_destroy_write_struct(&png_ptr, &info_ptr);

	fclose(fp);
	return false;
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
	SDL_Surface* surf24 = SDL_ConvertSurface(surface, &frmt24, 0);

	// Create the array of pointers to image data
	VLA(const void*, row_pointers, surface->h);
	for (int i = 0; i < surface->h; ++i) {
		row_pointers[i] = static_cast<char*>(surf24->pixels) + (i * surf24->pitch);
	}

	bool result = IMG_SavePNG_RW(surface->w, surface->h,
	                             row_pointers, filename, true);

	SDL_FreeSurface(surf24);

	if (!result) {
		throw CommandException("Failed to write " + filename);
	}
}

void save(unsigned width, unsigned height, const void** rowPointers,
          const SDL_PixelFormat& format, const std::string& filename)
{
	// this implementation creates 1 extra copy, can be optimized if required
	SDL_Surface* surface = SDL_CreateRGBSurface(
		SDL_SWSURFACE, width, height, format.BitsPerPixel,
		format.Rmask, format.Gmask, format.Bmask, format.Amask);
	for (unsigned y = 0; y < height; ++y) {
		memcpy(static_cast<char*>(surface->pixels) + y * surface->pitch,
		       rowPointers[y], width * format.BytesPerPixel);
	}
	try {
		save(surface, filename);
		SDL_FreeSurface(surface);
	} catch (...) {
		SDL_FreeSurface(surface);
		throw;
	}
}

void save(unsigned width, unsigned height,
          const void** rowPointers, const std::string& filename)
{
	if (!IMG_SavePNG_RW(width, height, rowPointers, filename, true)) {
		throw CommandException("Failed to write " + filename);
	}
}

void saveGrayscale(unsigned width, unsigned height,
	           const void** rowPointers, const std::string& filename)
{
	if (!IMG_SavePNG_RW(width, height, rowPointers, filename, false)) {
		throw CommandException("Failed to write " + filename);
	}
}

} // namespace PNG
} // namespace openmsx
