#ifndef OSDIMAGEBASEDWIDGET_HH
#define OSDIMAGEBASEDWIDGET_HH

#include "OSDWidget.hh"
#include "stl.hh"
#include <array>
#include <cstdint>
#include <span>

namespace openmsx {

class BaseImage;
class Display;

class OSDImageBasedWidget : public OSDWidget
{
protected:
	static constexpr auto imageBasedProperties = [] {
		using namespace std::literals;
		return concatArray(
			widgetProperties,
			std::array{
				"-rgba"sv, "-rgb"sv, "-alpha"sv,
				"-fadePeriod"sv, "-fadeTarget"sv,
				"-fadeCurrent"sv,
			});
	}();

public:
	[[nodiscard]] uint32_t getRGBA(uint32_t corner) const { return rgba[corner]; }
	[[nodiscard]] std::span<const uint32_t, 4> getRGBA4() const { return rgba; }

	[[nodiscard]] virtual uint8_t getFadedAlpha() const = 0;

	[[nodiscard]] std::span<const std::string_view> getProperties() const override {
		return imageBasedProperties;
	}
	void setProperty(Interpreter& interp,
	                 std::string_view name, const TclObject& value) override;
	void getProperty(std::string_view name, TclObject& result) const override;
	[[nodiscard]] float getRecursiveFadeValue() const override;
	[[nodiscard]] bool isVisible() const override;
	[[nodiscard]] bool isRecursiveFading() const override;

protected:
	OSDImageBasedWidget(Display& display, const TclObject& name);
	~OSDImageBasedWidget() override;
	[[nodiscard]] bool hasConstantAlpha() const;
	void createImage(OutputSurface& output);
	void invalidateLocal() override;
	void paintSDL(OutputSurface& output) override;
	void paintGL (OutputSurface& output) override;
	[[nodiscard]] virtual std::unique_ptr<BaseImage> createSDL(OutputSurface& output) = 0;
	[[nodiscard]] virtual std::unique_ptr<BaseImage> createGL (OutputSurface& output) = 0;

	void setError(std::string message);
	[[nodiscard]] bool hasError() const { return error; }

	std::unique_ptr<BaseImage> image;

private:
	void setRGBA(std::span<const uint32_t, 4> newRGBA);
	[[nodiscard]] bool isFading() const;
	[[nodiscard]] float getCurrentFadeValue() const;
	[[nodiscard]] float getCurrentFadeValue(uint64_t now) const;
	void updateCurrentFadeValue();

	void paint(OutputSurface& output, bool openGL);
	[[nodiscard]] gl::vec2 getTransformedPos(const OutputSurface& output) const;

private:
	uint64_t startFadeTime = 0;
	float fadePeriod = 0.0f;
	float fadeTarget = 1.0f;
	mutable float startFadeValue = 1.0f;
	std::array<uint32_t, 4> rgba;
	bool error = false;
};

} // namespace openmsx

#endif
