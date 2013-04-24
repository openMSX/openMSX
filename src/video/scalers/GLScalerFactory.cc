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
#include "memory.hh"
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
		//return make_unique<GLSaIScaler();
	case RenderSettings::SCALER_SIMPLE:
		return make_unique<GLSimpleScaler>(renderSettings);
	case RenderSettings::SCALER_RGBTRIPLET:
		return make_unique<GLRGBScaler>(renderSettings);
	case RenderSettings::SCALER_SCALE:
		return make_unique<GLScaleNxScaler>();
	case RenderSettings::SCALER_TV:
		return make_unique<GLTVScaler>(renderSettings);
	case RenderSettings::SCALER_HQ:
		return make_unique<GLHQScaler>();
	case RenderSettings::SCALER_MLAA: // fallback
	case RenderSettings::SCALER_HQLITE:
		return make_unique<GLHQLiteScaler>();
	default:
		UNREACHABLE;
	}
	return nullptr; // avoid warning
}

} // namespace GLScalerFactory
} // namespace openmsx
