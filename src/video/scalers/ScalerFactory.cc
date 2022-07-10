#include "ScalerFactory.hh"
#include "RenderSettings.hh"
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
#include "MLAAScaler.hh"
#include "Scaler1.hh"
#include "unreachable.hh"
#include "build-info.hh"
#include <cstdint>
#include <memory>

namespace openmsx {

template<std::unsigned_integral Pixel>
std::unique_ptr<Scaler<Pixel>> ScalerFactory<Pixel>::createScaler(
	const PixelOperations<Pixel>& pixelOps, RenderSettings& renderSettings)
{
	switch (renderSettings.getScaleFactor()) {
#if (MIN_SCALE_FACTOR <= 1) && (MAX_SCALE_FACTOR >= 1)
	case 1:
		return std::make_unique<Scaler1<Pixel>>(pixelOps);
#endif
#if (MIN_SCALE_FACTOR <= 2) && (MAX_SCALE_FACTOR >= 2)
	case 2:
		switch (renderSettings.getScaleAlgorithm()) {
		case RenderSettings::SCALER_SIMPLE:
			return std::make_unique<Simple2xScaler<Pixel>>(
				pixelOps, renderSettings);
		case RenderSettings::SCALER_SAI:
			return std::make_unique<SaI2xScaler<Pixel>>(pixelOps);
		case RenderSettings::SCALER_SCALE:
			return std::make_unique<Scale2xScaler<Pixel>>(pixelOps);
		case RenderSettings::SCALER_HQ:
			return std::make_unique<HQ2xScaler<Pixel>>(pixelOps);
		case RenderSettings::SCALER_HQLITE:
			return std::make_unique<HQ2xLiteScaler<Pixel>>(pixelOps);
		case RenderSettings::SCALER_RGBTRIPLET:
		case RenderSettings::SCALER_TV: // fallback
			return std::make_unique<Simple2xScaler<Pixel>>(
				pixelOps, renderSettings);
		case RenderSettings::SCALER_MLAA:
			return std::make_unique<MLAAScaler<Pixel>>(640, pixelOps);
		default:
			UNREACHABLE;
		}
#endif
#if (MIN_SCALE_FACTOR <= 4) && (MAX_SCALE_FACTOR >= 3)
	case 3:
	case 4: // fallback
		switch (renderSettings.getScaleAlgorithm()) {
		case RenderSettings::SCALER_SIMPLE:
			return std::make_unique<Simple3xScaler<Pixel>>(
				pixelOps, renderSettings);
		case RenderSettings::SCALER_SAI:
			return std::make_unique<SaI3xScaler<Pixel>>(pixelOps);
		case RenderSettings::SCALER_SCALE:
			return std::make_unique<Scale3xScaler<Pixel>>(pixelOps);
		case RenderSettings::SCALER_HQ:
			return std::make_unique<HQ3xScaler<Pixel>>(pixelOps);
		case RenderSettings::SCALER_HQLITE:
			return std::make_unique<HQ3xLiteScaler<Pixel>>(pixelOps);
		case RenderSettings::SCALER_RGBTRIPLET:
		case RenderSettings::SCALER_TV: // fallback
			return std::make_unique<RGBTriplet3xScaler<Pixel>>(
				pixelOps, renderSettings);
		case RenderSettings::SCALER_MLAA:
			return std::make_unique<MLAAScaler<Pixel>>(960, pixelOps);
		default:
			UNREACHABLE;
		}
#endif
	default:
		UNREACHABLE;
	}
	return nullptr; // avoid warning
}

// Force template instantiation.
#if HAVE_16BPP
template class ScalerFactory<uint16_t>;
#endif
#if HAVE_32BPP
template class ScalerFactory<uint32_t>;
#endif

} // namespace openmsx
