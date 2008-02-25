// $Id$

#ifndef OSDTEXT_HH
#define OSDTEXT_HH

#include "OSDWidget.hh"
#include "openmsx.hh"
#include <memory>

namespace openmsx {

class BaseImage;

class OSDText : public OSDWidget
{
public:
	OSDText(OSDGUI& gui);

	virtual void getProperties(std::set<std::string>& result) const;
	virtual void setProperty(const std::string& name, const std::string& value);
	virtual std::string getProperty(const std::string& name) const;
	virtual std::string getType() const;

	virtual void paintSDL(OutputSurface& output);
	virtual void paintGL (OutputSurface& output);
	virtual void invalidate();

private:
	template <typename IMAGE> void paint(OutputSurface& output);

	std::string text;
	std::string fontfile;
	int x, y, size;
	byte r, g, b, a;
	std::auto_ptr<BaseImage> image;
};

} // namespace openmsx

#endif
