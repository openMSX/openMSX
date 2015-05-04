#ifndef OSDRECTANGLE_HH
#define OSDRECTANGLE_HH

#include "OSDImageBasedWidget.hh"
#include "openmsx.hh"
#include <memory>

namespace openmsx {

class BaseImage;

class OSDRectangle final : public OSDImageBasedWidget
{
public:
	OSDRectangle(OSDGUI& gui, const std::string& name);

	std::vector<string_ref> getProperties() const override;
	void setProperty(Interpreter& interp,
	                 string_ref name, const TclObject& value) override;
	void getProperty(string_ref name, TclObject& result) const override;
	string_ref getType() const override;

private:
	bool takeImageDimensions() const;

	void getWidthHeight(const OutputRectangle& output,
	                    float& width, float& height) const override;
	byte getFadedAlpha() const override;
	std::unique_ptr<BaseImage> createSDL(OutputRectangle& output) override;
	std::unique_ptr<BaseImage> createGL (OutputRectangle& output) override;
	template <typename IMAGE> std::unique_ptr<BaseImage> create(
		OutputRectangle& output);

	std::string imageName;
	float w, h, relw, relh, scale, borderSize, relBorderSize;
	unsigned borderRGBA;
};

} // namespace openmsx

#endif
