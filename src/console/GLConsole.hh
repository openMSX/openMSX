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

	virtual bool loadFont(const std::string& filename);
	virtual bool loadBackground(const std::string& filename);

	virtual void paint();
	virtual const std::string& getName();

private:
	GLuint backgroundTexture;
	GLfloat backTexCoord[4];
};

} // namespace openmsx

#endif // __GLCONSOLE_HH__
