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
	SDL_Surface *screen = SDL_GetVideoSurface();
	blink = false;
	lastBlinkTime = 0;
	backgroundTexture = 0;
	dispX         = (screen->w / 32);
	dispY         = (screen->h / 15) * 9;
	consoleWidth  = (screen->w / 32) * 30;
	consoleHeight = (screen->h / 15) * 6;
	
	int width, height;
	GLfloat fontTexCoord[4];
	MSXConfig::Config *config = MSXConfig::Backend::instance()->getConfigById("Console");
	std::string fontName = config->getParameter("font");
	GLuint fontTexture = loadTexture(fontName,width, height, fontTexCoord);
	font = new GLFont(fontTexture, width, height, fontTexCoord);

	try {
		int width, height;
		std::string backgroundName = config->getParameter("background");
		backgroundTexture = loadTexture(backgroundName,
		                                width, height, backTexCoord);
	} catch(MSXException &e) {
		// no background or missing file
	}
}

GLConsole::~GLConsole()
{
	delete font;
	glDeleteTextures(1, &backgroundTexture);
}


int GLConsole::powerOfTwo(int a)
{
	int res = 1;
	while (a > res)
		res <<= 1;
	return res;
}

GLuint GLConsole::loadTexture(const std::string &filename, int &width, int &height, GLfloat *texCoord)
{
	SDL_Surface* image1 = IMG_Load(File::findName(filename, CONFIG).c_str());
	if (image1 == NULL)
		throw MSXException("Error loading texture");
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
	if (image2 == NULL)
		throw MSXException("Error loading texture");
	
	SDL_Rect area;
	area.x = 0;
	area.y = 0;
	area.w = width;
	area.h = height;
	SDL_BlitSurface(image1, &area, image2, &area);
	SDL_FreeSurface(image1);

	GLuint texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, 4, w2, h2, 0, GL_RGBA, GL_UNSIGNED_BYTE, image2->pixels);
	SDL_FreeSurface(image2);

	return texture;
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

	int screenlines = consoleHeight / font->height();
	for (int loop=0; loop<screenlines; loop++) {
		int num = loop+consoleScrollBack;
		if (num < lines.size()) {
			glPushMatrix();
			font->drawText(lines[num], CHAR_BORDER,
			               consoleHeight - (1+loop)*font->height());
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
				      CHAR_BORDER + cursorLocation * font->width(),
				      consoleHeight - font->height());
			
		}
	}
	
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glPopAttrib();
}

#endif	// __GLCONSOLE_AVAILABLE__
