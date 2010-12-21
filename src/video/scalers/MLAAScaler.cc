// $Id$

#include "MLAAScaler.hh"
#include "OutputSurface.hh"
#include "FrameSource.hh"
#include "PixelOperations.hh"
#include "Math.hh"
#include "openmsx.hh"
#include "vla.hh"
#include "build-info.hh"
#include <algorithm>
#include <cassert>

namespace openmsx {

template <class Pixel>
MLAAScaler<Pixel>::MLAAScaler(
		unsigned dstWidth_, const PixelOperations<Pixel>& pixelOps_)
	: dstWidth(dstWidth_)
	, pixelOps(pixelOps_)
{
}

template <class Pixel>
void MLAAScaler<Pixel>::scaleImage(
		FrameSource& src, const RawFrame* superImpose,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	(void)superImpose; // TODO: Support superimpose.
	//fprintf(stderr, "scale line [%d..%d) to [%d..%d), width %d to %d\n",
	//	srcStartY, srcEndY, dstStartY, dstEndY, srcWidth, dstWidth
	//	);

	// TODO: Support non-integer zoom factors.
	const unsigned zoomFactorX = dstWidth / srcWidth;
	const unsigned zoomFactorY = (dstEndY - dstStartY) / (srcEndY - srcStartY);

	// Retrieve line pointers.
	// We allow lookups one line before and after the scaled area.
	// This is not just a trick to avoid range checks: to properly handle
	// pixels at the top/bottom of the display area we must compare them to
	// the border color.
	const int srcNumLines = srcEndY - srcStartY;
	VLA(const Pixel*, srcLinePtrsArray, srcNumLines + 2);
	const Pixel** srcLinePtrs = &srcLinePtrsArray[1];
	for (int y = -1; y < srcNumLines + 1; y++) {
		srcLinePtrs[y] = src.getLinePtr<Pixel>(srcStartY + y, srcWidth);
	}

	enum { UP = 1 << 0, RIGHT = 1 << 1, DOWN = 1 << 2, LEFT = 1 << 3 };
	VLA(byte, edges, srcNumLines * srcWidth);
	byte* edgePtr = edges;
	for (int y = 0; y < srcNumLines; y++) {
		const Pixel* srcLinePtr = srcLinePtrs[y];
		for (unsigned x = 0; x < srcWidth; x++) {
			Pixel colMid = srcLinePtr[x];
			byte pixEdges = 0;
			if (x > 0 && srcLinePtr[x - 1] != colMid) {
				pixEdges |= LEFT;
			}
			if (x < srcWidth - 1 && srcLinePtr[x + 1] != colMid) {
				pixEdges |= RIGHT;
			}
			if (srcLinePtrs[y - 1][x] != colMid) {
				pixEdges |= UP;
			}
			if (srcLinePtrs[y + 1][x] != colMid) {
				pixEdges |= DOWN;
			}
			*edgePtr++ = pixEdges;
		}
	}

	dst.lock();
	// Do a mosaic scale so every destination pixel has a color.
	unsigned dstY = dstStartY;
	for (int y = 0; y < srcNumLines; y++) {
		const Pixel* srcLinePtr = srcLinePtrs[y];
		for (unsigned x = 0; x < srcWidth; x++) {
			Pixel col = srcLinePtr[x];
			for (unsigned iy = 0; iy < zoomFactorY; iy++) {
				Pixel* dstLinePtr = dst.getLinePtrDirect<Pixel>(dstY + iy);
				for (unsigned ix = 0; ix < zoomFactorX; ix++) {
					dstLinePtr[x * zoomFactorX + ix] = col;
				}
			}
		}
		dstY += zoomFactorY;
	}
	// Find horizontal L-shapes and anti-alias them.
	dstY = dstStartY;
	edgePtr = edges;
	for (int y = 0; y < srcNumLines; y++) {
		unsigned x = 0;
		while (x < srcWidth) {
			// Check which corners are part of a slope.
			bool slopeTopLeft = false;
			bool slopeTopRight = false;
			bool slopeBotLeft = false;
			bool slopeBotRight = false;

			// Search for slopes on the top edge.
			unsigned topEndX = x + 1;
			// TODO: Making the slopes end in the middle of the edge segment
			//       is simple but inaccurate. Can we do better?
			// Four cases:
			// -- no slope
			// /- left slope
			// -\ right slope
			// /\ U-shape
			if (edgePtr[x] & UP) {
				while (topEndX < srcWidth
					&& (edgePtr[topEndX] & (UP | LEFT)) == UP) topEndX++;
				slopeTopLeft = (edgePtr[x] & LEFT)
					&& srcLinePtrs[y + 1][x - 1] == srcLinePtrs[y][x]
					&& srcLinePtrs[y][x - 1] == srcLinePtrs[y - 1][x];
				slopeTopRight = (edgePtr[topEndX - 1] & RIGHT)
					&& srcLinePtrs[y + 1][topEndX] == srcLinePtrs[y][topEndX - 1]
					&& srcLinePtrs[y][topEndX] == srcLinePtrs[y - 1][topEndX - 1];
			}

			// Search for slopes on the bottom edge.
			unsigned botEndX = x + 1;
			if (edgePtr[x] & DOWN) {
				while (botEndX < srcWidth
					&& (edgePtr[botEndX] & (DOWN | LEFT)) == DOWN) botEndX++;
				slopeBotLeft = (edgePtr[x] & LEFT)
					&& srcLinePtrs[y - 1][x - 1] == srcLinePtrs[y][x]
					&& srcLinePtrs[y][x - 1] == srcLinePtrs[y + 1][x];
				slopeBotRight = (edgePtr[botEndX - 1] & RIGHT)
					&& srcLinePtrs[y - 1][botEndX] == srcLinePtrs[y][botEndX - 1]
					&& srcLinePtrs[y][botEndX] == srcLinePtrs[y + 1][botEndX - 1];
			}

			// Determine edge start and end points.
			const unsigned startX = x;
			assert(!slopeTopRight || !slopeBotRight || topEndX == botEndX);
			const unsigned endX = slopeTopRight ? topEndX : botEndX;
			// Determine what the next pixel is to check for edges.
			if (!(slopeTopLeft || slopeTopRight ||
				  slopeBotLeft || slopeBotRight)) {
				x++;
				// Nothing to render.
				continue;
			} else {
				x = endX;
			}

			// Antialias either the top or the bottom, but not both.
			// TODO: Figure out what the best way is to handle these situations.
			if (slopeTopLeft && slopeBotLeft) {
				slopeTopLeft = slopeBotLeft = false;
			}
			if (slopeTopRight && slopeBotRight) {
				slopeTopRight = slopeBotRight = false;
			}

			// Render slopes.
			const Pixel* srcTopLinePtr = srcLinePtrs[y - 1];
			const Pixel* srcCurLinePtr = srcLinePtrs[y];
			const Pixel* srcBotLinePtr = srcLinePtrs[y + 1];
			const unsigned x0 = startX * 2 * zoomFactorX;
			const unsigned x1 =
				  endX == srcWidth
				? srcWidth * 2 * zoomFactorX
				: ( slopeTopLeft || slopeBotLeft
				  ? (startX + endX) * zoomFactorX
				  : x0
				  );
			const unsigned x3 = endX * 2 * zoomFactorX;
			const unsigned x2 =
				  startX == 0
				? 0
				: ( slopeTopRight || slopeBotRight
				  ? (startX + endX) * zoomFactorX
				  : x3
				  );
			for (unsigned iy = 0; iy < zoomFactorY; iy++) {
				Pixel* dstLinePtr = dst.getLinePtrDirect<Pixel>(dstY + iy);

				// Figure out which parts of the line should be blended.
				bool blendTopLeft = false;
				bool blendTopRight = false;
				bool blendBotLeft = false;
				bool blendBotRight = false;
				if (iy * 2 < zoomFactorY) {
					blendTopLeft = slopeTopLeft;
					blendTopRight = slopeTopRight;
				}
				if (iy * 2 + 1 >= zoomFactorY) {
					blendBotLeft = slopeBotLeft;
					blendBotRight = slopeBotRight;
				}

				// Render left side.
				if (blendTopLeft || blendBotLeft) {
					// TODO: This is implied by !(slopeTopLeft && slopeBotLeft),
					//       which is ensured by a temporary measure.
					assert(!(blendTopLeft && blendBotLeft));
					const Pixel* srcMixLinePtr;
					float lineY;
					if (blendTopLeft) {
						srcMixLinePtr = srcTopLinePtr;
						lineY = (zoomFactorY - 1 - iy) / float(zoomFactorY);
					} else {
						srcMixLinePtr = srcBotLinePtr;
						lineY = iy / float(zoomFactorY);
					}
					for (unsigned fx = x0 | 1; fx < x1; fx += 2) {
						float rx = (fx - x0) / float(x1 - x0);
						float ry = 0.5f + rx * 0.5f;
						float weight = (ry - lineY) * zoomFactorY;
						dstLinePtr[fx / 2] = pixelOps.lerp(
							srcMixLinePtr[fx / (zoomFactorX * 2)],
							srcCurLinePtr[fx / (zoomFactorX * 2)],
							Math::clip<0, 256>(int(256 * weight))
							);
					}
				}

				// Render right side.
				if (blendTopRight || blendBotRight) {
					// TODO: This is implied by !(slopeTopRight && slopeBotRight),
					//       which is ensured by a temporary measure.
					assert(!(blendTopRight && blendBotRight));
					const Pixel* srcMixLinePtr;
					float lineY;
					if (blendTopRight) {
						srcMixLinePtr = srcTopLinePtr;
						lineY = (zoomFactorY - 1 - iy) / float(zoomFactorY);
					} else {
						srcMixLinePtr = srcBotLinePtr;
						lineY = iy / float(zoomFactorY);
					}
					// TODO: The weight is slightly too high for the middle
					//       pixel when zoomFactorX is odd and we are rendering
					//       a U-shape.
					for (unsigned fx = x2 | 1; fx < x3; fx += 2) {
						float rx = (fx - x2) / float(x3 - x2);
						float ry = 1.0f - rx * 0.5f;
						float weight = (ry - lineY) * zoomFactorY;
						dstLinePtr[fx / 2] = pixelOps.lerp(
							srcMixLinePtr[fx / (zoomFactorX * 2)],
							srcCurLinePtr[fx / (zoomFactorX * 2)],
							Math::clip<0, 256>(int(256 * weight))
							);
					}
				}

				// Draw edge indicators.
				if (false) {
					if (iy == 0) {
						if (slopeTopLeft) {
							for (unsigned fx = x0; fx < x1; fx += 2) {
								dstLinePtr[fx / 2] = (Pixel)0x00FF0000;
							}
						}
						if (slopeTopRight) {
							for (unsigned fx = x2; fx < x3; fx += 2) {
								dstLinePtr[fx / 2] = (Pixel)0x000000FF;
							}
						}
					} else if (iy == zoomFactorY - 1) {
						if (slopeBotLeft) {
							for (unsigned fx = x0; fx < x1; fx += 2) {
								dstLinePtr[fx / 2] = (Pixel)0x00FFFF00;
							}
						}
						if (slopeBotRight && iy == zoomFactorY - 1) {
							for (unsigned fx = x2; fx < x3; fx += 2) {
								dstLinePtr[fx / 2] = (Pixel)0x0000FF00;
							}
						}
					}
				}
			}
		}
		edgePtr += srcWidth;
		dstY += zoomFactorY;
	}
	// TODO: This is compensation for the fact that we do not support
	//       non-integer zoom factors yet.
	if (srcWidth * zoomFactorX != dstWidth) {
		for (unsigned dy = dstStartY; dy < dstY; dy++) {
			unsigned sy = std::min(
				(dy - dstStartY) / zoomFactorY - srcStartY,
				unsigned(srcNumLines)
				);
			Pixel col = srcLinePtrs[sy][srcWidth - 1];
			Pixel* dstLinePtr = dst.getLinePtrDirect<Pixel>(dy);
			for (unsigned dx = srcWidth * zoomFactorX; dx < dstWidth; dx++) {
				dstLinePtr[dx] = col;
			}
		}
	}
	if (dstY != dstEndY) {
		// Typically this will pick the border color, but there is no guarantee.
		// However, we're inside a workaround anyway, so it's good enough.
		Pixel col = srcLinePtrs[srcNumLines - 1][srcWidth - 1];
		for (unsigned dy = dstY; dy < dstEndY; dy++) {
			Pixel* dstLinePtr = dst.getLinePtrDirect<Pixel>(dy);
			for (unsigned dx = 0; dx < dstWidth; dx++) {
				dstLinePtr[dx] = col;
			}
		}
	}
	dst.unlock();

	src.freeLineBuffers();
}


// Force template instantiation.
#if HAVE_16BPP
template class MLAAScaler<word>;
#endif
#if HAVE_32BPP
template class MLAAScaler<unsigned>;
#endif

} // namespace openmsx
