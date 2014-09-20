#ifndef OSDIMAGEBASEDWIDGET_HH
#define OSDIMAGEBASEDWIDGET_HH

#include "OSDWidget.hh"
#include "openmsx.hh"
#include <cstdint>

namespace openmsx {

class OSDGUI;
class BaseImage;

class OSDImageBasedWidget : public OSDWidget
{
public:
	unsigned getRGBA(unsigned corner) const { return rgba[corner]; }
	const unsigned* getRGBA4() const { return rgba; }

	virtual byte getFadedAlpha() const = 0;

	std::vector<string_ref> getProperties() const override;
	void setProperty(Interpreter& interp,
	                 string_ref name, const TclObject& value) override;
	void getProperty(string_ref name, TclObject& result) const override;
	double getRecursiveFadeValue() const override;

protected:
	OSDImageBasedWidget(const OSDGUI& gui, const std::string& name);
	~OSDImageBasedWidget();
	bool hasConstantAlpha() const;
	void createImage(OutputRectangle& output);
	void invalidateLocal() override;
	void paintSDL(OutputSurface& output) override;
	void paintGL (OutputSurface& output) override;
	virtual std::unique_ptr<BaseImage> createSDL(OutputRectangle& output) = 0;
	virtual std::unique_ptr<BaseImage> createGL (OutputRectangle& output) = 0;

	void setError(std::string message);
	bool hasError() const { return error; }

	std::unique_ptr<BaseImage> image;

private:
	void setRGBA(const unsigned newRGBA[4]);
	bool isFading() const;
	double getCurrentFadeValue() const;
	double getCurrentFadeValue(uint64_t) const;
	void updateCurrentFadeValue();

	void paint(OutputSurface& output, bool openGL);
	void getTransformedXY(const OutputRectangle& output,
	                      double& outx, double& outy) const;

	const OSDGUI& gui;
	uint64_t startFadeTime;
	double fadePeriod;
	double fadeTarget;
	mutable double startFadeValue;
	unsigned rgba[4];
	bool error;
};

} // namespace openmsx

#endif
