#ifndef DIRECTSCALEROUTPUT_HH
#define DIRECTSCALEROUTPUT_HH

#include "ScalerOutput.hh"
#include "SDLOutputSurface.hh"
#include <concepts>
#include <optional>

namespace openmsx {

template<std::unsigned_integral Pixel>
class DirectScalerOutput final : public ScalerOutput<Pixel>
{
public:
	explicit DirectScalerOutput(SDLOutputSurface& output);

	[[nodiscard]] unsigned getWidth()  const override {
		return output.getLogicalWidth();
	}
	[[nodiscard]] unsigned getHeight() const override {
		return output.getLogicalHeight();
	}

	void frameStart() override {
		pixelAccess.emplace(output.getDirectPixelAccess());
	}
	void frameStop() override {
		pixelAccess.reset();
	}

	[[nodiscard]] std::span<Pixel> acquireLine(unsigned y) override;
	void releaseLine(unsigned y, std::span<Pixel> buf) override;
	void fillLine   (unsigned y, Pixel color) override;

private:
	SDLOutputSurface& output;
	std::optional<SDLDirectPixelAccess> pixelAccess;
};

} // namespace openmsx

#endif
