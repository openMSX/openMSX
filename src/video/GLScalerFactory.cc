// $Id$

#include "GLScalerFactory.hh"
#include "GLSimpleScaler.hh"
#include "GLScaleNxScaler.hh"
#include "GLTVScaler.hh"
#include "RenderSettings.hh"
#include "EnumSetting.hh"
#include <cassert>

using std::auto_ptr;

namespace openmsx {

auto_ptr<GLScaler> GLScalerFactory::createScaler(
	RenderSettings& renderSettings
	)
{
	switch (renderSettings.getScaleAlgorithm().getValue()) {
	case RenderSettings::SCALER_SIMPLE:
	// TODO: Until we have GL versions of these, map them to "simple".
	case RenderSettings::SCALER_SAI:
	case RenderSettings::SCALER_HQ:
	case RenderSettings::SCALER_HQLITE:
	case RenderSettings::SCALER_RGBTRIPLET:
		return auto_ptr<GLScaler>(
			new GLSimpleScaler(renderSettings)
			);
	case RenderSettings::SCALER_SCALE:
		return auto_ptr<GLScaler>(new GLScaleNxScaler());
	case RenderSettings::SCALER_TV:
		return auto_ptr<GLScaler>(new GLTVScaler());
	default:
		assert(false);
	}
	return auto_ptr<GLScaler>(); // avoid warning
}

} // namespace openmsx

