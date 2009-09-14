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

using std::auto_ptr;

namespace openmsx {
namespace GLScalerFactory {

auto_ptr<GLScaler> createScaler(RenderSettings& renderSettings)
{
	switch (renderSettings.getScaleAlgorithm().getValue()) {
	case RenderSettings::SCALER_SAI:
		// disabled for now:
		//   - it doesn't work (yet) on ATI cards
		//   - it probably has some bugs because (on nvidia cards)
		//     it does not give the same result as the SW SaI scaler,
		//     although it's reasonably close
		//return auto_ptr<GLScaler>(new GLSaIScaler());
	case RenderSettings::SCALER_SIMPLE:
		return auto_ptr<GLScaler>(new GLSimpleScaler(renderSettings));
	case RenderSettings::SCALER_RGBTRIPLET:
		return auto_ptr<GLScaler>(new GLRGBScaler(renderSettings));
	case RenderSettings::SCALER_SCALE:
		return auto_ptr<GLScaler>(new GLScaleNxScaler());
	case RenderSettings::SCALER_TV:
		return auto_ptr<GLScaler>(new GLTVScaler());
	case RenderSettings::SCALER_HQ:
		return auto_ptr<GLScaler>(new GLHQScaler());
	case RenderSettings::SCALER_HQLITE:
		return auto_ptr<GLScaler>(new GLHQLiteScaler());
	default:
		UNREACHABLE;
	}
	return auto_ptr<GLScaler>(); // avoid warning
}

} // namespace GLScalerFactory
} // namespace openmsx
