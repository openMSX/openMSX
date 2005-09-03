// $Id$

#ifndef GLCONSOLE_HH
#define GLCONSOLE_HH

#include "OSDConsoleRenderer.hh"
#include "GLUtil.hh"

namespace openmsx {

class GLConsole : public OSDConsoleRenderer
{
public:
	GLConsole(MSXMotherBoard& motherBoard);
	virtual ~GLConsole();

	virtual void loadFont(const std::string& filename);
	virtual void loadBackground(const std::string& filename);
	virtual unsigned getScreenW() const;
	virtual unsigned getScreenH() const;

	virtual void paint();
	virtual const std::string& getName();

private:
	GLuint backgroundTexture;
	GLfloat backTexCoord[4];
};

} // namespace openmsx

#endif
