// $Id$

#include "GLConsole.hh"
#ifdef __GLCONSOLE_AVAILABLE__

#include <cassert>

#ifdef HAVE_SDL_IMAGE_H
#include "SDL_image.h"
#else
#include "SDL/SDL_image.h"
#endif

#include "MSXConfig.hh"
#include "GLFont.hh"
#include "File.hh"


GLConsole::GLConsole()
{
	if (fontName.empty()) {
		font = NULL;
		return;
	}
	
	SDL_Surface *screen = SDL_GetVideoSurface();
	blink = false;
	lastBlinkTime = 0;
	dispX         = (screen->w / 32);
	dispY         = (screen->h / 15) * 9;
	consoleWidth  = (screen->w / 32) * 30;
	consoleHeight = (screen->h / 15) * 6;
	
	// load font
	int width, height;
	GLfloat fontTexCoord[4];
	GLuint fontTexture = 0;
	loadTexture(fontName, fontTexture, width, height, fontTexCoord);
	font = new GLFont(fontTexture, width, height, fontTexCoord);

	// load background
	backgroundTexture = 0;
	backgroundSetting = new BackgroundSetting(this, backgroundName);
}

GLConsole::~GLConsole()
{
	delete backgroundSetting;
	delete font;
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

bool GLConsole::loadBackground(const std::string &filename)
{
	if (filename.empty()) {
		return false;
	}
	int dummyWidth, dummyHeight;
	PRT_DEBUG("GLConsole 1");
	return loadTexture(filename, backgroundTexture,
	                   dummyWidth, dummyHeight, backTexCoord);
}

bool GLConsole::loadTexture(const std::string &filename, GLuint &texture,
                            int &width, int &height, GLfloat *texCoord)
{
	PRT_DEBUG("GLConsole 2");
	File file(filename);
	SDL_Surface* image1 = IMG_Load(file.getLocalName().c_str());
	if (image1 == NULL) {
		return false;
	}
	PRT_DEBUG("GLConsole 3");
	SDL_SetAlpha(image1, 0, 0);
	
	width  = image1->w;
	height = image1->h;
	int w2 = powerOfTwo(width);
	int h2 = powerOfTwo(height);
	texCoord[0] = 0.0f;			// Min X
	texCoord[1] = 0.0f;			// Min Y
	texCoord[2] = (GLfloat)width  / w2;	// Max X
	texCoord[3] = (GLfloat)height / h2;	// Max Y

	PRT_DEBUG("GLConsole 4");
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
	PRT_DEBUG("GLConsole 5");
	
	SDL_Rect area;
	area.x = 0;
	area.y = 0;
	area.w = width;
	area.h = height;
	SDL_BlitSurface(image1, &area, image2, &area);
	SDL_FreeSurface(image1);

	PRT_DEBUG("GLConsole 6");
	if (texture) {
		glDeleteTextures(1, &texture);
	}
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, 4, w2, h2, 0, GL_RGBA, GL_UNSIGNED_BYTE, image2->pixels);
	SDL_FreeSurface(image2);
	PRT_DEBUG("GLConsole 7");
	return true;
}

// Draws the console buffer to the screen
void GLConsole::drawConsole()
{
	if (!isVisible) return;

	glPushAttrib(GL_ENABLE_BIT);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	SDL_Surface *screen = SDL_GetVideoSurface();
	glViewport(0, 0, screen->w, screen->h);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0.0, screen->w, screen->h, 0.0, 0.0, 1.0);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glTranslated(dispX, dispY, 0);
	// draw the background image if there is one
	if (backgroundTexture) {
		glBindTexture(GL_TEXTURE_2D, backgroundTexture);
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
	}

	int screenlines = consoleHeight / font->getHeight();
	for (int loop=0; loop<screenlines; loop++) {
		int num = loop+consoleScrollBack;
		if (num < lines.size()) {
			glPushMatrix();
			font->drawText(lines[num], CHAR_BORDER,
			               consoleHeight - (1+loop)*font->getHeight());
			glPopMatrix();
		}
	}
	
	// Check if the blink period is over
	if (SDL_GetTicks() > lastBlinkTime) {
		lastBlinkTime = SDL_GetTicks() + BLINK_RATE;
		blink = !blink;
	}
	if (consoleScrollBack == 0) {
		if (blink) {
			// Print cursor if there is enough room
			int cursorLocation = lines[0].length();
			font->drawText(std::string("_"), 
			      CHAR_BORDER + cursorLocation * font->getWidth(),
			      consoleHeight - font->getHeight());
			
		}
	}
	
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glPopAttrib();
}

#endif	// __GLCONSOLE_AVAILABLE__
