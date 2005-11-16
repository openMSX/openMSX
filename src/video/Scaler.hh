// $Id$

#ifndef SCALER_HH
#define SCALER_HH

#include <memory>

class SDL_PixelFormat;

namespace openmsx {

class RenderSettings;
class FrameSource;
class OutputSurface;

/** Enumeration of Scalers known to openMSX.
  */
enum ScalerID {
	/** SimpleScaler. */
	SCALER_SIMPLE,
	/** SaI2xScaler. */
	SCALER_SAI2X,
	/** Scale2xScaler. */
	SCALER_SCALE2X,
	/** Scale3xScaler. */
	SCALER_SCALE3X,
	/** HQ2xScaler. */
	SCALER_HQ2X,
	/** HQ3xScaler. */
	SCALER_HQ3X,
	/** HQ2xLiteScaler. */
	SCALER_HQ2XLITE,
	/** HQ3xLiteScaler. */
	SCALER_HQ3XLITE,
	/** RGBTriplet3xScaler. */
	SCALER_RGBTRIPLET3X,
	/** SimpleScaler3x. */
	SCALER_SIMPLE3X,
	/** Low resolution (320x240) scaler */
	SCALER_LOW
};

/** Abstract base class for scalers.
  * A scaler is an algorithm that converts low-res graphics to hi-res graphics.
  */
template <class Pixel>
class Scaler
{
public:
	virtual ~Scaler() {}

	/** Instantiates a Scaler.
	  * @param id Identifies the scaler algorithm.
	  * @param format Pixel format of the surfaces the scaler will be used on.
	  * @return A Scaler object, owned by the caller.
	  */
	static std::auto_ptr<Scaler> createScaler(
		ScalerID id, SDL_PixelFormat* format,
		RenderSettings& renderSettings);

	/** Fills the given area, which contains only a single color.
	  * @param color Color the area should be filled with.
	  * @param dst Destination: image to store the scaled output in.
	  * @param startY Destination Y-coordinate of the top line.
	  * @param endY Destination Y-coordinate of the bottom line (exclusive).
	  */
	virtual void scaleBlank(Pixel color, OutputSurface& dst,
	                        unsigned startY, unsigned endY);

	/** Scales the image in the given area, which must consist of lines which
	  * are all equally wide.
	  * Scaling factor depends on the concrete scaler.
	  * @param src Source: the frame to be scaled.
	  * @param lineWidth The number of pixels per line for the given area.
	  * @param srcStartY Y-coordinate of the top source line (inclusive).
	  * @param srcEndY Y-coordinate of the bottom source line (exclusive).
	  * @param dst Destination: image to store the scaled output in.
	  * @param dstStartY Y-coordinate of the top destination line (inclusive).
	  * @param dstEndY Y-coordinate of the bottom destination line (exclusive).
	  */
	virtual void scaleImage(
		FrameSource& src, unsigned lineWidth,
		unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY) = 0;
};

} // namespace openmsx

#endif
