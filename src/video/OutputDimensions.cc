#include "OutputDimensions.hh"

namespace openmsx {

OutputDimensions::OutputDimensions(gl::ivec2 physSize_, RenderSettings::ScaleFactor factor)
	: m_physSize(physSize_)
{
	gl::ivec2 iLogSize(320, 240);
	gl::vec2 logSize(iLogSize); // convert int->float

	using enum RenderSettings::ScaleFactor;
	switch (factor) {
	case FREE:
		// Stretch the logical image to the full physical output area.
		m_viewSize = physSize_;
		break;
	case FIXED_ASPECT_RATIO: {
		float scale = min_component(gl::vec2(m_physSize) / logSize);
		m_viewSize = round(logSize * scale);
		break;
	}
	case INTEGER: {
		float scale = min_component(gl::vec2(m_physSize) / logSize);
		int intScale = (scale < 1.0f) ? 1 : int(scale);
		m_viewSize = iLogSize * intScale;
		break;
	}
	default: { // F1-F8
		auto f = std::to_underlying(factor);
		assert(1 <= f && f <= 8);
		m_viewSize = iLogSize * f;
	}
	}
	m_viewOffset = (m_physSize - m_viewSize) / 2;
}

} // namespace openmsx
