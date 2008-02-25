// $Id$

#ifndef OSDRECTANGLE_HH
#define OSDRECTANGLE_HH

#include "OSDWidget.hh"
#include "openmsx.hh"
#include <memory>

namespace openmsx {

class BaseImage;

class OSDRectangle : public OSDWidget
{
public:
	OSDRectangle(OSDGUI& gui);

	virtual void getProperties(std::set<std::string>& result) const;
	virtual void setProperty(const std::string& name, const std::string& value);
	virtual std::string getProperty(const std::string& name) const;
	virtual std::string getType() const;

	virtual void paintSDL(OutputSurface& output);
	virtual void paintGL (OutputSurface& output);
	virtual void invalidate();

private:
	template <typename IMAGE> void paint(OutputSurface& output);

	std::string imageName;
	int x, y, w, h;
	std::auto_ptr<BaseImage> image;
};

} // namespace openmsx

#endif
