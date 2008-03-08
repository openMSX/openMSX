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
	byte getAlpha() const { return a; }

	virtual void getProperties(std::set<std::string>& result) const;
	virtual void setProperty(const std::string& name, const std::string& value);
	virtual std::string getProperty(const std::string& name) const;

protected:
	OSDImageBasedWidget(const OSDGUI& gui, const std::string& name);
	virtual void invalidateLocal();
	virtual void getWidthHeight(const OutputSurface& output,
	                            int& width, int& height) const;
	virtual void paintSDL(OutputSurface& output);
	virtual void paintGL (OutputSurface& output);
	virtual BaseImage* createSDL(OutputSurface& output) = 0;
	virtual BaseImage* createGL (OutputSurface& output) = 0;

	void getTransformedXY(const OutputSurface& output,
	                      int& outx, int& outy) const;

	void setError(const std::string& message);
	bool hasError() const { return error; }

protected:
	std::auto_ptr<BaseImage> image;

private:
	void paint(OutputSurface& output, bool openGL);

	const OSDGUI& gui;
	byte r, g, b, a;
	bool error;
};

} // namespace openmsx

#endif
