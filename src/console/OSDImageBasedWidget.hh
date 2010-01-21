// $Id$

#ifndef OSDIMAGEBASEDWIDGET_HH
#define OSDIMAGEBASEDWIDGET_HH

#include "OSDWidget.hh"
#include "openmsx.hh"

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
	byte getAlpha() const { return a; }

	byte getFadedAlpha() const;

	virtual void getProperties(std::set<std::string>& result) const;
	virtual void setProperty(const std::string& name, const TclObject& value);
	virtual void getProperty(const std::string& name, TclObject& result) const;
	virtual double getRecursiveFadeValue() const;

protected:
	OSDImageBasedWidget(const OSDGUI& gui, const std::string& name);
	virtual void invalidateLocal();
	virtual void paintSDL(OutputSurface& output);
	virtual void paintGL (OutputSurface& output);
	virtual BaseImage* createSDL(OutputSurface& output) = 0;
	virtual BaseImage* createGL (OutputSurface& output) = 0;

	void setError(const std::string& message);
	bool hasError() const { return error; }

	std::auto_ptr<BaseImage> image;

private:
	bool isFading() const;
	double getCurrentFadeValue() const;
	double getCurrentFadeValue(unsigned long long) const;
	void updateCurrentFadeValue();

	void paint(OutputSurface& output, bool openGL);
	void getTransformedXY(const OutputSurface& output,
	                      double& outx, double& outy) const;

	const OSDGUI& gui;
	unsigned long long startFadeTime;
	double fadePeriod;
	double fadeTarget;
	mutable double startFadeValue;
	byte r, g, b, a;
	bool error;
};

} // namespace openmsx

#endif
