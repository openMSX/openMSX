#ifndef MLAASCALER_HH
#define MLAASCALER_HH

#include "Scaler.hh"
#include "PixelOperations.hh"

namespace openmsx {

/** Scaler that uses a variation of the morphological anti-aliasing algorithm.
  * The paper that describes the original MLAA algorithm can be found here:
  *   http://visual-computing.intel-research.net
  *                                    /publications/papers/2009/mlaa/mlaa.pdf
  * The original algorithm is a filter; it was adapted to scale the image.
  * The classification of edges has been expanded to work better with the
  * hand-drawn 2D images we apply the scaler to, as opposed to the 3D rendered
  * images that the original algorithm was designed for.
  */
template<typename Pixel>
class MLAAScaler final : public Scaler<Pixel>
{
public:
	MLAAScaler(unsigned dstWidth, const PixelOperations<Pixel>& pixelOps);

	void scaleImage(FrameSource& src, const RawFrame* superImpose,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY) override;

private:
	const PixelOperations<Pixel> pixelOps;
	const unsigned dstWidth;
};

} // namespace openmsx

#endif
