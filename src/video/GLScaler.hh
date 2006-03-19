// $Id$

#ifndef GLSCALER_HH
#define GLSCALER_HH

namespace openmsx {

class PartialColourTexture;

/** Abstract base class for OpenGL scalers.
  * A scaler is an algorithm that converts low-res graphics to hi-res graphics.
  */
class GLScaler
{
public:
	virtual ~GLScaler() {}

	/** Scales the image in the given area, which must consist of lines which
	  * are all equally wide.
	  * Scaling factor depends on the concrete scaler.
	  * @param src Source: texture containing the frame to be scaled.
	  * @param srcStartY Y-coordinate of the top source line (inclusive).
	  * @param srcEndY Y-coordinate of the bottom source line (exclusive).
	  * @param srcWidth The number of pixels per line for the given area.
	  * @param dstStartY Y-coordinate of the top destination line (inclusive).
	  * @param dstEndY Y-coordinate of the bottom destination line (exclusive).
	  * @param dstWidth The number of pixels per line on the output screen.
	  */
	virtual void scaleImage(
		PartialColourTexture& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		unsigned dstStartY, unsigned dstEndY, unsigned dstWidth
		) = 0;
};

} // namespace openmsx

#endif // GLSCALER_HH
