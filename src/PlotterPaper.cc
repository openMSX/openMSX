#include "PlotterPaper.hh"

#include "ranges.hh"

#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <span>

namespace openmsx {

// Table containing half circle-width for every y-position.
// Zoomed in factor 16x, so:
// * y-coordinates are 16x larger
// * reported width is also 16x larger
static std::span<int> get_dot_table(int radius16)
{
	static std::map<int, MemBuffer<int>> cache;
	auto [it, inserted] = cache.try_emplace(radius16);
	auto& table = it->second;
	if (inserted) { // was not yet present
		// Table covers full vertical extent: y from -(radius16+16) to +(radius16+16)
		table.resize(2 * (radius16 + 16));

		int offset = radius16 + 16;  // Center of table (y=0 relative to circle center)
		int r_sq = radius16 * radius16;

		// Compute half-widths for all y positions relative to circle center
		for (int y_rel = -(radius16 + 16); y_rel < (radius16 + 16); ++y_rel) {
			int y_sq = y_rel * y_rel;

			if (y_sq > r_sq) {
				// Outside circle
				table[offset + y_rel] = 0;
			} else {
				// Inside circle: compute half-width
				// a = sqrt(r^2 - y^2)
				float a_float = std::sqrt(float(r_sq - y_sq));
				table[offset + y_rel] = int(a_float);
			}
		}
	}
	return table;
}

// Rasterize a stationary dot (pen dwells at a point)
// ink_color: color of ink (gl::vec3{r,g,b} for RGB, or float for grayscale)
//     For subtractive model: 0=no ink (white), 1=full ink (black)
//     To control ink amount, pre-multiply ink_color (e.g., ink_color * 0.5f)
// Uses 16×16 sub-pixel grid (256 shading levels)
template<typename InkType>
void rasterize_dot(Canvas<InkType>& canvas, gl::vec2 center, float pen_radius, InkType ink_color)
{
	// Compute pixel bounding box
	auto bbox_min = max(gl::ivec2{0}, floor(center - gl::vec2(pen_radius)));
	auto bbox_max = min(canvas.size() - gl::ivec2{1}, ceil(center + gl::vec2(pen_radius)));

	// Early exit if no pixels to draw
	if (bbox_min.x >= bbox_max.x || bbox_min.y >= bbox_max.y) return;

	// Quantized center position (in 1/16 pixel units)
	int xPos16 = int(16.0f * center.x);
	int yPos16 = int(16.0f * center.y);
	int radius16 = int(16.0f * pen_radius);
	auto table = get_dot_table(radius16);

	// Starting y offset in lookup table
	int y = 16 * bbox_min.y - yPos16 + 16 + radius16;

	for (int yy = bbox_min.y; yy < bbox_max.y; ++yy, y += 16) {
		auto line = canvas.getLine(yy);
		auto table_slice = subspan<16>(table, y);

		// Calculate which pixel contains the Y-axis (x=0, circle center)
		int x_start = 16 * bbox_min.x - xPos16;

		// Find the pixel that crosses the Y-axis
		// Y-axis at x=0, middle pixel has x in [-16, 0)
		int xx_mid = bbox_min.x + (-x_start + 15) / 16;
		xx_mid = std::clamp(xx_mid, bbox_min.x, bbox_max.x - 1);

		// Left half: pixels before middle (x+16 < 0)
		int x = x_start;
		for (int xx = bbox_min.x; xx < xx_mid; ++xx) {
			int sum = 0;
			for (int i = 0; i < 16; ++i) {
				int a = table_slice[i];
				int coverage = std::clamp(16 + a + x, 0, 16);
				sum += coverage;
			}
			// Accumulate into float canvas (normalized to [0, 1])
			line[xx] += ink_color * (sum / 256.0f);
			x += 16;
		}

		// Middle pixel: exactly 1 (if Y-axis within bounding box)
		if (xx_mid < bbox_max.x) {
			int sum = 0;
			for (int i = 0; i < 16; ++i) {
				int a = table_slice[i];
				int left_coverage = std::clamp(16 + a + x, 0, 2 * a);
				int right_coverage = std::clamp(a - x, 0, 16);
				int coverage = (x < -a) ? left_coverage : right_coverage;
				sum += coverage;
			}
			line[xx_mid] += ink_color * (sum / 256.0f);
			x += 16;
		}

		// Right half: pixels after middle (x >= 0)
		for (int xx = xx_mid + 1; xx < bbox_max.x; ++xx) {
			int sum = 0;
			for (int i = 0; i < 16; ++i) {
				int a = table_slice[i];
				int coverage = std::clamp(a - x, 0, 16);
				sum += coverage;
			}
			line[xx] += ink_color * (sum / 256.0f);
			x += 16;
		}
	}
}


// Analytical uniform disc: compute ink contribution from pen motion along a segment
// Does NOT include dwell time at endpoints - add those separately if needed
// Ink is automatically normalized: alpha=1.0 at center of long segment
// Caller should normalize endpoint dwells by dividing by (2*pen_radius) for consistency
// Uses 8x MSAA for anti-aliasing
template<typename InkType>
void rasterize_segment_motion(Canvas<InkType>& canvas, gl::vec2 A, gl::vec2 B, float pen_radius, InkType ink_color)
{
	// MSAA 8x sample pattern (positions within pixel, in range [0, 1])
	// (0,0) = top-left corner, (1,1) = bottom-right corner, (0.5,0.5) = center
	// Rotated grid pattern for good quality anti-aliasing
	static constexpr std::array<gl::vec2, 8> msaa_8x = {{
		{0.125f, 0.375f}, {0.625f, 0.125f}, {0.375f, 0.875f}, {0.875f, 0.625f},
		{0.125f, 0.875f}, {0.875f, 0.125f}, {0.375f, 0.375f}, {0.625f, 0.625f}
	}};

	auto D = B - A;
	float L_sq = dot(D, D);
	if (L_sq < 1e-18f) return; // degenerate segment

	float L = std::sqrt(L_sq);
	gl::vec2 T = D * (1.0f / L); // unit tangent
	gl::vec2 N = {-T.y, T.x};    // unit normal

	// Pre-transform MSAA samples into (T, N) coordinate system (once per segment)
	// Exploits bilinearity: dot(a+b, c) = dot(a, c) + dot(b, c)
	std::array<gl::vec2, msaa_8x.size()> msaa_TN;
	for (size_t i = 0; i < msaa_8x.size(); ++i) {
		msaa_TN[i] = {dot(msaa_8x[i], T), dot(msaa_8x[i], N)};
	}

	// Vertical bounding box (using vector math, but only y component is used here)
	auto segment_min = min(A, B);
	auto segment_max = max(A, B);
	int y_min = std::max(0, int(std::floor(segment_min.y - pen_radius)));
	int y_max = std::min(canvas.size().y - 1, int(std::ceil(segment_max.y + pen_radius)));

	// Normalization: max ink is 2*pen_radius, scale by 1/(2*pen_radius*num_samples) for MSAA
	// Pre-multiply by 2 to optimize the common fast-path (avoids multiply in inner loop)
	constexpr float scale_factor = 1.0f / (2.0f * msaa_8x.size());
	const float pen_radius_sq = pen_radius * pen_radius;
	const InkType ink_contribution_double = ink_color * (2.0f * scale_factor / pen_radius);

	// Lambda to process a pixel at (x, dy) and write to line if non-zero
	// dy is the y-coordinate relative to A.y (computed once per scanline)
	auto process_pixel = [&](std::span<InkType> line, int x, float dy) {
		// Transform pixel position into (T, N) coordinate system (once per pixel)
		gl::vec2 pixel_minus_A = {float(x) - A.x, dy};
		gl::vec2 pixel_TN = {dot(pixel_minus_A, T), dot(pixel_minus_A, N)};

		// Accumulate half-ink (optimizes fast path: avoids *2 multiply for middle section)
		float half_ink_sum = 0.0f;
		for (const auto& sample_tn : msaa_TN) {
			// Exploit bilinearity: distances are just additions in (T,N) space
			float d_parallel = pixel_TN.x + sample_tn.x;  // Distance along line
			float d_perp = pixel_TN.y + sample_tn.y;      // Perpendicular distance (signed)
			float d_perp_sq = d_perp * d_perp;	      // Squared distance (positive)

			// Early rejection: too far perpendicular to line
			if (d_perp_sq > pen_radius_sq) continue;

			// Compute half-width of integration interval
			float h = std::sqrt(pen_radius_sq - d_perp_sq);

			// Fast path: middle section (most common case for long lines)
			// When h < d_parallel < L - h, the integration interval is 2*h
			// We accumulate h (not 2*h) and compensate with pre-doubled ink_contribution
			if (h < d_parallel && d_parallel < L - h) {
				half_ink_sum += h;
			} else {
				// Slow path: near start or end caps (needs clamping)
				float t_min = std::max(0.0f, d_parallel - h);
				float t_max = std::min(L, d_parallel + h);
				half_ink_sum += 0.5f * std::max(0.0f, t_max - t_min);
			}
		}
		if (half_ink_sum > 0.0f) {
			line[x] += ink_contribution_double * half_ink_sum;
		}
	};

	const float epsilon = 1e-6f;
	if (std::abs(T.y) > epsilon) {
		// Non-horizontal segment: use scanline intersection for tighter x bounds
		const float inv_Ty = 1.0f / T.y;
		const float dx_factor = (std::abs(N.x) > epsilon ? pen_radius / std::abs(N.x) : pen_radius);

		for (int y = y_min; y <= y_max; ++y) {
			auto line = canvas.getLine(y);
			float y_float = float(y);
			float dy = y_float - A.y;  // Relative to A (loop-invariant for x-loop)
			float Py = y_float + 0.5f;
			float t_center = (Py - A.y) * inv_Ty;
			float x_center = A.x + t_center * T.x;
			int x_min = std::max(0, (int)std::floor(x_center - dx_factor));
			int x_max = std::min(canvas.size().x - 1, (int)std::ceil(x_center + dx_factor));

			for (int x = x_min; x <= x_max; ++x) {
				process_pixel(line, x, dy);
			}
		}
	} else {
		// Nearly horizontal segment: use axis-aligned bounding box
		int x_min = std::max(0, int(std::floor(segment_min.x - pen_radius)));
		int x_max = std::min(canvas.size().x - 1, int(std::ceil(segment_max.x + pen_radius)));

		for (int y = y_min; y <= y_max; ++y) {
			auto line = canvas.getLine(y);
			float dy = float(y) - A.y;  // Relative to A (loop-invariant for x-loop)
			for (int x = x_min; x <= x_max; ++x) {
				process_pixel(line, x, dy);
			}
		}
	}
}

// Rasterize a polyline with proper endpoint dwell times
template<typename InkType>
void rasterize_polyline(
	Canvas<InkType>& canvas, std::span<const gl::vec2> pts,
	float pen_radius, InkType ink_color, float dwell_time = 1.0f)
{
	if (pts.size() < 2) {
		if (pts.size() == 1) {
			// Single point: just a dot with specified dwell time
			rasterize_dot(canvas, pts[0], pen_radius, ink_color * dwell_time);
		}
		return;
	}

	// Normalize dwell to ink amount and pre-multiply with ink_color
	const float ink_amount = dwell_time / (2.0f * pen_radius);
	const InkType endpoint_ink = ink_color * ink_amount;

	// Dwell at start point (pen lands)
	rasterize_dot(canvas, pts[0], pen_radius, endpoint_ink);

	// All segments (just motion, no dwell at interior points)
	for (size_t i = 0; i + 1 < pts.size(); ++i) {
		rasterize_segment_motion(canvas, pts[i], pts[i + 1], pen_radius, ink_color);
	}

	// Dwell at end point (pen lifts)
	rasterize_dot(canvas, pts.back(), pen_radius, endpoint_ink);
}

// Convenience helper. Is this useful enough? Remove?
template <typename InkType>
void rasterize_segment(
	Canvas<InkType>& canvas, gl::vec2 A, gl::vec2 B,
	float pen_radius, InkType ink_color, float dwell_time = 1.0f)
{
	std::array pts = {A, B};
	rasterize_polyline(canvas, pts, pen_radius, ink_color, dwell_time);
}

////

// InkType: 'gl::vec3' for RGB, 'float' for grayscale
// Deduce pixel type from ink type
template<typename InkType> struct PixelTypeFor;
template<> struct PixelTypeFor<gl::vec3> { using type = RgbPixel; };
template<> struct PixelTypeFor<float> { using type = GrayPixel; };
template<typename InkType> using PixelTypeFor_t = typename PixelTypeFor<InkType>::type;

template<typename Sampler> using InkTypeForSampler = decltype(std::declval<Sampler>()(0, 0));

template<typename Sampler>
Canvas<PixelTypeFor_t<InkTypeForSampler<Sampler>>> ink_to_pixels(gl::ivec2 size, Sampler sampler)
{
	using InkType = InkTypeForSampler<Sampler>;
	using PixelType = PixelTypeFor_t<InkType>;

	// Saturation strength - models paper's limited ink absorption
	// Rational function: ink*(1+k) / (1+k*ink), scaled to 0-255 range
	// - Passes through (1,1): single stroke looks correct
	// - For overlaps (ink>1): output < ink (overlaps saturate smoothly)
	constexpr float k = 4.0f;
	constexpr float numerator = (1.0f + k) * 255.0f;

	Canvas<PixelType> output(size, PixelType(255));
	for (int y = 0; y < size.y; ++y) {
		auto out_line = output.getLine(y);
		for (int x = 0; x < size.x; ++x) {
			InkType accumulated_ink = sampler(x, y);
			if (accumulated_ink == InkType(0.0f)) {
				continue;  // Already initialized to white (255)
			}

			// Apply saturation function in 0-255 range (formula works for both float and gl::vec3)
			InkType saturated_ink_255 = accumulated_ink * numerator / (InkType(1.0f) + k * accumulated_ink);

			// Apply subtractive ink model: white paper (255) minus saturated ink
			InkType pixel_value = InkType(255.0f) - saturated_ink_255;
			using gl::clamp;  // Bring both clamp functions into scope ..
			using std::clamp; // .. overload resolution picks the right one
			out_line[x] = PixelType(clamp(pixel_value, InkType(0.0f), InkType(255.0f)));
		}
	}
	return output;
}

////

PlotterPaper::PlotterPaper(gl::ivec2 size)
	: paper(size)
{
}

void PlotterPaper::draw_dot(gl::vec2 pos, float radius, gl::vec3 color)
{
	anythingPlotted = true;
	damage = true;
	rasterize_dot(paper, pos, radius, color);
}

void PlotterPaper::draw_motion(gl::vec2 from, gl::vec2 to, float radius, gl::vec3 color)
{
	anythingPlotted = true;
	damage = true;
	rasterize_segment_motion(paper, from, to, radius, color);
}

Canvas<RgbPixel> PlotterPaper::getRGB() const
{
	auto out_size = size();
	auto sampler = [&](int x, int y) { return paper.getLine(y)[x]; };
	return ink_to_pixels(out_size, sampler);
}

gl::Texture& PlotterPaper::updateTexture(gl::ivec2 targetSize)
{
	auto fullSize = paper.size();
	auto clampedTargetSize = min(targetSize, gl::ivec2(1024)); // ensure below 2048 x 2028
	auto factor = max_component(trunc(fullSize / clampedTargetSize));
	auto newTexSize = max(fullSize / factor, gl::ivec2(1));

	if (texture.get() && texSize == newTexSize && !damage) {
		return texture; // already up to date
	}

	damage = false;
	texSize = newTexSize;

	auto sample_direct = [&](int x, int y) { return paper.getLine(y)[x]; };
	auto down_sample_generic = [&](int x, int y) {
		auto sum = gl::vec3(0.0f);
		for (int sy = 0; sy < factor; ++sy) {
			auto line = subspan(paper.getLine(y * factor + sy), x * factor, factor);
			for (int sx = 0; sx < factor; ++sx) {
				sum += line[sx];
			}
		}
		return sum * (1.0f / float(factor * factor));
	};
	auto down_sample_N = [&]<int FACTOR>(int x, int y) {
		auto sum = gl::vec3(0.0f);
		for (int sy = 0; sy < FACTOR; ++sy) {
			auto line = subspan<FACTOR>(paper.getLine(y * FACTOR + sy), x * FACTOR);
			for (int sx = 0; sx < FACTOR; ++sx) {
				sum += line[sx];
			}
		}
		return sum * (1.0f / float(FACTOR * FACTOR));
	};
	auto rgb = [&]{
		// specializations for small common factors,
		// this gives a reasonably large speedup
		switch (factor) {
		case 1:  return ink_to_pixels(texSize, sample_direct);
		case 2:  return ink_to_pixels(texSize, [&](int x, int y) { return down_sample_N.operator()<2>(x, y); });
		case 3:  return ink_to_pixels(texSize, [&](int x, int y) { return down_sample_N.operator()<3>(x, y); });
		case 4:  return ink_to_pixels(texSize, [&](int x, int y) { return down_sample_N.operator()<4>(x, y); });
		case 5:  return ink_to_pixels(texSize, [&](int x, int y) { return down_sample_N.operator()<5>(x, y); });
		case 6:  return ink_to_pixels(texSize, [&](int x, int y) { return down_sample_N.operator()<6>(x, y); });
		case 7:  return ink_to_pixels(texSize, [&](int x, int y) { return down_sample_N.operator()<7>(x, y); });
		case 8:  return ink_to_pixels(texSize, [&](int x, int y) { return down_sample_N.operator()<8>(x, y); });
		default: return ink_to_pixels(texSize, down_sample_generic);
		}
	}();

	if (!texture.get()) {
		texture.allocate();
		texture.setInterpolation(true);
	} else {
		texture.bind();
	}
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, texSize.x, texSize.y, 0, GL_RGB,
	             GL_UNSIGNED_BYTE, &rgb.getLine(0)[0].x);
	// TODO
	//	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texSize.x, texSize.y, GL_RGB,
	//			GL_UNSIGNED_BYTE, dataToUpload);
	return texture;
}

} // namespace openmsx
