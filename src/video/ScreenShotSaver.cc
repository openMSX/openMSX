// $Id$

/* PNG save code by Darren Grant sdl@lokigames.com */
/* heavily modified for openMSX by Joost Damad joost@lumatec.be */

#include <sstream>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <sys/types.h>
#include <png.h>
#include "ScreenShotSaver.hh"
#include "FileOperations.hh"
#include "FileException.hh"
#include "CommandException.hh"
#include "build-info.hh"
#include <SDL.h>

using std::string;

namespace openmsx {

static bool IMG_SavePNG_RW(int width, int height, png_bytep* row_pointers,
                           const string& filename)
{
	FILE* fp = fopen(filename.c_str(), "wb");
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

	png_set_IHDR(png_ptr, info_ptr, width, height, 8,
	             PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
	             PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	// Write the file header information.  REQUIRED
	png_write_info(png_ptr, info_ptr);

	// write out the entire image data in one call
	png_write_image(png_ptr, row_pointers);
	png_write_end(png_ptr, info_ptr);

	if (info_ptr->palette) {
		free(info_ptr->palette);
	}
	png_destroy_write_struct(&png_ptr, &info_ptr);

	fclose(fp);
	return true;

error:
	if (info_ptr->palette) {
		free(info_ptr->palette);
	}
	png_destroy_write_struct(&png_ptr, &info_ptr);

	fclose(fp);
	return false;
}

void ScreenShotSaver::save(SDL_Surface* surface, const string& filename)
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
	png_bytep* row_pointers = (png_bytep*)malloc(sizeof(png_bytep)*surface->h);
	for (int i = 0; i < surface->h; ++i) {
		row_pointers[i] = ((byte*)surf24->pixels) + (i * surf24->pitch);
	}

	bool result = IMG_SavePNG_RW(surface->w, surface->h, row_pointers, filename);

	free(row_pointers);
	SDL_FreeSurface(surf24);

	if (!result) {
		throw CommandException("Failed to write " + filename);
	}
}

void ScreenShotSaver::save(unsigned width, unsigned height,
                 byte** row_pointers, const string& filename)
{
	if (!IMG_SavePNG_RW(width, height, (png_bytep*)row_pointers, filename)) {
		throw CommandException("Failed to write " + filename);
	}
}

} // namespace openmsx
