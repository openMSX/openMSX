// $Id$

#include <cassert>
#include <SDL_image.h>
#include "GLConsole.hh"
#include "DummyFont.hh"
#include "GLFont.hh"
#include "File.hh"
#include "Console.hh"
#include "CliCommOutput.hh"


namespace openmsx {

GLConsole::GLConsole(Console& console_)
	: OSDConsoleRenderer(console_)
	, backgroundTexture(0)
{
	initConsole();
}

GLConsole::~GLConsole()
{
	if (backgroundTexture) {
		glDeleteTextures(1, &backgroundTexture);
	}
	backgroundSetting.reset();
}


int GLConsole::powerOfTwo(int a)
{
	int res = 1;
	while (a > res) {
		res <<= 1;
	}
	return res;
}

bool GLConsole::loadFont(const string &filename)
{
	if (filename.empty()) {
		return false;
	}
	int width, height;
	GLfloat fontTexCoord[4];
	GLuint fontTexture = 0;
	if (loadTexture(filename, fontTexture, width, height, fontTexCoord)) {
		font.reset(new GLFont(fontTexture, width, height, fontTexCoord));
		return true;
	} else {
		return false;
	}
}

bool GLConsole::loadBackground(const string &filename)
{
	if (filename.empty()) {
		if (backgroundTexture) {
			glDeleteTextures(1, &backgroundTexture);
		}
		backgroundTexture = 0;
		return true;
	} else {
		int dummyWidth, dummyHeight;
		return loadTexture(filename, backgroundTexture,
			dummyWidth, dummyHeight, backTexCoord);
	}
}

bool GLConsole::loadTexture(const string &filename, GLuint &texture,
		int &width, int &height, GLfloat *texCoord)
{
	SDL_Surface* image1;
	try {
		File file(filename);
		image1 = IMG_Load(file.getLocalName().c_str());
		if (image1 == NULL) {
			CliCommOutput::instance().printWarning("File \"" +
			        file.getURL() + "\" is not a valid image");
			return false;
		}
	} catch (FileException &e) {
		CliCommOutput::instance().printWarning("Could not open file \"" +
		        filename + "\": " + e.getMessage());
		return false;
	}
	SDL_SetAlpha(image1, 0, 0);

	width  = image1->w;
	height = image1->h;
	int w2 = powerOfTwo(width);
	int h2 = powerOfTwo(height);
	texCoord[0] = 0.0f;			// Min X
	texCoord[1] = 0.0f;			// Min Y
	texCoord[2] = (GLfloat)width  / w2;	// Max X
	texCoord[3] = (GLfloat)height / h2;	// Max Y

	SDL_Surface* image2 = SDL_CreateRGBSurface(SDL_SWSURFACE, w2, h2, 32,
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
		0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000
#else
		0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF
#endif
	);
	if (image2 == NULL) {
		return false;
	}

	SDL_Rect area;
	area.x = 0;
	area.y = 0;
	area.w = width;
	area.h = height;
	SDL_BlitSurface(image1, &area, image2, &area);
	SDL_FreeSurface(image1);

	if (texture) {
		glDeleteTextures(1, &texture);
	}
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, 4, w2, h2, 0, GL_RGBA, GL_UNSIGNED_BYTE, image2->pixels);
	SDL_FreeSurface(image2);
	return true;
}

void GLConsole::paint()
{
	glPushAttrib(GL_ENABLE_BIT);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	SDL_Surface* screen = SDL_GetVideoSurface();

	OSDConsoleRenderer::updateConsoleRect(destRect);

	glViewport(0, 0, screen->w, screen->h);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0.0, screen->w, screen->h, 0.0, 0.0, 1.0);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glTranslated(destRect.x, destRect.y, 0);
	// Draw the background image if there is one, otherwise a solid rectangle.
	if (backgroundTexture) {
		glEnable(GL_TEXTURE_2D);
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		glBindTexture(GL_TEXTURE_2D, backgroundTexture);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	} else {
		glDisable(GL_TEXTURE_2D);
		glColor4ub(0, 0, 0, CONSOLE_ALPHA);
	}
	glBegin(GL_QUADS);
	glTexCoord2f(backTexCoord[0], backTexCoord[1]);
	glVertex2i(0, 0);
	glTexCoord2f(backTexCoord[0], backTexCoord[3]);
	glVertex2i(0, destRect.h);
	glTexCoord2f(backTexCoord[2], backTexCoord[3]);
	glVertex2i(destRect.w, destRect.h);
	glTexCoord2f(backTexCoord[2], backTexCoord[1]);
	glVertex2i(destRect.w, 0);
	glEnd();

	glEnable(GL_TEXTURE_2D);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	int screenlines = destRect.h / font->getHeight();
	for (int loop = 0; loop < screenlines; loop++) {
		int num = loop + console.getScrollBack();
		glPushMatrix();
		font->drawText(console.getLine(num), CHAR_BORDER,
		               destRect.h - (1 + loop) * font->getHeight());
		glPopMatrix();
	}

	// Check if the blink period is over
	if (SDL_GetTicks() > lastBlinkTime) {
		lastBlinkTime = SDL_GetTicks() + BLINK_RATE;
		blink = !blink;
	}
	
	unsigned cursorX;
	unsigned cursorY;
	console.getCursorPosition(cursorX, cursorY);
	if (cursorX != lastCursorPosition) {
		blink = true; // force cursor
		lastBlinkTime=SDL_GetTicks() + BLINK_RATE; // maximum time
		lastCursorPosition = cursorX;
	}
	if (console.getScrollBack() == 0) {
		if (blink) {
			// Print cursor if there is enough room
			font->drawText(string("_"),
				CHAR_BORDER + cursorX * font->getWidth(),
				destRect.h - (font->getHeight() * (cursorY + 1)));

		}
	}

	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glPopAttrib();
}

const string& GLConsole::getName()
{
	static const string NAME = "GLConsole";
	return NAME;
}

} // namespace openmsx

