#ifndef OSDRECTANGLE_HH
#define OSDRECTANGLE_HH

#include "OSDImageBasedWidget.hh"
#include "stl.hh"
#include <array>
#include <cstdint>
#include <memory>

namespace openmsx {

class BaseImage;

class OSDRectangle final : public OSDImageBasedWidget
{
protected:
	static constexpr auto rectangleProperties = [] {
		using namespace std::literals;
		return concatArray(
			imageBasedProperties,
			std::array{
				"-w"sv, "-h"sv, "-relw"sv, "-relh"sv,
				"-scale"sv, "-image"sv,
				"-bordersize"sv, "-relbordersize"sv,
				"-borderrgba"sv,
			});
	}();

public:
	OSDRectangle(Display& display, const TclObject& name);

	[[nodiscard]] std::span<const std::string_view> getProperties() const override {
		return rectangleProperties;
	}
	void setProperty(Interpreter& interp,
	                 std::string_view name, const TclObject& value) override;
	void getProperty(std::string_view name, TclObject& result) const override;
	[[nodiscard]] std::string_view getType() const override;

private:
	[[nodiscard]] bool takeImageDimensions() const;

	[[nodiscard]] gl::vec2 getSize(const OutputSurface& output) const override;
	[[nodiscard]] uint8_t getFadedAlpha() const override;
	[[nodiscard]] std::unique_ptr<BaseImage> createSDL(OutputSurface& output) override;
	[[nodiscard]] std::unique_ptr<BaseImage> createGL (OutputSurface& output) override;
	template<typename IMAGE> [[nodiscard]] std::unique_ptr<BaseImage> create(
		OutputSurface& output);

private:
	std::string imageName;
	gl::vec2 size, relSize;
	float scale, borderSize, relBorderSize;
	uint32_t borderRGBA;
};

} // namespace openmsx

#endif
