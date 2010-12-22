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
	byte* edgeGenPtr = edges;
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
			*edgeGenPtr++ = pixEdges;
		}
	}

	enum {
		// Is this pixel part of an edge?
		// And if so, where on the edge is it?
		EDGE_START      = 3 << 14,
		EDGE_END        = 2 << 14,
		EDGE_INNER      = 1 << 14,
		EDGE_NONE       = 0 << 14,
		EDGE_MASK       = 3 << 14,
		// Is the edge is part of one or more slopes?
		// And if so, what is the direction of the slope(s)?
		SLOPE_TOP_LEFT  = 1 << 13,
		SLOPE_TOP_RIGHT = 1 << 12,
		SLOPE_BOT_LEFT  = 1 << 11,
		SLOPE_BOT_RIGHT = 1 << 10,
		// How long is this edge?
		// For the start and end, these bits contain the length.
		// For inner pixels, these bits contain the distance to the start pixel.
		SPAN_MASK       = (1 << 10) - 1
	};
	assert(srcWidth <= SPAN_MASK);

	// Find horizontal edges.
	VLA(word, horizontals, srcNumLines * srcWidth);
	word* horizontalGenPtr = horizontals;
	const byte* edgePtr = edges;
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
			const unsigned endX = slopeTopRight ? topEndX : (
				slopeBotRight ? botEndX : std::max(topEndX, botEndX)
				);

			// Store info about edge and determine next pixel to check.
			if (!(slopeTopLeft || slopeTopRight ||
				  slopeBotLeft || slopeBotRight)) {
				*horizontalGenPtr++ = EDGE_NONE;
				x++;
			} else {
				word slopes =
					  (slopeTopLeft  ? SLOPE_TOP_LEFT  : 0)
					| (slopeTopRight ? SLOPE_TOP_RIGHT : 0)
					| (slopeBotLeft  ? SLOPE_BOT_LEFT  : 0)
					| (slopeBotRight ? SLOPE_BOT_RIGHT : 0);
				word length = endX - startX;
				if (length == 1) {
					*horizontalGenPtr++ = EDGE_START | EDGE_END | slopes | 1;
				} else {
					*horizontalGenPtr++ = EDGE_START | slopes | length;
					for (word i = 1; i < length - 1; i++) {
						*horizontalGenPtr++ = EDGE_INNER | slopes | i;
					}
					*horizontalGenPtr++ = EDGE_END | slopes | length;
				}
				x = endX;
			}
		}
		assert(x == srcWidth);
		edgePtr += srcWidth;
	}
	assert(edgePtr - edges == srcNumLines * srcWidth);

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

	// Render the edges.
	const word* horizontalPtr = horizontals;
	dstY = dstStartY;
	for (int y = 0; y < srcNumLines; y++) {
		unsigned x = 0;
		while (x < srcWidth) {
			// Fetch information about the edge, if any, at the current pixel.
			word horzInfo = *horizontalPtr;
			if ((horzInfo & EDGE_MASK) == EDGE_NONE) {
				x++;
				horizontalPtr++;
				continue;
			}
			assert((horzInfo & EDGE_MASK) == EDGE_START);

			// Check which corners are part of a slope.
			bool slopeTopLeft  = (horzInfo & SLOPE_TOP_LEFT ) != 0;
			bool slopeTopRight = (horzInfo & SLOPE_TOP_RIGHT) != 0;
			bool slopeBotLeft  = (horzInfo & SLOPE_BOT_LEFT ) != 0;
			bool slopeBotRight = (horzInfo & SLOPE_BOT_RIGHT) != 0;
			const unsigned startX = x;
			const unsigned endX = x + (horzInfo & SPAN_MASK);
			x = endX;
			horizontalPtr += endX - startX;

			// Antialias either the top or the bottom, but not both.
			// TODO: Figure out what the best way is to handle these situations.
			if (slopeTopLeft && slopeBotLeft) {
				// TODO: This masks the fact that if we have two slopes on a
				//       single line we don't necessarily have a common end
				//       point: endX might be different from topEndX or botEndX.
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
							for (unsigned fx = x0 | 1; fx < x1; fx += 2) {
								dstLinePtr[fx / 2] = (Pixel)0x00FF0000;
							}
						}
						if (slopeTopRight) {
							for (unsigned fx = x2 | 1; fx < x3; fx += 2) {
								dstLinePtr[fx / 2] = (Pixel)0x000000FF;
							}
						}
					} else if (iy == zoomFactorY - 1) {
						if (slopeBotLeft) {
							for (unsigned fx = x0 | 1; fx < x1; fx += 2) {
								dstLinePtr[fx / 2] = (Pixel)0x00FFFF00;
							}
						}
						if (slopeBotRight && iy == zoomFactorY - 1) {
							for (unsigned fx = x2 | 1; fx < x3; fx += 2) {
								dstLinePtr[fx / 2] = (Pixel)0x0000FF00;
							}
						}
					}
				}
			}
		}
		assert(x == srcWidth);
		dstY += zoomFactorY;
	}
	assert(horizontalPtr - horizontals == srcNumLines * srcWidth);

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
