// $Id$

#include "GLFont.hh"

namespace openmsx {

const int NUM_CHRS = 256;
const int CHARS_PER_ROW = 16;
const int CHARS_PER_COL = NUM_CHRS / CHARS_PER_ROW;


GLFont::GLFont(GLuint texture, int width, int height, GLfloat* texCoord)
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
			    glTexCoord2f(x, y); // top left
			    glVertex2i(0, 0);
			    glTexCoord2f(x, y + texChrHeight); // bottom left
			    glVertex2i(0, charHeight);
			    glTexCoord2f(x + texChrWidth, y + texChrHeight); // bottom right
			    glVertex2i(charWidth, charHeight);
			    glTexCoord2f(x + texChrWidth, y); // top right
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

void GLFont::drawText(const std::string& str, int x, int y, byte alpha)
{
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE,
	          (alpha == 255) ? GL_REPLACE : GL_MODULATE);
	glColor4ub(255, 255, 255, alpha);
	glBindTexture(GL_TEXTURE_2D, fontTexture);
	glTranslated(x, y, 0);
	const char* text = str.c_str();
	glListBase(listBase);
	glCallLists(strlen(text), GL_UNSIGNED_BYTE, text);
}

} // namespace openmsx

