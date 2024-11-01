#include "GLScalerFactory.hh"
#include "GLSimpleScaler.hh"
#include "GLRGBScaler.hh"
#include "GLScaleNxScaler.hh"
#include "GLTVScaler.hh"
#include "GLHQScaler.hh"
#include "GLContext.hh"
#include "RenderSettings.hh"
#include "unreachable.hh"
#include <memory>

namespace openmsx::GLScalerFactory {

std::unique_ptr<GLScaler> createScaler(
	RenderSettings& renderSettings, unsigned maxWidth, unsigned maxHeight)
{
	GLScaler& fallback = gl::context->getFallbackScaler();
	switch (renderSettings.getScaleAlgorithm()) {
	using enum RenderSettings::ScaleAlgorithm;
	case SIMPLE:
		return std::make_unique<GLSimpleScaler>(renderSettings, fallback);
	case RGBTRIPLET:
		return std::make_unique<GLRGBScaler>(renderSettings, fallback);
	case SCALE:
		return std::make_unique<GLScaleNxScaler>(fallback);
	case TV:
		return std::make_unique<GLTVScaler>(renderSettings);
	case HQ:
		return std::make_unique<GLHQScaler>(fallback, maxWidth, maxHeight);
	default:
		UNREACHABLE;
	}
}

} // namespace openmsx::GLScalerFactory
