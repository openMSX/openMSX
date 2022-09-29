#ifndef DEFLICKER_HH
#define DEFLICKER_HH

#include "FrameSource.hh"
#include <memory>
#include <span>

namespace openmsx {

class RawFrame;

class Deflicker : public FrameSource
{
public:
	// Factory method, actually returns a Deflicker subclass.
	[[nodiscard]] static std::unique_ptr<Deflicker> create(
		const PixelFormat& format,
		std::span<std::unique_ptr<RawFrame>, 4> lastFrames);
	void init();
	virtual ~Deflicker() = default;

protected:
	Deflicker(const PixelFormat& format,
	          std::span<std::unique_ptr<RawFrame>, 4> lastFrames);

	[[nodiscard]] unsigned getLineWidth(unsigned line) const override;

protected:
	std::span<std::unique_ptr<RawFrame>, 4> lastFrames;
};

} // namespace openmsx

#endif
