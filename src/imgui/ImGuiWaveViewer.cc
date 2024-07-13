#include "ImGuiWaveViewer.hh"

#include "MSXMixer.hh"
#include "MSXMotherBoard.hh"
#include "SoundDevice.hh"

#include "FFTReal.hh"
#include "blackmanWindow.hh"
#include "halfband.hh"
#include "narrow.hh"
#include "ranges.hh"
#include "view.hh"
#include "xrange.hh"

#include "imgui.h"
#include "imgui_internal.h" // for ImLerp

#include <algorithm>
#include <cmath>
#include <functional>
#include <numeric>

namespace openmsx {

// Simplified/optimized version of ImGui::PlotLines()
static void plotLines(std::span<const float> values, float scale_min, float scale_max, gl::vec2 outer_size)
{
	const auto& style = ImGui::GetStyle();

	gl::vec2 pos = ImGui::GetCursorScreenPos();
	auto outer_tl = pos;              // top-left
	auto outer_br = pos + outer_size; // bottom-right
	auto inner_tl = outer_tl + gl::vec2(style.FramePadding);
	auto inner_br = outer_br - gl::vec2(style.FramePadding);

	ImGui::RenderFrame(outer_tl, outer_br, ImGui::GetColorU32(ImGuiCol_FrameBg),
	                   true, style.FrameRounding);

	int num_values = narrow<int>(values.size());
	if (num_values < 2) return;
	int num_items = num_values - 1;

	int inner_width = std::max(1, static_cast<int>(inner_br.x - inner_tl.x));
	int res_w = std::min(inner_width, num_items);

	float t_step = 1.0f / (float)res_w;
	float scale_range = scale_max - scale_min;
	float inv_scale = (scale_range != 0.0f) ? (1.0f / scale_range) : 0.0f;

	auto color = ImGui::GetColorU32(ImGuiCol_PlotLines);

	auto valueToY = [&](float v) {
		return 1.0f - std::clamp((v - scale_min) * inv_scale, 0.0f, 1.0f);
	};
	float t = 0.0f;
	auto tp0 = gl::vec2(t, valueToY(values[0]));
	auto pos0 = ImLerp(inner_tl, inner_br, tp0);

	auto* drawList = ImGui::GetWindowDrawList();
	for (int n = 0; n < res_w; n++) {
		int idx = static_cast<int>(t * num_items + 0.5f);
		assert(0 <= idx && idx < num_values);
		float v = values[(idx + 1) % num_values];

		t += t_step;
		auto tp1 = gl::vec2(t, valueToY(v));
		auto pos1 = ImLerp(inner_tl, inner_br, tp1);
		drawList->AddLine(pos0, pos1, color);
		pos0 = pos1;
	}
}

// Simplified/optimized version of ImGui::PlotHistogram()
static void plotHistogram(std::span<const float> values, float scale_min, float scale_max, gl::vec2 outer_size)
{
	const auto& style = ImGui::GetStyle();

	gl::vec2 pos = ImGui::GetCursorScreenPos();
	auto outer_tl = pos;              // top-left
	auto outer_br = pos + outer_size; // bottom-right
	auto inner_tl = outer_tl + gl::vec2(style.FramePadding);
	auto inner_br = outer_br - gl::vec2(style.FramePadding);

	ImGui::RenderFrame(outer_tl, outer_br, ImGui::GetColorU32(ImGuiCol_FrameBg),
	                   true, style.FrameRounding);
	if (values.empty()) return;

	int num_values = narrow<int>(values.size());
	int inner_width = std::max(1, static_cast<int>(inner_br.x - inner_tl.x));
	int res_w = std::min(inner_width, num_values);

	float t_step = 1.0f / static_cast<float>(res_w);
	float scale_range = scale_max - scale_min;
	float inv_scale = (scale_range != 0.0f) ? (1.0f / scale_range) : 0.0f;

	auto valueToY = [&](float v) {
		return 1.0f - std::clamp((v - scale_min) * inv_scale, 0.0f, 1.0f);
	};
	float zero_line = (scale_min * scale_max < 0.0f)
	                ? (1 + scale_min * inv_scale)
	                : (scale_min < 0.0f ? 0.0f : 1.0f);

	auto color = ImGui::GetColorU32(ImGuiCol_PlotHistogram);

	float t0 = 0.0f;
	auto* drawList = ImGui::GetWindowDrawList();
	drawList->PrimReserve(6 * res_w, 4 * res_w);

	for (int n = 0; n < res_w; n++) {
		int idx = static_cast<int>(t0 * num_values + 0.5f);
		assert(0 <= idx && idx < num_values);
		float y0 = valueToY(values[idx]);
		float t1 = t0 + t_step;

		auto pos0 = ImLerp(inner_tl, inner_br, gl::vec2(t0, y0));
		auto pos1 = ImLerp(inner_tl, inner_br, gl::vec2(t1, zero_line));
		drawList->PrimRect(pos0, pos1, color);

		t0 = t1;
	}
}


void ImGuiWaveViewer::save(ImGuiTextBuffer& buf)
{
	savePersistent(buf, *this, persistentElements);
}

void ImGuiWaveViewer::loadLine(std::string_view name, zstring_view value)
{
	loadOnePersistent(name, value, *this, persistentElements);
}

static void paintVUMeter(std::span<const float> buf, float factor, bool muted)
{
	// skip if not visible
	gl::vec2 pos = ImGui::GetCursorScreenPos();
	ImGui::SetNextItemWidth(-FLT_MIN); // full cell-width
	auto width = ImGui::CalcItemWidth();
	auto height = ImGui::GetFrameHeight();
	ImGui::Dummy(gl::vec2{width, height});
	if (!ImGui::IsItemVisible()) return;

	// calculate the average power of the signal
	auto len = float(buf.size());
	auto avg = std::reduce(buf.begin(), buf.end()) / len;
	auto squaredSum = std::transform_reduce(buf.begin(), buf.end(), 0.0f,
		std::plus<>{}, [avg](float x) { auto norm = x - avg; return norm * norm; });
	auto power = std::log10((squaredSum * factor * factor) / len);

	// transform into a value for how to draw this [0, 1]
	static constexpr auto range = 6.0f; // 60dB
	auto clamped = std::clamp(power, -range, 0.0f);
	auto f = (clamped + range) / range;

	// draw gradient
	auto size = gl::vec2{f * width, height};
	auto colorL = muted ? ImVec4{0.2f, 0.2f, 0.2f, 1.0f} : ImVec4{0.0f, 1.0f, 0.0f, 1.0f}; // green
	auto colorR = muted ? ImVec4{0.7f, 0.7f, 0.7f, 1.0f} : ImVec4{1.0f, 0.0f, 0.0f, 1.0f}; // red
	auto color1 = ImGui::ColorConvertFloat4ToU32(colorL);
	auto color2 = ImGui::ColorConvertFloat4ToU32(ImLerp(colorL, colorR, f));
	auto* drawList = ImGui::GetWindowDrawList();
	drawList->AddRectFilledMultiColor(
		pos, pos + size,
		color1, color2, color2, color1);
}

static void paintWave(std::span<const float> buf)
{
	// skip if not visible
	auto pos = ImGui::GetCursorPos();
	ImGui::SetNextItemWidth(-FLT_MIN); // full cell-width
	auto size = gl::vec2{ImGui::CalcItemWidth(), ImGui::GetFrameHeight()};
	ImGui::Dummy(size);
	if (!ImGui::IsItemVisible()) return;
	ImGui::SetCursorPos(pos);

	if (buf.size() < 2) {
		// otherwise PlotLines() doesn't draw anything, we want a line in the middle
		static constexpr std::array<float, 2> silent = {0.0f, 0.0f};
		buf = silent;
	}
	auto peak = std::transform_reduce(buf.begin(), buf.end(), 0.0f,
		[](auto x, auto y) { return std::max(x, y); },
		[](auto x) { return std::abs(x); });
	if (peak == 0.0f) peak = 1.0f; // force line in the middle

	plotLines(buf, -peak, peak, size);
}

static constexpr int FFT_LEN_L2 = 11; // 2048-point FFT -> 1024 bins in the spectrum
static constexpr int FFT_LEN = 1 << FFT_LEN_L2;
struct ReduceResult {
	std::span<float> result; // down-sampled input
	std::span<float, FFT_LEN> extendedResult; // zero-padded
	float normalize = 1.0f; // down-sampling changes amplitude
};
static ReduceResult reduce(std::span<const float> buf, std::span<float> work)
{
	// allocate part of the workBuffer (like a monotonic memory allocator)
	auto allocate = [&](size_t size) {
		assert(size <= work.size());
		auto result = work.first(size);
		work = work.subspan(size);
		return result;
	};

	// This function reduces (or rarely extends) the given buffer to exactly
	// 2048 samples, while retaining the lower part of the frequency
	// spectrum.
	//
	// Typically 'buf' contains too many samples. E.g. the PSG produces
	// sound at ~224kHz, so 1/7 second takes ~31.9k samples. We're not
	// interested in those very high frequencies. And we don't want to run a
	// needlessly expensive FFT operation. Therefor this routine:
	// * Filters away the upper-half of the frequency spectrum (via a
	//   half-band FIR filter).
	// * Then drops every other sample (this doesn't change the spectrum if
	//   the filter has high enough quality).
	// * Repeat until the buffer has 2048 samples or less.
	// * Finally zero-pad the result to exactly 2048 samples (this does not
	//   change the spectrum).
	float normalize = 1.0f;
	if (buf.size() <= FFT_LEN) {
		auto extended = std::span<float, FFT_LEN>(allocate(FFT_LEN));
		auto result = extended.subspan(0, buf.size());
		ranges::copy(buf, result);
		ranges::fill(extended.subspan(buf.size()), 0.0f);
		return {result, extended, normalize};
	}
	while (true) {
		static_assert(HALF_BAND_EXTRA & 1);
		if ((buf.size() & 1) == 0) {
			buf = buf.subspan(1); // drop oldest sample (need an odd number)
		}
		assert(buf.size() >= HALF_BAND_EXTRA);
		size_t m = (buf.size() - HALF_BAND_EXTRA) / 2;
		auto extended = allocate(std::max(m, size_t(FFT_LEN)));
		auto result = extended.subspan(0, m);
		halfBand(buf, result);
		normalize *= 0.5f;

		if (m <= FFT_LEN) {
			assert(extended.size() == FFT_LEN);
			ranges::fill(extended.subspan(m), 0.0f);
			return {result, std::span<float, FFT_LEN>(extended), normalize};
		}
		buf = result;
	}
}

static void paintSpectrum(std::span<const float> buf, float factor)
{
	// skip if not visible
	auto pos = ImGui::GetCursorPos();
	ImGui::SetNextItemWidth(-FLT_MIN); // full cell-width
	auto size = gl::vec2{ImGui::CalcItemWidth(), ImGui::GetFrameHeight()};
	ImGui::Dummy(size);
	if (!ImGui::IsItemVisible()) return;
	ImGui::SetCursorPos(pos);

	// We want to take an FFT of fixed length (2048 points), reduce the
	// input (decimate + zero-pad) to this exact length.
	std::array<float, 32768> workBuf; // ok, uninitialized
	auto [signal, zeroPadded, normalize] = reduce(buf, workBuf);

	// remove DC and apply window-function
	auto window = blackmanWindow(signal.size());
	auto avg = std::reduce(signal.begin(), signal.end()) / float(signal.size());
	for (auto [s, w] : view::zip_equal(signal, window)) {
		s = (s - avg) * w;
	}

	// perform FFT
	std::array<float, FFT_LEN> f;   // ok, uninitialized
	std::array<float, FFT_LEN> tmp; // ok, uninitialized
	FFTReal<FFT_LEN_L2>::execute(zeroPadded, f, tmp);

	// combine real and imaginary components into magnitude (we ignore phase)
	normalize *= factor;
	auto offset = std::log10(normalize * normalize * (1.0f / FFT_LEN));
	static constexpr int HALF = FFT_LEN / 2;
	std::array<float, HALF - 1> magnitude; // ok, uninitialized
	for (int i = 0; i < 1023; ++i) {
		float real = f[i + 1];
		float imag = f[i + 1 + HALF];
		float mag = real * real + imag * imag;
		magnitude[i] = std::log10(mag) + offset + 6.0f;
	}

	// actually plot the result
	plotHistogram(magnitude, 0.0f, 6.0f, size);
}

static void stereoToMono(std::span<const float> stereo, float factorL, float factorR,
                         std::vector<float>& mono)
{
	assert((stereo.size() & 1) == 0);
	auto size = stereo.size() / 2;
	mono.resize(size);
	for (auto i : xrange(size)) {
		mono[i] = factorL * stereo[2 * i + 0]
		        + factorR * stereo[2 * i + 1];
	}
}

static void paintDevice(SoundDevice& device, std::span<const MSXMixer::SoundDeviceInfo::ChannelSettings> settings)
{
	std::vector<float> tmpBuf; // recycle buffer for all channels

	bool stereo = device.hasStereoChannels();
	auto [factorL, factorR] = device.getAmplificationFactor();
	auto factor = stereo ? 1.0f : factorL;

	im::ID_for_range(device.getNumChannels(), [&](int channel) {
		auto monoBuf = [&]{
			auto buf = device.getLastBuffer(channel);
			if (!stereo) return buf;
			stereoToMono(buf, factorL, factorR, tmpBuf);
			return std::span<const float>{tmpBuf};
		}();

		if (ImGui::TableNextColumn()) { // name
			ImGui::StrCat(channel + 1);
		}
		auto& muteSetting = *settings[channel].mute;
		bool muted = muteSetting.getBoolean();
		if (ImGui::TableNextColumn()) { // mute
			if (ImGui::Checkbox("##mute", &muted)) {
				muteSetting.setBoolean(muted);
			}
		}
		if (ImGui::TableNextColumn()) { // vu-meter
			paintVUMeter(monoBuf, factor, muted);
		}
		if (ImGui::TableNextColumn()) { // waveform
			paintWave(monoBuf);
		}
		if (ImGui::TableNextColumn()) { // spectrum
			paintSpectrum(monoBuf, factor);
		}
	});
}

void ImGuiWaveViewer::paint(MSXMotherBoard* motherBoard)
{
	if (!show || !motherBoard) return;

	ImGui::SetNextWindowSize(gl::vec2{38, 15} * ImGui::GetFontSize(), ImGuiCond_FirstUseEver);
	im::Window("Audio channel viewer", &show, [&]{
		for (const auto& info: motherBoard->getMSXMixer().getDeviceInfos()) {
			auto& device = *info.device;
			const auto& name = device.getName();
			if (!ImGui::CollapsingHeader(name.c_str())) continue;
			HelpMarker("Right-click column header to (un)hide columns.\n"
			           "Drag to reorder or resize columns.");

			int flags = ImGuiTableFlags_RowBg |
			            ImGuiTableFlags_BordersV |
			            ImGuiTableFlags_BordersOuter |
			            ImGuiTableFlags_Resizable |
			            ImGuiTableFlags_Reorderable |
			            ImGuiTableFlags_Hideable |
			            ImGuiTableFlags_SizingStretchProp;
			im::Table("##table", 5, flags, [&]{ // note: use the same id for all tables
				ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
				ImGui::TableSetupColumn("channel", ImGuiTableColumnFlags_NoReorder | ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed);
				ImGui::TableSetupColumn("mute", ImGuiTableColumnFlags_DefaultHide | ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed);
				ImGui::TableSetupColumn("VU-meter", 0, 1.0f);
				ImGui::TableSetupColumn("Waveform", 0, 2.0f);
				ImGui::TableSetupColumn("Spectrum", 0, 3.0f);
				ImGui::TableHeadersRow();
				im::ID(name, [&]{
					paintDevice(device, info.channelSettings);
				});
			});
		}
	});
}

} // namespace openmsx
