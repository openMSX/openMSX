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
	virtual void setProperty(string_ref name, const TclObject& value);
	virtual void getProperty(string_ref name, TclObject& result) const;
	virtual string_ref getType() const;

private:
	bool takeImageDimensions() const;

	virtual void getWidthHeight(const OutputRectangle& output,
	                            double& width, double& height) const;
	virtual byte getFadedAlpha() const;
	virtual BaseImage* createSDL(OutputRectangle& output);
	virtual BaseImage* createGL (OutputRectangle& output);
	template <typename IMAGE> BaseImage* create(OutputRectangle& output);

	std::string imageName;
	double w, h, relw, relh, scale, borderSize, relBorderSize;
	unsigned borderRGBA;
};

} // namespace openmsx

#endif
