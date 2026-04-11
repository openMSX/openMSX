#include "OutputDimensions.hh"

namespace openmsx {

OutputDimensions::OutputDimensions(gl::ivec2 physSize_, RenderSettings::ScaleMode mode)
	: m_physSize(physSize_)
{
	gl::ivec2 iLogSize(320, 240);
	gl::vec2 logSize(iLogSize); // convert int->float

	using enum RenderSettings::ScaleMode;
	switch (mode) {
	case FREE: {
		// Stretch the logical image to the full physical output area.
		m_viewScale = gl::vec2(physSize_) / logSize;
		m_viewSize = physSize_;
		m_viewOffset = gl::ivec2(0, 0);
		break;
	}
	case FIXED_ASPECT_RATIO: {
		gl::vec2 physSize(physSize_); // convert int->float

		float scale = min_component(physSize / logSize);
		m_viewScale = gl::vec2(scale); // same scale for X and Y to preserve aspect ratio

		gl::vec2 viewSize = logSize * scale;
		m_viewSize = round(viewSize);

		gl::vec2 viewOffset = (physSize - viewSize) * 0.5f;
		m_viewOffset = round(viewOffset);
		break;
	}
	case INTEGER: {
		gl::vec2 physSize(physSize_); // convert int->float

		float scale = min_component(physSize / logSize);
		int intScale = (scale < 1.0f) ? 1 : int(scale);
		m_viewScale = gl::vec2(float(intScale));

		m_viewSize = iLogSize * intScale;
		gl::vec2 viewOffset = (physSize - gl::vec2(m_viewSize)) * 0.5f;
		m_viewOffset = round(viewOffset);
		break;
	}
	}
}

} // namespace openmsx
