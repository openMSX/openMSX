// $Id$

#include <cassert>
#include "Display.hh"
#include "GLConsole.hh"
#include "GLFont.hh"
#include "Console.hh"
#include "GLImage.hh"

using std::string;

namespace openmsx {

GLConsole::GLConsole(Console& console_)
	: OSDConsoleRenderer(console_)
	, backgroundTexture(0)
{
	initConsole();
	Display::instance().addLayer(this);
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

void GLConsole::paint()
{
	byte visibility = getVisibility();
	if (!visibility) return;
	
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
	glVertex2i(0, destRect.h);
	glTexCoord2f(backTexCoord[2], backTexCoord[3]);
	glVertex2i(destRect.w, destRect.h);
	glTexCoord2f(backTexCoord[2], backTexCoord[1]);
	glVertex2i(destRect.w, 0);
	glEnd();

	int screenlines = destRect.h / font->getHeight();
	for (int loop = 0; loop < screenlines; loop++) {
		int num = loop + console.getScrollBack();
		glPushMatrix();
		font->drawText(console.getLine(num), CHAR_BORDER,
		               destRect.h - (1 + loop) * font->getHeight(),
		               visibility);
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
				destRect.h - (font->getHeight() * (cursorY + 1)),
				visibility);

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

