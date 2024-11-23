#ifndef DEFLICKER_HH
#define DEFLICKER_HH

#include "FrameSource.hh"

#include <memory>
#include <span>

namespace openmsx {

class RawFrame;

class Deflicker final : public FrameSource
{
public:
	explicit Deflicker(std::span<std::unique_ptr<RawFrame>, 4> lastFrames);
	Deflicker(const Deflicker&) = default;
	Deflicker(Deflicker&&) = default;
	Deflicker& operator=(const Deflicker&) = default;
	Deflicker& operator=(Deflicker&&) = default;
	~Deflicker() = default;

	void init();

	[[nodiscard]] unsigned getLineWidth(unsigned line) const override;
	[[nodiscard]] std::span<const Pixel> getUnscaledLine(
		unsigned line, std::span<Pixel> helpBuf) const override;

private:
	std::span<std::unique_ptr<RawFrame>, 4> lastFrames;
};

} // namespace openmsx

#endif
