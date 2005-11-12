// $Id$

#include "GLConsole.hh"
#include "GLFont.hh"
#include "GLImage.hh"
#include "Console.hh"
#include "MSXMotherBoard.hh"
#include "Timer.hh"
#include <cassert>
#include <SDL.h>

using std::string;

namespace openmsx {

GLConsole::GLConsole(MSXMotherBoard& motherBoard)
	: OSDConsoleRenderer(motherBoard)
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

void GLConsole::loadFont(const string& filename)
{
	unsigned width, height;
	GLfloat fontTexCoord[4];
	GLuint fontTexture = GLImage::loadTexture(
		filename, width, height, fontTexCoord);
	font.reset(new GLFont(fontTexture, width, height, fontTexCoord));
}

void GLConsole::loadBackground(const string& filename)
{
	if (filename.empty()) {
		if (backgroundTexture) {
			glDeleteTextures(1, &backgroundTexture);
			backgroundTexture = 0;
		}
		return;
	}
	unsigned dummyWidth, dummyHeight;
	GLuint tmp = GLImage::loadTexture(
		filename, dummyWidth, dummyHeight, backTexCoord);
	if (backgroundTexture) {
		glDeleteTextures(1, &backgroundTexture);
	}
	backgroundTexture = tmp;
}

unsigned GLConsole::getScreenW() const
{
	// TODO refactor
	SDL_Surface* surface = SDL_GetVideoSurface();
	return surface->w;
}

unsigned GLConsole::getScreenH() const
{
	// TODO refactor
	SDL_Surface* surface = SDL_GetVideoSurface();
	return surface->h;
}

void GLConsole::paint()
{
	byte visibility = getVisibility();
	if (!visibility) return;

	glPushAttrib(GL_ENABLE_BIT);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	OSDConsoleRenderer::updateConsoleRect();

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glTranslated(destX, destY, 0);
	// Draw the background image if there is one, otherwise a solid rectangle.
	if (backgroundTexture) {
		glEnable(GL_TEXTURE_2D);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE,
		          (visibility == 255) ? GL_REPLACE : GL_MODULATE);
		glColor4ub(255, 255, 255, visibility);
		glBindTexture(GL_TEXTURE_2D, backgroundTexture);
	} else {
		glDisable(GL_TEXTURE_2D);
		glColor4ub(0, 0, 0, (CONSOLE_ALPHA * visibility) >> 8);
	}
	glBegin(GL_QUADS);
	glTexCoord2f(backTexCoord[0], backTexCoord[1]);
	glVertex2i(0, 0);
	glTexCoord2f(backTexCoord[0], backTexCoord[3]);
	glVertex2i(0, destH);
	glTexCoord2f(backTexCoord[2], backTexCoord[3]);
	glVertex2i(destW, destH);
	glTexCoord2f(backTexCoord[2], backTexCoord[1]);
	glVertex2i(destW, 0);
	glEnd();

	Console& console = getConsole();
	int screenlines = destH / font->getHeight();
	for (int loop = 0; loop < screenlines; loop++) {
		int num = loop + console.getScrollBack();
		glPushMatrix();
		font->drawText(console.getLine(num), CHAR_BORDER,
		               destH - (1 + loop) * font->getHeight(),
		               visibility);
		glPopMatrix();
	}

	// Check if the blink period is over
	if (Timer::getTime() > lastBlinkTime) {
		lastBlinkTime = Timer::getTime() + BLINK_RATE;
		blink = !blink;
	}

	unsigned cursorX, cursorY;
	console.getCursorPosition(cursorX, cursorY);
	if ((cursorX != lastCursorX) || (cursorY != lastCursorY)) {
		blink = true; // force cursor
		lastBlinkTime = Timer::getTime() + BLINK_RATE; // maximum time
		lastCursorX = cursorX;
		lastCursorY = cursorY;
	}
	if (console.getScrollBack() == 0) {
		if (blink) {
			// Print cursor if there is enough room
			font->drawText(string("_"),
				CHAR_BORDER + cursorX * font->getWidth(),
				destH - (font->getHeight() * (cursorY + 1)),
				visibility);

		}
	}

	glPopMatrix();
	glPopAttrib();
}

const string& GLConsole::getName()
{
	static const string NAME = "GLConsole";
	return NAME;
}

} // namespace openmsx

