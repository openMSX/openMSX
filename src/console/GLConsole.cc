// $Id$

#include "GLConsole.hh"

#ifdef __OPENGL_AVAILABLE__

#include <cassert>

#include "MSXConfig.hh"
#include "DummyFont.hh"
#include "GLFont.hh"
#include "File.hh"
#include "Console.hh"


namespace openmsx {

GLConsole::GLConsole(Console * console_)
	:OSDConsoleRenderer (console_)
{
	console = console_;
	std::string temp = console->getId();
	fontSetting = new FontSetting(this, temp+"font", fontName);
	initConsoleSize();
	
	SDL_Rect rect;
	OSDConsoleRenderer::updateConsoleRect(rect);
	dispX = rect.x;
	dispY = rect.y;
	consoleHeight = rect.h;
	consoleWidth = rect.w;
	
	// load background
	backgroundTexture = 0;
	
	backgroundSetting = new BackgroundSetting(this, temp + "background", backgroundName);
}

GLConsole::~GLConsole()
{
	delete backgroundSetting;
	delete fontSetting;
	if (backgroundTexture) {
		glDeleteTextures(1, &backgroundTexture);
	}
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
		delete font;
		font = new GLFont(fontTexture, width, height, fontTexCoord);
		return true;
	} else {
		return false;
	}
}

bool GLConsole::loadBackground(const string &filename)
{
	if (filename.empty()) {
		return false;
	}
	int dummyWidth, dummyHeight;
	return loadTexture(filename, backgroundTexture,
		dummyWidth, dummyHeight, backTexCoord);
}

bool GLConsole::loadTexture(const string &filename, GLuint &texture,
		int &width, int &height, GLfloat *texCoord)
{
	SDL_Surface *image1;
	try {
		File file(filename);
		image1 = IMG_Load(file.getLocalName().c_str());
		if (image1 == NULL) {
			PRT_INFO("File \"" << file.getURL() << "\" is not a valid image");
			return false;
		}
	} catch (FileException &e) {
		PRT_INFO(
			"Could not open file \"" << filename << "\": " << e.getMessage()
			);
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

void GLConsole::updateConsole()
{
}

void GLConsole::updateConsoleRect(SDL_Surface *screen)
{
	SDL_Rect rect;
	OSDConsoleRenderer::updateConsoleRect(rect);
	dispX = rect.x;
	dispY = rect.y;
	consoleHeight = rect.h;
	consoleWidth = rect.w;
}

// Draws the console buffer to the screen
void GLConsole::drawConsole()
{
	if (!console->isVisible()) {
		return;
	}

	glPushAttrib(GL_ENABLE_BIT);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	SDL_Surface *screen = SDL_GetVideoSurface();

	updateConsoleRect(screen);

	glViewport(0, 0, screen->w, screen->h);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0.0, screen->w, screen->h, 0.0, 0.0, 1.0);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glTranslated(dispX, dispY, 0);
	// Draw the background image if there is one, otherwise a solid rectangle.
	if (backgroundTexture) {
		glEnable(GL_TEXTURE_2D);
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		glBindTexture(GL_TEXTURE_2D, backgroundTexture);
	} else {
		glDisable(GL_TEXTURE_2D);
		glColor4ub(0, 0, 0, CONSOLE_ALPHA);
	}
	glBegin(GL_QUADS);
	glTexCoord2f(backTexCoord[0], backTexCoord[1]);
	glVertex2i(0, 0);
	glTexCoord2f(backTexCoord[0], backTexCoord[3]);
	glVertex2i(0, consoleHeight);
	glTexCoord2f(backTexCoord[2], backTexCoord[3]);
	glVertex2i(consoleWidth, consoleHeight);
	glTexCoord2f(backTexCoord[2], backTexCoord[1]);
	glVertex2i(consoleWidth, 0);
	glEnd();

	glEnable(GL_TEXTURE_2D);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	int screenlines = consoleHeight / font->getHeight();
	for (int loop = 0; loop < screenlines; loop++) {
		int num = loop + console->getScrollBack();
		glPushMatrix();
		font->drawText(console->getLine(num), CHAR_BORDER,
		               consoleHeight - (1 + loop) * font->getHeight());
		glPopMatrix();
	}

	// Check if the blink period is over
	if (SDL_GetTicks() > lastBlinkTime) {
		lastBlinkTime = SDL_GetTicks() + BLINK_RATE;
		blink = !blink;
	}
	int cursorX;
	int cursorY;	 
	
	console->getCursorPosition(&cursorX, &cursorY);
	if (cursorX != lastCursorPosition){
		blink=true; // force cursor
		lastBlinkTime=SDL_GetTicks() + BLINK_RATE; // maximum time
		lastCursorPosition=cursorX;
	}
	if (console->getScrollBack() == 0) {
		if (blink) {
			// Print cursor if there is enough room
			font->drawText(string("_"),
				CHAR_BORDER + cursorX * font->getWidth(),
				consoleHeight - (font->getHeight()* (cursorY+1)));

		}
	}

	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glPopAttrib();
}

} // namespace openmsx

#endif // __OPENGL_AVAILABLE__
