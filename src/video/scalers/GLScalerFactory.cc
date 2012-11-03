// $Id$

#include "GLScalerFactory.hh"
#include "GLSimpleScaler.hh"
#include "GLRGBScaler.hh"
#include "GLSaIScaler.hh"
#include "GLScaleNxScaler.hh"
#include "GLTVScaler.hh"
#include "GLHQScaler.hh"
#include "GLHQLiteScaler.hh"
#include "RenderSettings.hh"
#include "EnumSetting.hh"
#include "unreachable.hh"

using std::unique_ptr;

namespace openmsx {
namespace GLScalerFactory {

unique_ptr<GLScaler> createScaler(RenderSettings& renderSettings)
{
	switch (renderSettings.getScaleAlgorithm().getValue()) {
	case RenderSettings::SCALER_SAI:
		// disabled for now:
		//   - it doesn't work (yet) on ATI cards
		//   - it probably has some bugs because (on nvidia cards)
		//     it does not give the same result as the SW SaI scaler,
		//     although it's reasonably close
		//return unique_ptr<GLScaler>(new GLSaIScaler());
	case RenderSettings::SCALER_SIMPLE:
		return unique_ptr<GLScaler>(new GLSimpleScaler(renderSettings));
	case RenderSettings::SCALER_RGBTRIPLET:
		return unique_ptr<GLScaler>(new GLRGBScaler(renderSettings));
	case RenderSettings::SCALER_SCALE:
		return unique_ptr<GLScaler>(new GLScaleNxScaler());
	case RenderSettings::SCALER_TV:
		return unique_ptr<GLScaler>(new GLTVScaler(renderSettings));
	case RenderSettings::SCALER_HQ:
		return unique_ptr<GLScaler>(new GLHQScaler());
	case RenderSettings::SCALER_MLAA: // fallback
	case RenderSettings::SCALER_HQLITE:
		return unique_ptr<GLScaler>(new GLHQLiteScaler());
	default:
		UNREACHABLE;
	}
	return unique_ptr<GLScaler>(); // avoid warning
}

} // namespace GLScalerFactory
} // namespace openmsx
