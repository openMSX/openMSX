// $Id$

#include "ScalerFactory.hh"
#include "RenderSettings.hh"
#include "EnumSetting.hh"
#include "IntegerSetting.hh"
#include "Simple2xScaler.hh"
#include "Simple3xScaler.hh"
#include "SaI2xScaler.hh"     // note: included even if MAX_SCALE_FACTOR == 1
#include "SaI3xScaler.hh"
#include "Scale2xScaler.hh"
#include "Scale3xScaler.hh"
#include "HQ2xScaler.hh"
#include "HQ3xScaler.hh"
#include "HQ2xLiteScaler.hh"
#include "HQ3xLiteScaler.hh"
#include "RGBTriplet3xScaler.hh"
#include "Scaler1.hh"
#include "unreachable.hh"
#include "build-info.hh"

using std::auto_ptr;

namespace openmsx {

template <class Pixel>
auto_ptr<Scaler> ScalerFactory<Pixel>::createScaler(
	const PixelOperations<Pixel>& pixelOps, RenderSettings& renderSettings)
{
	switch (renderSettings.getScaleFactor().getValue()) {
#if (MIN_SCALE_FACTOR <= 1) && (MAX_SCALE_FACTOR >= 1)
	case 1:
		return auto_ptr<Scaler>(new Scaler1<Pixel>(pixelOps));
#endif
#if (MIN_SCALE_FACTOR <= 2) && (MAX_SCALE_FACTOR >= 2)
	case 2:
		switch (renderSettings.getScaleAlgorithm().getValue()) {
		case RenderSettings::SCALER_SIMPLE:
			return auto_ptr<Scaler>(
				new Simple2xScaler<Pixel>(pixelOps, renderSettings));
		case RenderSettings::SCALER_SAI:
			return auto_ptr<Scaler>(new SaI2xScaler<Pixel>(pixelOps));
		case RenderSettings::SCALER_SCALE:
			return auto_ptr<Scaler>(new Scale2xScaler<Pixel>(pixelOps));
		case RenderSettings::SCALER_HQ:
			return auto_ptr<Scaler>(new HQ2xScaler<Pixel>(pixelOps));
		case RenderSettings::SCALER_HQLITE:
			return auto_ptr<Scaler>(new HQ2xLiteScaler<Pixel>(pixelOps));
		case RenderSettings::SCALER_RGBTRIPLET:
		case RenderSettings::SCALER_TV: // fallback
			return auto_ptr<Scaler>(
				new Simple2xScaler<Pixel>(pixelOps, renderSettings));
		default:
			UNREACHABLE;
		}
#endif
#if (MIN_SCALE_FACTOR <= 4) && (MAX_SCALE_FACTOR >= 3)
	case 3:
	case 4: // fallback
		switch (renderSettings.getScaleAlgorithm().getValue()) {
		case RenderSettings::SCALER_SIMPLE:
			return auto_ptr<Scaler>(
				new Simple3xScaler<Pixel>(pixelOps, renderSettings));
		case RenderSettings::SCALER_SAI:
			return auto_ptr<Scaler>(new SaI3xScaler<Pixel>(pixelOps));
		case RenderSettings::SCALER_SCALE:
			return auto_ptr<Scaler>(new Scale3xScaler<Pixel>(pixelOps));
		case RenderSettings::SCALER_HQ:
			return auto_ptr<Scaler>(new HQ3xScaler<Pixel>(pixelOps));
		case RenderSettings::SCALER_HQLITE:
			return auto_ptr<Scaler>(new HQ3xLiteScaler<Pixel>(pixelOps));
		case RenderSettings::SCALER_RGBTRIPLET:
		case RenderSettings::SCALER_TV: // fallback
			return auto_ptr<Scaler>(
				new RGBTriplet3xScaler<Pixel>(pixelOps, renderSettings));
		default:
			UNREACHABLE;
		}
#endif
	default:
		UNREACHABLE;
	}
	return auto_ptr<Scaler>(); // avoid warning
}

// Force template instantiation.
#if HAVE_16BPP
template class ScalerFactory<word>;
#endif
#if HAVE_32BPP
template class ScalerFactory<unsigned>;
#endif

} // namespace openmsx
