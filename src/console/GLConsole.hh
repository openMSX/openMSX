// $Id$

#ifndef __GLCONSOLE_HH__
#define __GLCONSOLE_HH__

#include "OSDConsoleRenderer.hh"
#include "GLUtil.hh"

namespace openmsx {

class GLConsole : public OSDConsoleRenderer
{
public:
	GLConsole(Console& console);
	virtual ~GLConsole();

	virtual bool loadFont(const string& filename);
	virtual bool loadBackground(const string& filename);

	virtual void paint();
	virtual const string& getName();

private:
	int powerOfTwo(int a);
	bool loadTexture(const string& filename, GLuint& texture,
	                 int& width, int& height, GLfloat* texCoord);

	GLuint backgroundTexture;
	GLfloat backTexCoord[4];
};

} // namespace openmsx

#endif // __GLCONSOLE_HH__
