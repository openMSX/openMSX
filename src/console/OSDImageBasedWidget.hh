// $Id$

#ifndef OSDIMAGEBASEDWIDGET_HH
#define OSDIMAGEBASEDWIDGET_HH

#include "OSDWidget.hh"

namespace openmsx {

class OSDGUI;
class BaseImage;

class OSDImageBasedWidget : public OSDWidget
{
public:
	virtual ~OSDImageBasedWidget();

	byte getRed()   const { return r; }
	byte getGreen() const { return g; }
	byte getBlue()  const { return b; }
	byte getAlpha() const; // returns faded value

	virtual void getProperties(std::set<std::string>& result) const;
	virtual void setProperty(const std::string& name, const std::string& value);
	virtual std::string getProperty(const std::string& name) const;

protected:
	OSDImageBasedWidget(const OSDGUI& gui, const std::string& name);
	virtual void invalidateLocal();
	virtual void getWidthHeight(const OutputSurface& output,
	                            double& width, double& height) const;
	virtual void paintSDL(OutputSurface& output);
	virtual void paintGL (OutputSurface& output);
	virtual BaseImage* createSDL(OutputSurface& output) = 0;
	virtual BaseImage* createGL (OutputSurface& output) = 0;

	void setError(const std::string& message);
	bool hasError() const { return error; }

	std::auto_ptr<BaseImage> image;

private:
	bool isFading() const;
	void setAlpha(byte alpha);
	void setAlpha(byte alpha, unsigned long long now);
	byte getAlpha(unsigned long long now) const;
	void paint(OutputSurface& output, bool openGL);
	void getTransformedXY(const OutputSurface& output,
	                      double& outx, double& outy) const;

	const OSDGUI& gui;
	unsigned long long setFadeTime;
	double fadePeriod;
	byte fadeTarget;
	byte r, g, b;
	mutable byte a;
	bool error;
};

} // namespace openmsx

#endif
