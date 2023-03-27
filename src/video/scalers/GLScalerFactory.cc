#include "GLScalerFactory.hh"
#include "GLSimpleScaler.hh"
#include "GLRGBScaler.hh"
#include "GLScaleNxScaler.hh"
#include "GLTVScaler.hh"
#include "GLHQScaler.hh"
#include "GLHQLiteScaler.hh"
#include "GLContext.hh"
#include "RenderSettings.hh"
#include "unreachable.hh"
#include <memory>

namespace openmsx::GLScalerFactory {

std::unique_ptr<GLScaler> createScaler(RenderSettings& renderSettings)
{
	GLScaler& fallback = gl::context->getFallbackScaler();
	switch (renderSettings.getScaleAlgorithm()) {
	case RenderSettings::SCALER_SIMPLE:
		return std::make_unique<GLSimpleScaler>(renderSettings, fallback);
	case RenderSettings::SCALER_RGBTRIPLET:
		return std::make_unique<GLRGBScaler>(renderSettings, fallback);
	case RenderSettings::SCALER_SCALE:
		return std::make_unique<GLScaleNxScaler>(fallback);
	case RenderSettings::SCALER_TV:
		return std::make_unique<GLTVScaler>(renderSettings);
	case RenderSettings::SCALER_HQ:
		return std::make_unique<GLHQScaler>(fallback);
	case RenderSettings::SCALER_HQLITE:
		return std::make_unique<GLHQLiteScaler>(fallback);
	default:
		UNREACHABLE;
	}
	return nullptr; // avoid warning
}

} // namespace openmsx::GLScalerFactory
