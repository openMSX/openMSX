// $Id$

#include <png.h>

#include "ScreenShotSaver.hh"
#include "MSXException.hh"
#include "File.hh"
#include <SDL/SDL.h>

/* PNG save code by Darren Grant sdl@lokigames.com */
/* heavily modified for openMSX by Joost Damad joost@lumatec.be */

/* Save a PNG type image to an SDL datasource */
static void png_write_data(png_structp ctx, png_bytep area, png_size_t size)
{
	SDL_RWops *src;

	src = (SDL_RWops *)png_get_io_ptr(ctx);
	SDL_RWwrite(src, area, size, 1);
}

static void png_io_flush(png_structp ctx)
{
	SDL_RWops *src;

	src = (SDL_RWops *)png_get_io_ptr(ctx);
	/* how do I flush src? */
}

static void png_user_warn(png_structp ctx, png_const_charp str)
{
	fprintf(stderr, "libpng: warning: %s\n", str);
}

static void png_user_error(png_structp ctx, png_const_charp str)
{
	fprintf(stderr, "libpng: error: %s\n", str);
}

static int IMG_SavePNG_RW(SDL_Surface *surface, SDL_RWops *src)
{
	png_structp png_ptr = 0;
	png_infop info_ptr = 0;
	png_bytep *row_pointers = 0;
	int i;
	int colortype;
	int result = -1;

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, png_user_error, png_user_warn);

	if (png_ptr == NULL)
	{
		//IMG_SetError("Couldn't allocate memory for PNG file");
		return -1;
	}

	/* Allocate/initialize the image information data.  REQUIRED */
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL)
	{
		//IMG_SetError("Couldn't create image information for PNG file");
		goto done;
	}

	/* Set error handling. */
	if (setjmp(png_ptr->jmpbuf))
	{
		/* If we get here, we had a problem reading the file */
		//IMG_SetError("Error writing the PNG file");
		goto done;
	}

	png_set_write_fn(png_ptr, src, png_write_data, png_io_flush);

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
	
	//fprintf(stderr, "Bits per pixel: %d\n Width: %d Height: %d Pitch: %d\n", surface->format->BitsPerPixel, surface->w, surface->h, surface->pitch);
	
	colortype = PNG_COLOR_TYPE_RGB;
	png_set_IHDR(png_ptr, info_ptr, surface->w, surface->h, 8,
		colortype, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	/* Write the file header information.  REQUIRED */
	png_write_info(png_ptr, info_ptr);

	/* Create the array of pointers to image data */
	row_pointers = (png_bytep*) malloc(sizeof(png_bytep)*surface->h);

	for (i = 0; i < surface->h; i++)
	{
		SDL_PixelFormat *fmt = surface->format;
		Uint32 temp, pixel;
		row_pointers[i] = (Uint8*) malloc( 3*surface->w );
		for (int j=0; j < surface->w; j++)
		{
			pixel=*((Uint16*)
				(
					(Uint8*)surface->pixels +
					i*(surface->pitch) +
					j*(fmt->BytesPerPixel))
				);
			temp=pixel&fmt->Rmask; /* Isolate red component */
			temp=temp>>fmt->Rshift;/* Shift it down to 8-bit */
			temp=temp<<fmt->Rloss; /* Expand to a full 8-bit number */
			row_pointers[i][j*3]=(Uint8)temp;
			temp=pixel&fmt->Gmask; /* Isolate green component */
			temp=temp>>fmt->Gshift;/* Shift it down to 8-bit */
			temp=temp<<fmt->Gloss; /* Expand to a full 8-bit number */
			row_pointers[i][j*3+1]=(Uint8)temp;
			temp=pixel&fmt->Bmask; /* Isolate blue component */
			temp=temp>>fmt->Bshift;/* Shift it down to 8-bit */
			temp=temp<<fmt->Bloss; /* Expand to a full 8-bit number */
			row_pointers[i][j*3+2]=(Uint8)temp;
		}
	}
	/* write out the entire image data in one call */
	png_write_image(png_ptr, row_pointers);
	png_write_end(png_ptr, info_ptr);
	result = 0;  /* success! */

done:
	if (row_pointers)
	{
		for (i = 0; i < surface->h; i++)
		{
			free(row_pointers[i]);
		}
		free(row_pointers);
	}
	if (info_ptr->palette)
		free(info_ptr->palette);

	png_destroy_write_struct(&png_ptr, (png_infopp)NULL);

	return result;
}

namespace openmsx {

ScreenShotSaver::ScreenShotSaver(SDL_Surface* image)
	:image(image)
{
}

void ScreenShotSaver::take(const string& filename)
{
	FILE* fp = fopen(filename.c_str(), "wb");

	SDL_RWops* foo = SDL_RWFromFP(fp, 1);
	if (IMG_SavePNG_RW(image, foo) < 0) {
		throw MSXException("Failed to write " + filename);
	}
	fclose(fp);
}

} // namespace openmsx
