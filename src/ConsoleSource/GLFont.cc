// $Id$

#include "GLFont.hh"
#ifdef __GLFONT_AVAILABLE__

#include "SDL/SDL.h"
#include "SDL/SDL_image.h"

const int NUM_CHRS = 256;


GLFont::GLFont(GLuint texture, int width, int height, GLfloat *texCoord)
{
	fontTexture = texture;
	charWidth  = width / NUM_CHRS;
	charHeight = height;
	GLfloat texWidth  = texCoord[2];
	GLfloat texHeight = texCoord[3];
	GLfloat texChrWidth  = texWidth / NUM_CHRS;
	GLfloat texChrHeight = texHeight;
	
	listBase = glGenLists(NUM_CHRS);
	for (int i = 0; i < NUM_CHRS; i++) {
		GLfloat x = i * texChrWidth;
		glNewList(listBase + i, GL_COMPILE);
		  glBegin(GL_QUADS);
		    glTexCoord2f(x, 0.0f);				// top left
		    glVertex2i(0, 0);
		    glTexCoord2f(x, texChrHeight);			// bottom left
		    glVertex2i(0, charHeight);
		    glTexCoord2f(x + texChrWidth, texChrHeight);	// bottom right
		    glVertex2i(charWidth, charHeight);
		    glTexCoord2f(x + texChrWidth, 0.0f);		// top right
		    glVertex2i(charWidth, 0);
		  glEnd();
		  glTranslated(charWidth, 0, 0);
		glEndList();
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

int GLFont::height()
{
	return charHeight;
}

int GLFont::width()
{
	return charWidth;
}

#endif	// __GLFONT_AVAILABLE__
