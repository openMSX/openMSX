// $Id$

#include "GLFont.hh"
#ifdef __GLFONT_AVAILABLE__

#include "SDL/SDL.h"

#ifdef HAVE_SDL_IMAGE_H
#include "SDL_image.h"
#else
#include "SDL/SDL_image.h"
#endif

const int NUM_CHRS = 256;
const int CHARS_PER_ROW = 16;
const int CHARS_PER_COL = NUM_CHRS / CHARS_PER_ROW;


GLFont::GLFont(GLuint texture, int width, int height, GLfloat *texCoord)
{
	fontTexture = texture;
	charWidth  = width  / CHARS_PER_ROW;
	charHeight = height / CHARS_PER_COL;
	GLfloat texWidth  = texCoord[2];
	GLfloat texHeight = texCoord[3];
	GLfloat texChrWidth  = texWidth  / CHARS_PER_ROW;
	GLfloat texChrHeight = texHeight / CHARS_PER_COL;
	
	listBase = glGenLists(NUM_CHRS);
	for (int v = 0; v < CHARS_PER_COL; v++) {
		for (int u = 0; u < NUM_CHRS; u++) {
			int n = u + CHARS_PER_ROW * v;
			GLfloat x = u * texChrWidth;
			GLfloat y = v * texChrHeight;
			glNewList(listBase + n, GL_COMPILE);
			  glBegin(GL_QUADS);
			    glTexCoord2f(x, y);					// top left
			    glVertex2i(0, 0);
			    glTexCoord2f(x, y + texChrHeight);			// bottom left
			    glVertex2i(0, charHeight);
			    glTexCoord2f(x + texChrWidth, y + texChrHeight);	// bottom right
			    glVertex2i(charWidth, charHeight);
			    glTexCoord2f(x + texChrWidth, y);			// top right
			    glVertex2i(charWidth, 0);
			  glEnd();
			  glTranslated(charWidth, 0, 0);
			glEndList();
		}
	}
}

GLFont::~GLFont()
{
	glDeleteLists(listBase, NUM_CHRS);
	glDeleteTextures(1, &fontTexture);
}


void GLFont::drawText(const std::string &string, int x, int y)
{
	glBindTexture(GL_TEXTURE_2D, fontTexture);
	glTranslated(x, y, 0);
	const char* text = string.c_str();
	glListBase(listBase);
	glCallLists(strlen(text), GL_UNSIGNED_BYTE, text);
}

#endif	// __GLFONT_AVAILABLE__
