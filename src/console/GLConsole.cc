// $Id$

#include <cassert>
#include <SDL_image.h>
#include "GLConsole.hh"
#include "DummyFont.hh"
#include "GLFont.hh"
#include "File.hh"
#include "Console.hh"
#include "CliCommOutput.hh"
#include "GLImage.hh"


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

bool GLConsole::loadFont(const string& filename)
{
	if (filename.empty()) {
		return false;
	}
	unsigned width, height;
	GLfloat fontTexCoord[4];
	GLuint fontTexture = GLImage::loadTexture(
		filename, width, height, fontTexCoord);
	if (fontTexture) {
		font.reset(new GLFont(fontTexture, width, height, fontTexCoord));
		return true;
	} else {
		return false;
	}
}

bool GLConsole::loadBackground(const string& filename)
{
	if (filename.empty()) {
		if (backgroundTexture) {
			glDeleteTextures(1, &backgroundTexture);
			backgroundTexture = 0;
		}
		return true;
	}
	unsigned dummyWidth, dummyHeight;
	GLuint tmp = GLImage::loadTexture(
		filename, dummyWidth, dummyHeight, backTexCoord);
	if (tmp) {
		if (backgroundTexture) {
			glDeleteTextures(1, &backgroundTexture);
		}
		backgroundTexture = tmp;
	}
	return tmp;
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

