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
	uint32_t getRGBA(uint32_t corner) const { return rgba[corner]; }
	const uint32_t* getRGBA4() const { return rgba; }

	virtual uint8_t getFadedAlpha() const = 0;

	std::vector<string_view> getProperties() const override;
	void setProperty(Interpreter& interp,
	                 string_view name, const TclObject& value) override;
	void getProperty(string_view name, TclObject& result) const override;
	float getRecursiveFadeValue() const override;

protected:
	OSDImageBasedWidget(Display& display, const TclObject& name);
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
	void setRGBA(const uint32_t newRGBA[4]);
	bool isFading() const;
	float getCurrentFadeValue() const;
	float getCurrentFadeValue(uint64_t) const;
	void updateCurrentFadeValue();

	void paint(OutputSurface& output, bool openGL);
	gl::vec2 getTransformedPos(const OutputRectangle& output) const;

	uint64_t startFadeTime;
	float fadePeriod;
	float fadeTarget;
	mutable float startFadeValue;
	uint32_t rgba[4];
	bool error;
};

} // namespace openmsx

#endif
