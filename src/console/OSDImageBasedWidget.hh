#ifndef OSDIMAGEBASEDWIDGET_HH
#define OSDIMAGEBASEDWIDGET_HH

#include "OSDWidget.hh"
#include <cstdint>

namespace openmsx {

class BaseImage;
class Display;

class OSDImageBasedWidget : public OSDWidget
{
public:
	unsigned getRGBA(unsigned corner) const { return rgba[corner]; }
	const unsigned* getRGBA4() const { return rgba; }

	virtual uint8_t getFadedAlpha() const = 0;

	std::vector<string_ref> getProperties() const override;
	void setProperty(Interpreter& interp,
	                 string_ref name, const TclObject& value) override;
	void getProperty(string_ref name, TclObject& result) const override;
	float getRecursiveFadeValue() const override;

protected:
	OSDImageBasedWidget(Display& display, const TclObject& name);
	~OSDImageBasedWidget();
	bool hasConstantAlpha() const;
	void createImage(OutputSurface& output);
	void invalidateLocal() override;
	void paintSDL(OutputSurface& output) override;
	void paintGL (OutputSurface& output) override;
	virtual std::unique_ptr<BaseImage> createSDL(OutputSurface& output) = 0;
	virtual std::unique_ptr<BaseImage> createGL (OutputSurface& output) = 0;

	void setError(std::string message);
	bool hasError() const { return error; }

	std::unique_ptr<BaseImage> image;

private:
	void setRGBA(const unsigned newRGBA[4]);
	bool isFading() const;
	float getCurrentFadeValue() const;
	float getCurrentFadeValue(uint64_t) const;
	void updateCurrentFadeValue();

	void paint(OutputSurface& output, bool openGL);
	gl::vec2 getTransformedPos(const OutputSurface& output) const;

	uint64_t startFadeTime;
	float fadePeriod;
	float fadeTarget;
	mutable float startFadeValue;
	unsigned rgba[4];
	bool error;
};

} // namespace openmsx

#endif
