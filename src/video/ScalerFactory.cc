// $Id$

#include "RenderSettings.hh"
#include "EnumSetting.hh"
#include "IntegerSetting.hh"
#include "ScalerFactory.hh"
#include "SimpleScaler.hh"
#include "SaI2xScaler.hh"
#include "SaI3xScaler.hh"
#include "Scale2xScaler.hh"
#include "Scale3xScaler.hh"
#include "HQ2xScaler.hh"
#include "HQ3xScaler.hh"
#include "HQ2xLiteScaler.hh"
#include "HQ3xLiteScaler.hh"
#include "RGBTriplet3xScaler.hh"
#include "Simple3xScaler.hh"
#include "LowScaler.hh"
#include <cassert>

using std::auto_ptr;

namespace openmsx {

template <class Pixel>
auto_ptr<Scaler> ScalerFactory<Pixel>::createScaler(
	const PixelOperations<Pixel>& pixelOps, RenderSettings& renderSettings)
{
	switch (renderSettings.getScaleFactor().getValue()) {
	case 1:
		return auto_ptr<Scaler>(new LowScaler<Pixel>(pixelOps));
	case 2:
		switch (renderSettings.getScaleAlgorithm().getValue()) {
		case RenderSettings::SCALER_SIMPLE:
			return auto_ptr<Scaler>(
				new SimpleScaler<Pixel>(pixelOps, renderSettings)
				);
		case RenderSettings::SCALER_SAI:
			return auto_ptr<Scaler>(new SaI2xScaler<Pixel>(pixelOps));
		case RenderSettings::SCALER_SCALE:
			return auto_ptr<Scaler>(new Scale2xScaler<Pixel>(pixelOps));
		case RenderSettings::SCALER_HQ:
			return auto_ptr<Scaler>(new HQ2xScaler<Pixel>(pixelOps));
		case RenderSettings::SCALER_HQLITE:
			return auto_ptr<Scaler>(new HQ2xLiteScaler<Pixel>(pixelOps));
		case RenderSettings::SCALER_RGBTRIPLET:
			return auto_ptr<Scaler>(
				new SimpleScaler<Pixel>(pixelOps, renderSettings)
				);
		default:
			assert(false);
		}
	case 3:
		switch (renderSettings.getScaleAlgorithm().getValue()) {
		case RenderSettings::SCALER_SIMPLE:
			return auto_ptr<Scaler>(
				new Simple3xScaler<Pixel>(pixelOps, renderSettings)
				);
		case RenderSettings::SCALER_SAI:
			return auto_ptr<Scaler>(new SaI3xScaler<Pixel>(pixelOps));
		case RenderSettings::SCALER_SCALE:
			return auto_ptr<Scaler>(new Scale3xScaler<Pixel>(pixelOps));
		case RenderSettings::SCALER_HQ:
			return auto_ptr<Scaler>(new HQ3xScaler<Pixel>(pixelOps));
		case RenderSettings::SCALER_HQLITE:
			return auto_ptr<Scaler>(new HQ3xLiteScaler<Pixel>(pixelOps));
		case RenderSettings::SCALER_RGBTRIPLET:
			return auto_ptr<Scaler>(
				new RGBTriplet3xScaler<Pixel>(pixelOps, renderSettings)
				);
		default:
			assert(false);
		}
	default:
		assert(false);
	}
	return auto_ptr<Scaler>(); // avoid warning
}

// Force template instantiation.
template class ScalerFactory<word>;
template class ScalerFactory<unsigned>;

} // namespace openmsx

