// $Id$

#include <sstream>
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/types.h>
#include <png.h>
#include "ScreenShotSaver.hh"
#include "FileOperations.hh"
#include "ReadDir.hh"

using std::ostringstream;
using std::max;

/* PNG save code by Darren Grant sdl@lokigames.com */
/* heavily modified for openMSX by Joost Damad joost@lumatec.be */

namespace openmsx {

static bool IMG_SavePNG_RW(int width, int height, png_bytep* row_pointers,
                           const string& filename)
{
	FILE* fp = fopen(filename.c_str(), "wb");
	if (!fp) {
		return false;
	}
	bool result = false;
	png_infop info_ptr = 0;

	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
	        NULL, NULL, NULL);
	if (png_ptr == NULL) {
		// Couldn't allocate memory for PNG file
		goto done;
	}
	png_ptr->io_ptr = fp;

	// Allocate/initialize the image information data.  REQUIRED
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		// Couldn't create image information for PNG file
		goto done;
	}

	// Set error handling.
	if (setjmp(png_ptr->jmpbuf)) {
		// Error writing the PNG file
		goto done;
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
	result = true;  // success!

done:
	if (info_ptr->palette) {
		free(info_ptr->palette);
	}
	png_destroy_write_struct(&png_ptr, &info_ptr);

	fclose(fp);
	return result;
}

void ScreenShotSaver::save(SDL_Surface* surface, const string& filename)
{
	// TODO: save screenshots in specific directory, like HOMEDIR/screenshots
	// Create the array of pointers to image data
	png_bytep* row_pointers = (png_bytep*)malloc(sizeof(png_bytep)*surface->h);

	// TODO: rewrite to use SDL_convertSurface
	for (int i = 0; i < surface->h; ++i) {
		SDL_PixelFormat* fmt = surface->format;
		row_pointers[i] = (Uint8*)malloc(3 * surface->w);
		for (int j = 0; j < surface->w; ++j) {
			Uint32 pixel = *((Uint32*)((Uint8*)surface->pixels +
			                           i * (surface->pitch) +
			                           j * (fmt->BytesPerPixel)));
			Uint32 temp;
			temp = pixel & fmt->Rmask; // Isolate red component
			temp = temp >> fmt->Rshift;// Shift it down to 8-bit
			temp = temp << fmt->Rloss; // Expand to a full 8-bit number
			row_pointers[i][j * 3 + 0] = (Uint8)temp;
			temp = pixel & fmt->Gmask; // Isolate green component
			temp = temp >> fmt->Gshift;// Shift it down to 8-bit
			temp = temp << fmt->Gloss; // Expand to a full 8-bit number
			row_pointers[i][j * 3 + 1] = (Uint8)temp;
			temp = pixel & fmt->Bmask; // Isolate blue component
			temp = temp >> fmt->Bshift;// Shift it down to 8-bit
			temp = temp << fmt->Bloss; // Expand to a full 8-bit number
			row_pointers[i][j * 3 + 2] = (Uint8)temp;
		}
	}

	bool result = IMG_SavePNG_RW(surface->w, surface->h, row_pointers, filename);

	for (int i = 0; i < surface->h; ++i) {
		free(row_pointers[i]);
	}
	free(row_pointers);
	
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


static int getNum(dirent* d)
{
	string name(d->d_name);
	if ((name.length() != 15) ||
	    (name.substr(0, 7) != "openmsx") ||
	    (name.substr(11, 4) != ".png")) {
		return 0;
	}
	string num(name.substr(7, 4));
	char* endpos;
	unsigned long n = strtoul(num.c_str(), &endpos, 10);
	if (*endpos != '\0') {
		return 0;
	}
	return n;
}

string ScreenShotSaver::getFileName()
{
	int max_num = 0;
	
	string dirName = FileOperations::getUserOpenMSXDir() + "/screenshots";
	try {
		FileOperations::mkdirp(dirName);
	} catch (FileException& e) {
		// ignore
	}

	ReadDir dir(dirName.c_str());
	while (dirent* d = dir.getEntry()) {
		max_num = max(max_num, getNum(d));
	}

	ostringstream os;
	os << dirName << "/openmsx";
	os.width(4);
	os.fill('0');
	os << (max_num + 1) << ".png";
	return os.str();
}

} // namespace openmsx
