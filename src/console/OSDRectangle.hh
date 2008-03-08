// $Id$

#ifndef OSDRECTANGLE_HH
#define OSDRECTANGLE_HH

#include "OSDImageBasedWidget.hh"
#include "openmsx.hh"
#include <memory>

namespace openmsx {

class BaseImage;

class OSDRectangle : public OSDImageBasedWidget
{
public:
	OSDRectangle(const OSDGUI& gui, const std::string& name);

	virtual void getProperties(std::set<std::string>& result) const;
	virtual void setProperty(const std::string& name, const std::string& value);
	virtual std::string getProperty(const std::string& name) const;
	virtual std::string getType() const;

private:
	virtual void getWidthHeight(const OutputSurface& output,
	                            int& width, int& height) const;
	virtual BaseImage* createSDL(OutputSurface& output);
	virtual BaseImage* createGL (OutputSurface& output);
	template <typename IMAGE> BaseImage* create(OutputSurface& output);

	std::string imageName;
	int w, h;
};

} // namespace openmsx

#endif
