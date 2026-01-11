/** \file Paper.cc
 * Paper/Page rendering implementation.
 * Extracted from Printer.cc for shared use.
 */

#include "Paper.hh"

#include "FileOperations.hh"
#include "PNG.hh"

#include "Math.hh"
#include "narrow.hh"
#include "small_buffer.hh"
#include "xrange.hh"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <ranges>

namespace openmsx {

Paper::Paper(unsigned x, unsigned y, double dotSizeX, double dotSizeY)
	: buf(size_t(x) * size_t(y))
	, colorBuf(size_t(x) * size_t(y) * 3, 255)
	, sizeX(x), sizeY(y)
{
	std::ranges::fill(buf, 255);
	std::ranges::fill(colorBuf, 255);
	setDotSize(dotSizeX, dotSizeY);
}

std::string Paper::save(bool color) const
{
	auto filename = FileOperations::getNextNumberedFileName(
		PRINT_DIR, "page", PRINT_EXTENSION);
	       if (color) {
		       // Save colorBuf as RGB PNG (each row is sizeX*3 bytes)
		       small_buffer<const uint8_t*, 4096> rowPointers(std::views::transform(xrange(sizeY),
			       [&](size_t y) { return &colorBuf[sizeX * 3 * y]; }));
		       PNG::saveRGB(sizeX, rowPointers, filename);
	       } else {
		       small_buffer<const uint8_t*, 4096> rowPointers(std::views::transform(xrange(sizeY),
			       [&](size_t y) { return &buf[sizeX * y]; }));
		       PNG::saveGrayscale(sizeX, rowPointers, filename);
	       }
	       return filename;
}

void Paper::plotColor(double x, double y, uint8_t r, uint8_t g, uint8_t b)
{
	int xx1 = std::max(int(floor(x - radiusX)), 0);
	int xx2 = std::min(int(ceil (x + radiusX)), int(sizeX));
	int yy1 = std::max(int(floor(y - radiusY)), 0);
	int yy2 = std::min(int(ceil (y + radiusY)), int(sizeY));

	int ytab = 16 * yy1 - int(16 * y) + 16 + radius16;
	for (auto yy : xrange(yy1, yy2)) {
		int xtab = 16 * xx1 - int(16 * x);
		for (auto xx : xrange(xx1, xx2)) {
			int sum = 0;
			for (auto i : xrange(16)) {
				int a = table[ytab + i];
				if (xtab < -a) {
					int t = 16 + a + xtab;
					if (t > 0) {
						sum += std::min(t, 2 * a);
					}
				} else {
					int t = a - xtab;
					if (t > 0) {
						sum += std::min(16, t);
					}
				}
			}
			unsigned idx = (yy * sizeX + xx) * 3;
			if (sum > 0 && idx + 2 < colorBuf.size()) {
				colorBuf[idx + 0] = r;
				colorBuf[idx + 1] = g;
				colorBuf[idx + 2] = b;
			}
			xtab += 16;
		}
		ytab += 16;
	}
}

void Paper::setDotSize(double dotSizeX, double dotSizeY)
{
	radiusX = dotSizeX * 0.5;
	radiusY = dotSizeY * 0.5;

	auto rx = int(16 * radiusX);
	auto ry = int(16 * radiusY);
	radius16 = ry;

	table.clear();
	table.resize(2 * (radius16 + 16), -(1 << 30));

	int offset = ry + 16;
	int rx2 = 2 * rx * rx;
	int ry2 = 2 * ry * ry;

	int x = 0;
	int y = ry;
	int de_x = ry * ry;
	int de_y = (1 - 2 * ry) * rx * rx;
	int e = 0;
	int sx = 0;
	int sy = rx2 * ry;
	while (sx <= sy) {
		table[offset - y - 1] = x;
		table[offset + y    ] = x;
		x    += 1;
		sx   += ry2;
		e    += de_x;
		de_x += ry2;
		if ((2 * e + de_y) > 0) {
			y    -= 1;
			sy   -= rx2;
			e    += de_y;
			de_y += rx2;
		}
	}

	x = rx;
	y = 0;
	de_x = (1 - 2 * rx) * ry * ry;
	de_y = rx * rx;
	e = 0;
	sx = ry2 * rx;
	sy = 0;
	while (sy <= sx) {
		table[offset - y - 1] = x;
		table[offset + y    ] = x;
		y    += 1;
		sy   += rx2;
		e    += de_y;
		de_y += rx2;
		if ((2 * e + de_x) > 0) {
			x -= 1;
			sx -= ry2;
			e += de_x;
			de_x += ry2;
		}
	}
}

void Paper::plot(double xPos, double yPos)
{
	int xx1 = std::max(int(floor(xPos - radiusX)), 0);
	int xx2 = std::min(int(ceil (xPos + radiusX)), int(sizeX));
	int yy1 = std::max(int(floor(yPos - radiusY)), 0);
	int yy2 = std::min(int(ceil (yPos + radiusY)), int(sizeY));

	int y = 16 * yy1 - int(16 * yPos) + 16 + radius16;
	for (auto yy : xrange(yy1, yy2)) {
		int x = 16 * xx1 - int(16 * xPos);
		for (auto xx : xrange(xx1, xx2)) {
			int sum = 0;
			for (auto i : xrange(16)) {
				int a = table[y + i];
				if (x < -a) {
					int t = 16 + a + x;
					if (t > 0) {
						sum += std::min(t, 2 * a);
					}
				} else {
					int t = a - x;
					if (t > 0) {
						sum += std::min(16, t);
					}
				}
			}
			dot(xx, yy) = narrow<uint8_t>(std::max(0, dot(xx, yy) - sum));
			x += 16;
		}
		y += 16;
	}
}

uint8_t& Paper::dot(unsigned x, unsigned y)
{
	assert(x < sizeX);
	assert(y < sizeY);
	return buf[y * sizeX + x];
}

} // namespace openmsx
