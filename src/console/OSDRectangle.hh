#ifndef OSDRECTANGLE_HH
#define OSDRECTANGLE_HH

#include "OSDImageBasedWidget.hh"
#include <memory>

namespace openmsx {

class BaseImage;

class OSDRectangle final : public OSDImageBasedWidget
{
public:
	OSDRectangle(Display& display, const TclObject& name);

	std::vector<std::string_view> getProperties() const override;
	void setProperty(Interpreter& interp,
	                 std::string_view name, const TclObject& value) override;
	void getProperty(std::string_view name, TclObject& result) const override;
	std::string_view getType() const override;

private:
	bool takeImageDimensions() const;

	gl::vec2 getSize(const OutputSurface& output) const override;
	uint8_t getFadedAlpha() const override;
	std::unique_ptr<BaseImage> createSDL(OutputSurface& output) override;
	std::unique_ptr<BaseImage> createGL (OutputSurface& output) override;
	template <typename IMAGE> std::unique_ptr<BaseImage> create(
		OutputSurface& output);

	std::string imageName;
	gl::vec2 size, relSize;
	float scale, borderSize, relBorderSize;
	unsigned borderRGBA;
};

} // namespace openmsx

#endif
