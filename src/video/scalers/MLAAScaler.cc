#include "MLAAScaler.hh"
#include "FrameSource.hh"
#include "ScalerOutput.hh"
#include "MemBuffer.hh"
#include "aligned.hh"
#include "vla.hh"
#include "build-info.hh"
#include <algorithm>
#include <vector>
#include <cassert>
#include <cstdint>

namespace openmsx {

template<std::unsigned_integral Pixel>
MLAAScaler<Pixel>::MLAAScaler(
		unsigned dstWidth_, const PixelOperations<Pixel>& pixelOps_)
	: pixelOps(pixelOps_)
	, dstWidth(dstWidth_)
{
}

template<std::unsigned_integral Pixel>
void MLAAScaler<Pixel>::scaleImage(
		FrameSource& src, const RawFrame* superImpose,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	(void)superImpose; // TODO: Support superimpose.
	//fprintf(stderr, "scale line [%d..%d) to [%d..%d), width %d to %d\n",
	//	srcStartY, srcEndY, dstStartY, dstEndY, srcWidth, dstWidth);

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
	auto** srcLinePtrs = &srcLinePtrsArray[1];
	std::vector<MemBuffer<Pixel, SSE_ALIGNMENT>> workBuffer;
	const Pixel* linePtr = nullptr;
	Pixel* work = nullptr;
	for (auto y : xrange(-1, srcNumLines + 1)) {
		if (linePtr == work) {
			// Allocate new workBuffer when needed
			// e.g. when used in previous iteration
			work = workBuffer.emplace_back(srcWidth).data();
		}
		auto line = src.getLine(srcStartY + y, std::span{work, srcWidth});
		linePtr = line.data();
		srcLinePtrs[y] = linePtr;
	}

	enum { UP = 1 << 0, RIGHT = 1 << 1, DOWN = 1 << 2, LEFT = 1 << 3 };
	MemBuffer<uint8_t> edges(srcNumLines * srcWidth);
	uint8_t* edgeGenPtr = edges.data();
	for (auto y : xrange(srcNumLines)) {
		auto* srcLinePtr = srcLinePtrs[y];
		for (auto x : xrange(srcWidth)) {
			Pixel colMid = srcLinePtr[x];
			uint8_t pixEdges = 0;
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
		EDGE_START      = 3 << 28,
		EDGE_END        = 2 << 28,
		EDGE_INNER      = 1 << 28,
		EDGE_NONE       = 0 << 28,
		EDGE_MASK       = 3 << 28,
		// Is the edge is part of one or more slopes?
		// And if so, what is the direction of the slope(s)?
		SLOPE_TOP_LEFT  = 1 << 27,
		SLOPE_TOP_RIGHT = 1 << 26,
		SLOPE_BOT_LEFT  = 1 << 25,
		SLOPE_BOT_RIGHT = 1 << 24,
		// How long is this edge?
		// For the start and end, these bits contain the length.
		// For inner pixels, these bits contain the distance to the start pixel.
		SPAN_BITS       = 12,
		SPAN_SHIFT_1    = 0,
		SPAN_SHIFT_2    = SPAN_SHIFT_1 + SPAN_BITS,
		SPAN_MASK       = (1 << SPAN_BITS) - 1,
	};
	assert(srcWidth <= SPAN_MASK);

	// Find horizontal edges.
	MemBuffer<unsigned> horizontals(srcNumLines * srcWidth);
	unsigned* horizontalGenPtr = horizontals.data();
	const uint8_t* edgePtr = edges.data();
	for (auto y : xrange(srcNumLines)) {
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

			// Store info about edge and determine next pixel to check.
			if (!(slopeTopLeft || slopeTopRight ||
				  slopeBotLeft || slopeBotRight)) {
				*horizontalGenPtr++ = EDGE_NONE;
				x++;
			} else {
				unsigned slopes =
					  (slopeTopLeft  ? SLOPE_TOP_LEFT  : 0)
					| (slopeTopRight ? SLOPE_TOP_RIGHT : 0)
					| (slopeBotLeft  ? SLOPE_BOT_LEFT  : 0)
					| (slopeBotRight ? SLOPE_BOT_RIGHT : 0);
				const unsigned lengths =
					((topEndX - x) << SPAN_SHIFT_1) |
					((botEndX - x) << SPAN_SHIFT_2);
				// Determine edge start and end points.
				assert(!slopeTopRight || !slopeBotRight || topEndX == botEndX);
				const unsigned endX = slopeTopRight ? topEndX : (
					slopeBotRight ? botEndX : std::max(topEndX, botEndX)
					);
				const unsigned length = endX - x;
				if (length == 1) {
					*horizontalGenPtr++ = EDGE_START | EDGE_END | slopes | lengths;
				} else {
					*horizontalGenPtr++ = EDGE_START | slopes | lengths;
					for (auto i : xrange(1u, length - 1)) {
						*horizontalGenPtr++ = EDGE_INNER | slopes | i;
					}
					*horizontalGenPtr++ = EDGE_END | slopes | lengths;
				}
				x = endX;
			}
		}
		assert(x == srcWidth);
		edgePtr += srcWidth;
	}
	assert(unsigned(edgePtr - edges.data()) == srcNumLines * srcWidth);
	assert(unsigned(horizontalGenPtr - horizontals.data()) == srcNumLines * srcWidth);

	// Find vertical edges.
	MemBuffer<unsigned> verticals(srcNumLines * srcWidth);
	edgePtr = edges.data();
	for (auto x : xrange(srcWidth)) {
		unsigned* verticalGenPtr = &verticals[x];
		int y = 0;
		while (y < srcNumLines) {
			// Check which corners are part of a slope.
			bool slopeTopLeft = false;
			bool slopeTopRight = false;
			bool slopeBotLeft = false;
			bool slopeBotRight = false;

			// Search for slopes on the left edge.
			int leftEndY = y + 1;
			if (edgePtr[y * srcWidth] & LEFT) {
				while (leftEndY < srcNumLines
					&& (edgePtr[leftEndY * srcWidth] & (LEFT | UP)) == LEFT) leftEndY++;
				assert(x > 0); // implied by having a left edge
				const unsigned nextX = std::min(x + 1, srcWidth - 1);
				slopeTopLeft = (edgePtr[y * srcWidth] & UP)
					&& srcLinePtrs[y - 1][nextX] == srcLinePtrs[y][x]
					&& srcLinePtrs[y - 1][x] == srcLinePtrs[y][x - 1];
				slopeBotLeft = (edgePtr[(leftEndY - 1) * srcWidth] & DOWN)
					&& srcLinePtrs[leftEndY][nextX] == srcLinePtrs[leftEndY - 1][x]
					&& srcLinePtrs[leftEndY][x] == srcLinePtrs[leftEndY - 1][x - 1];
			}

			// Search for slopes on the right edge.
			int rightEndY = y + 1;
			if (edgePtr[y * srcWidth] & RIGHT) {
				while (rightEndY < srcNumLines
					&& (edgePtr[rightEndY * srcWidth] & (RIGHT | UP)) == RIGHT) rightEndY++;
				assert(x < srcWidth); // implied by having a right edge
				const unsigned prevX = x == 0 ? 0 : x - 1;
				slopeTopRight = (edgePtr[y * srcWidth] & UP)
					&& srcLinePtrs[y - 1][prevX] == srcLinePtrs[y][x]
					&& srcLinePtrs[y - 1][x] == srcLinePtrs[y][x + 1];
				slopeBotRight = (edgePtr[(rightEndY - 1) * srcWidth] & DOWN)
					&& srcLinePtrs[rightEndY][prevX] == srcLinePtrs[rightEndY - 1][x]
					&& srcLinePtrs[rightEndY][x] == srcLinePtrs[rightEndY - 1][x + 1];
			}

			// Store info about edge and determine next pixel to check.
			if (!(slopeTopLeft || slopeTopRight ||
				  slopeBotLeft || slopeBotRight)) {
				*verticalGenPtr = EDGE_NONE;
				verticalGenPtr += srcWidth;
				y++;
			} else {
				unsigned slopes =
					  (slopeTopLeft  ? SLOPE_TOP_LEFT  : 0)
					| (slopeTopRight ? SLOPE_TOP_RIGHT : 0)
					| (slopeBotLeft  ? SLOPE_BOT_LEFT  : 0)
					| (slopeBotRight ? SLOPE_BOT_RIGHT : 0);
				const unsigned lengths =
					((leftEndY  - y) << SPAN_SHIFT_1) |
					((rightEndY - y) << SPAN_SHIFT_2);
				// Determine edge start and end points.
				assert(!slopeBotLeft || !slopeBotRight || leftEndY == rightEndY);
				const unsigned endY = slopeBotLeft ? leftEndY : (
					slopeBotRight ? rightEndY : std::max(leftEndY, rightEndY)
					);
				const unsigned length = endY - y;
				if (length == 1) {
					*verticalGenPtr = EDGE_START | EDGE_END | slopes | lengths;
					verticalGenPtr += srcWidth;
				} else {
					*verticalGenPtr = EDGE_START | slopes | lengths;
					verticalGenPtr += srcWidth;
					for (auto i : xrange(1u, length - 1)) {
						// TODO: To be fully accurate we need to have separate
						//       start/stop points for the two possible edges
						//       of this pixel. No code uses the inner info yet
						//       though, so this can be fixed later.
						*verticalGenPtr = EDGE_INNER | slopes | i;
						verticalGenPtr += srcWidth;
					}
					*verticalGenPtr = EDGE_END | slopes | lengths;
					verticalGenPtr += srcWidth;
				}
				y = endY;
			}
		}
		assert(y == srcNumLines);
		assert(unsigned(verticalGenPtr - verticals.data()) == x + srcNumLines * srcWidth);
		edgePtr++;
	}
	assert(unsigned(edgePtr - edges.data()) == srcWidth);

	VLA(std::span<Pixel>, dstLines, dst.getHeight());
	for (auto i : xrange(dstStartY, dstEndY)) {
		dstLines[i] = dst.acquireLine(i);
	}

	// Do a mosaic scale so every destination pixel has a color.
	unsigned dstY = dstStartY;
	for (auto y : xrange(srcNumLines)) {
		auto* srcLinePtr = srcLinePtrs[y];
		for (auto x : xrange(srcWidth)) {
			Pixel col = srcLinePtr[x];
			for (auto iy : xrange(zoomFactorY)) {
				for (auto ix : xrange(zoomFactorX)) {
					dstLines[dstY + iy][x * zoomFactorX + ix] = col;
				}
			}
		}
		dstY += zoomFactorY;
	}

	// Render the horizontal edges.
	const unsigned* horizontalPtr = horizontals.data();
	dstY = dstStartY;
	for (auto y : xrange(srcNumLines)) {
		unsigned x = 0;
		while (x < srcWidth) {
			// Fetch information about the edge, if any, at the current pixel.
			unsigned horzInfo = *horizontalPtr;
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
			const unsigned topEndX =
				startX + ((horzInfo >> SPAN_SHIFT_1) & SPAN_MASK);
			const unsigned botEndX =
				startX + ((horzInfo >> SPAN_SHIFT_2) & SPAN_MASK);
			// Determine edge start and end points.
			assert(!slopeTopRight || !slopeBotRight || topEndX == botEndX);
			const unsigned endX = slopeTopRight ? topEndX : (
				slopeBotRight ? botEndX : std::max(topEndX, botEndX)
				);
			x = endX;
			horizontalPtr += endX - startX;

			// Antialias either the top or the bottom, but not both.
			// TODO: Figure out what the best way is to handle these situations.
			if (slopeTopLeft && slopeBotLeft) {
				slopeTopLeft = slopeBotLeft = false;
			}
			if (slopeTopRight && slopeBotRight) {
				slopeTopRight = slopeBotRight = false;
			}

			// Render slopes.
			auto* srcTopLinePtr = srcLinePtrs[y - 1];
			auto* srcCurLinePtr = srcLinePtrs[y + 0];
			auto* srcBotLinePtr = srcLinePtrs[y + 1];
			const unsigned x0 = startX * 2 * zoomFactorX;
			const unsigned x1 =
				  slopeTopLeft
				? (startX + topEndX) * zoomFactorX
				: (slopeBotLeft
				  ? (startX + botEndX) * zoomFactorX
				  : x0);
			const unsigned x3 = endX * 2 * zoomFactorX;
			const unsigned x2 =
				  slopeTopRight
				? (startX + topEndX) * zoomFactorX
				: (slopeBotRight
				  ? (startX + botEndX) * zoomFactorX
				  : x3);
			for (auto iy : xrange(zoomFactorY)) {
				auto dstLinePtr = dstLines[dstY + iy];

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
					const Pixel* srcMixLinePtr = blendTopLeft
						? srcTopLinePtr
						: srcBotLinePtr;
					float lineY = blendTopLeft
						? ((zoomFactorY - 1 - iy) / float(zoomFactorY))
						: (iy / float(zoomFactorY));
					for (unsigned fx = x0 | 1; fx < x1; fx += 2) {
						float rx = (fx - x0) / float(x1 - x0);
						float ry = 0.5f + rx * 0.5f;
						float weight = (ry - lineY) * zoomFactorY;
						dstLinePtr[fx / 2] = pixelOps.lerp(
							srcMixLinePtr[fx / (zoomFactorX * 2)],
							srcCurLinePtr[fx / (zoomFactorX * 2)],
							std::clamp(int(256 * weight), 0, 256));
					}
				}

				// Render right side.
				if (blendTopRight || blendBotRight) {
					// TODO: This is implied by !(slopeTopRight && slopeBotRight),
					//       which is ensured by a temporary measure.
					assert(!(blendTopRight && blendBotRight));
					const Pixel* srcMixLinePtr = blendTopRight
						? srcTopLinePtr
						: srcBotLinePtr;
					float lineY = blendTopRight
						? ((zoomFactorY - 1 - iy) / float(zoomFactorY))
						: (iy / float(zoomFactorY));
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
							std::clamp(int(256 * weight), 0, 256));
					}
				}

				// Draw horizontal edge indicators.
				if (false) {
					if (iy == 0) {
						if (slopeTopLeft) {
							for (unsigned fx = x0 | 1; fx < x1; fx += 2) {
								dstLinePtr[fx / 2] =
									pixelOps.combine256(255, 0, 0);
							}
						}
						if (slopeTopRight) {
							for (unsigned fx = x2 | 1; fx < x3; fx += 2) {
								dstLinePtr[fx / 2] =
									pixelOps.combine256(0, 0, 255);
							}
						}
					} else if (iy == zoomFactorY - 1) {
						if (slopeBotLeft) {
							for (unsigned fx = x0 | 1; fx < x1; fx += 2) {
								dstLinePtr[fx / 2] =
									pixelOps.combine256(255, 255, 0);
							}
						}
						if (slopeBotRight) {
							for (unsigned fx = x2 | 1; fx < x3; fx += 2) {
								dstLinePtr[fx / 2] =
									pixelOps.combine256(0, 255, 0);
							}
						}
					}
				}
			}
		}
		assert(x == srcWidth);
		dstY += zoomFactorY;
	}
	assert(unsigned(horizontalPtr - horizontals.data()) == srcNumLines * srcWidth);

	// Render the vertical edges.
	for (auto x : xrange(srcWidth)) {
		const unsigned* verticalPtr = &verticals[x];
		int y = 0;
		while (y < srcNumLines) {
			// Fetch information about the edge, if any, at the current pixel.
			unsigned vertInfo = *verticalPtr;
			if ((vertInfo & EDGE_MASK) == EDGE_NONE) {
				y++;
				verticalPtr += srcWidth;
				continue;
			}
			assert((vertInfo & EDGE_MASK) == EDGE_START);

			// Check which corners are part of a slope.
			bool slopeTopLeft  = (vertInfo & SLOPE_TOP_LEFT ) != 0;
			bool slopeTopRight = (vertInfo & SLOPE_TOP_RIGHT) != 0;
			bool slopeBotLeft  = (vertInfo & SLOPE_BOT_LEFT ) != 0;
			bool slopeBotRight = (vertInfo & SLOPE_BOT_RIGHT) != 0;
			const unsigned startY = y;
			const unsigned leftEndY =
				startY + ((vertInfo >> SPAN_SHIFT_1) & SPAN_MASK);
			const unsigned rightEndY =
				startY + ((vertInfo >> SPAN_SHIFT_2) & SPAN_MASK);
			// Determine edge start and end points.
			assert(!slopeBotLeft || !slopeBotRight || leftEndY == rightEndY);
			const unsigned endY = slopeBotLeft ? leftEndY : (
				slopeBotRight ? rightEndY : std::max(leftEndY, rightEndY)
				);
			y = endY;
			verticalPtr += srcWidth * (endY - startY);

			// Antialias either the left or the right, but not both.
			if (slopeTopLeft && slopeTopRight) {
				slopeTopLeft = slopeTopRight = false;
			}
			if (slopeBotLeft && slopeBotRight) {
				slopeBotLeft = slopeBotRight = false;
			}

			// Render slopes.
			const unsigned leftX = x == 0 ? 0 : x - 1;
			const unsigned curX = x;
			const unsigned rightX = std::min(x + 1, srcWidth - 1);
			const unsigned y0 = startY * 2 * zoomFactorY;
			const unsigned y1 =
				  slopeTopLeft
				? (startY + leftEndY) * zoomFactorY
				: (slopeTopRight
				  ? (startY + rightEndY) * zoomFactorY
				  : y0);
			const unsigned y3 = endY * 2 * zoomFactorY;
			const unsigned y2 =
				  slopeBotLeft
				? (startY + leftEndY) * zoomFactorY
				: (slopeBotRight
				  ? (startY + rightEndY) * zoomFactorY
				  : y3);
			for (auto ix : xrange(zoomFactorX)) {
				const unsigned fx = x * zoomFactorX + ix;

				// Figure out which parts of the line should be blended.
				bool blendTopLeft = false;
				bool blendTopRight = false;
				bool blendBotLeft = false;
				bool blendBotRight = false;
				if (ix * 2 < zoomFactorX) {
					blendTopLeft = slopeTopLeft;
					blendBotLeft = slopeBotLeft;
				}
				if (ix * 2 + 1 >= zoomFactorX) {
					blendTopRight = slopeTopRight;
					blendBotRight = slopeBotRight;
				}

				// Render top side.
				if (blendTopLeft || blendTopRight) {
					assert(!(blendTopLeft && blendTopRight));
					unsigned mixX = blendTopLeft ? leftX : rightX;
					float lineX = blendTopLeft
						? ((zoomFactorX - 1 - ix) / float(zoomFactorX))
						: (ix / float(zoomFactorX));
					for (unsigned fy = y0 | 1; fy < y1; fy += 2) {
						auto dstLinePtr = dstLines[dstStartY + fy / 2];
						float ry = (fy - y0) / float(y1 - y0);
						float rx = 0.5f + ry * 0.5f;
						float weight = (rx - lineX) * zoomFactorX;
						dstLinePtr[fx] = pixelOps.lerp(
							srcLinePtrs[fy / (zoomFactorY * 2)][mixX],
							srcLinePtrs[fy / (zoomFactorY * 2)][curX],
							std::clamp(int(256 * weight), 0, 256));
					}
				}

				// Render bottom side.
				if (blendBotLeft || blendBotRight) {
					assert(!(blendBotLeft && blendBotRight));
					unsigned mixX = blendBotLeft ? leftX : rightX;
					float lineX = blendBotLeft
						? ((zoomFactorX - 1 - ix) / float(zoomFactorX))
						: (ix / float(zoomFactorX));
					for (unsigned fy = y2 | 1; fy < y3; fy += 2) {
						auto dstLinePtr = dstLines[dstStartY + fy / 2];
						float ry = (fy - y2) / float(y3 - y2);
						float rx = 1.0f - ry * 0.5f;
						float weight = (rx - lineX) * zoomFactorX;
						dstLinePtr[fx] = pixelOps.lerp(
							srcLinePtrs[fy / (zoomFactorY * 2)][mixX],
							srcLinePtrs[fy / (zoomFactorY * 2)][curX],
							std::clamp(int(256 * weight), 0, 256));
					}
				}

				// Draw vertical edge indicators.
				if (false) {
					if (ix == 0) {
						if (slopeTopLeft) {
							for (unsigned fy = y0 | 1; fy < y1; fy += 2) {
								auto dstLinePtr = dstLines[
									dstStartY + fy / 2];
								dstLinePtr[fx] =
									pixelOps.combine256(255, 0, 0);
							}
						}
						if (slopeBotLeft) {
							for (unsigned fy = y2 | 1; fy < y3; fy += 2) {
								auto dstLinePtr = dstLines[
									dstStartY + fy / 2];
								dstLinePtr[fx] =
									pixelOps.combine256(255, 255, 0);
							}
						}
					} else if (ix == zoomFactorX - 1) {
						if (slopeTopRight) {
							for (unsigned fy = y0 | 1; fy < y1; fy += 2) {
								auto dstLinePtr = dstLines[
									dstStartY + fy / 2];
								dstLinePtr[fx] =
									pixelOps.combine256(0, 0, 255);
							}
						}
						if (slopeBotRight) {
							for (unsigned fy = y2 | 1; fy < y3; fy += 2) {
								auto dstLinePtr = dstLines[
									dstStartY + fy / 2];
								dstLinePtr[fx] =
									pixelOps.combine256(0, 255, 0);
							}
						}
					}
				}
			}
		}
		assert(y == srcNumLines);
	}

	// TODO: This is compensation for the fact that we do not support
	//       non-integer zoom factors yet.
	if (srcWidth * zoomFactorX != dstWidth) {
		for (auto dy : xrange(dstStartY, dstY)) {
			unsigned sy = std::min(
				(dy - dstStartY) / zoomFactorY - srcStartY,
				unsigned(srcNumLines)
				);
			Pixel col = srcLinePtrs[sy][srcWidth - 1];
			for (auto dx : xrange(srcWidth * zoomFactorX, dstWidth)) {
				dstLines[dy][dx] = col;
			}
		}
	}
	if (dstY != dstEndY) {
		// Typically this will pick the border color, but there is no guarantee.
		// However, we're inside a workaround anyway, so it's good enough.
		Pixel col = srcLinePtrs[srcNumLines - 1][srcWidth - 1];
		for (auto dy : xrange(dstY, dstEndY)) {
			for (auto dx : xrange(dstWidth)) {
				dstLines[dy][dx] = col;
			}
		}
	}

	for (auto i : xrange(dstStartY, dstEndY)) {
		dst.releaseLine(i, dstLines[i]);
	}
}

// Force template instantiation.
#if HAVE_16BPP
template class MLAAScaler<uint16_t>;
#endif
#if HAVE_32BPP
template class MLAAScaler<uint32_t>;
#endif

} // namespace openmsx
