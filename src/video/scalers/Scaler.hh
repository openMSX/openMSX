#ifndef SCALER_HH
#define SCALER_HH

#include <concepts>

namespace openmsx {

class FrameSource;
class RawFrame;
template<std::unsigned_integral Pixel> class ScalerOutput;

/** Abstract base class for scalers.
  * A scaler is an algorithm that converts low-res graphics to hi-res graphics.
  */
template<std::unsigned_integral Pixel> class Scaler
{
public:
	virtual ~Scaler() = default;

	/** Scales the image in the given area, which must consist of lines which
	  * are all equally wide.
	  * Scaling factor depends on the concrete scaler.
	  * @param src Source: the frame to be scaled.
	  * @param superImpose The to-be-superimposed image (can be nullptr).
	  * @param srcStartY Y-coordinate of the top source line (inclusive).
	  * @param srcEndY Y-coordinate of the bottom source line (exclusive).
	  * @param srcWidth The number of pixels per line for the given area.
	  * @param dst Destination: image to store the scaled output in.
	  * @param dstStartY Y-coordinate of the top destination line (inclusive).
	  * @param dstEndY Y-coordinate of the bottom destination line (exclusive).
	  */
	virtual void scaleImage(FrameSource& src, const RawFrame* superImpose,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY) = 0;
};

} // namespace openmsx

#endif
