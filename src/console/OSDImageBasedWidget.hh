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

	byte   getRed()   const { return r; }
	byte   getGreen() const { return g; }
	byte   getBlue()  const { return b; }
	byte   getAlpha() const { return a; }
	int    getX()     const { return x; }
	int    getY()     const { return y; }
	double getRelX()  const { return relx; }
	double getRelY()  const { return rely; }

	virtual void getProperties(std::set<std::string>& result) const;
	virtual void setProperty(const std::string& name, const std::string& value);
	virtual std::string getProperty(const std::string& name) const;

	virtual void transformXY(const OutputSurface& output,
	                         int x, int y, double relx, double rely,
	                         int& outx, int& outy) const;

protected:
	OSDImageBasedWidget(const OSDGUI& gui, const std::string& name);
	virtual void invalidateLocal();
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
	double relx, rely;
	int x, y;
	byte r, g, b, a;
	bool error;
};

} // namespace openmsx

#endif
