#include "catch.hpp"
#include "GunstickSensor.hh"

#include <cstdint>
#include <optional>

using namespace openmsx;
using namespace openmsx::gunstick;
using gl::ivec2;
using gl::vec2;
using gl::vec4;

static constexpr uint32_t BLACK = 0xFF000000;
static constexpr uint32_t WHITE = 0xFFFFFFFF;
static constexpr uint32_t CYAN  = 0xFFE6F634; // the sky in Duck Hunt

// A black frame with a white square target.
struct Frame {
	int x0 = 0, y0 = 0, size = 0;
	uint32_t operator()(int x, int y) const {
		bool inside = (x0 <= x) && (x < x0 + size) && (y0 <= y) && (y < y0 + size);
		return inside ? WHITE : BLACK;
	}
};

static int pos(int x, int y) { return y * PIXELS_PER_LINE + x; }

// VDP ticks since the start of the frame -> ticks since RawFrame line 0. The
// exact number of lines above RawFrame line 0 doesn't matter for these tests.
static int fromFrameStart(int ticks) { return ticks - 20 * TICKS_PER_LINE; }

TEST_CASE("Gunstick: isBright")
{
	CHECK( isBright(WHITE));
	CHECK(!isBright(BLACK));
	CHECK(!isBright(0xFF800000)); // dark blue
	CHECK( isBright(0xFF40E0E0)); // light yellow (r=0xE0, g=0xE0, b=0x40)
}

TEST_CASE("Gunstick: beamPosition")
{
	// RawFrame column 160 is drawn at tick 770 of a line.
	CHECK(beamPosition(770) == pos(160, 0));
	CHECK(beamPosition(100 * TICKS_PER_LINE + 770 + 4 * 10) == pos(170, 100));
	// lines above RawFrame line 0
	CHECK(beamPosition(-TICKS_PER_LINE + 770) == pos(160, -1));
	CHECK(beamPosition(-1) == pos(309, -1)); // last tick of line -1
	// later in time -> larger position
	CHECK(beamPosition(50000) < beamPosition(50004));
	CHECK(beamPosition(-50004) < beamPosition(-50000));
}

TEST_CASE("Gunstick: seesLight")
{
	Frame frame{.x0 = 100, .y0 = 100, .size = 16}; // white 16x16 square
	ivec2 aim(108, 108);

	SECTION("the sensor reacts one line after the beam drew the target") {
		// (103,103) is the first target pixel inside the sensor circle
		CHECK(!seesLight(pos(103, 103), aim, frame));       // just drawn
		CHECK(!seesLight(pos(103, 103) + 300, aim, frame)); // less than a line ago
		CHECK( seesLight(pos(103, 104), aim, frame));       // one line later
	}
	SECTION("beam not yet at the target") {
		CHECK(!seesLight(pos(108, 90), aim, frame));
		CHECK(!seesLight(pos(50, 104), aim, frame)); // first line of the sensor area
	}
	SECTION("the signal is held for about 26 lines") {
		// last bright pixel the sensor sees: (113,113)
		CHECK( seesLight(pos(108, 113 + 20), aim, frame));
		CHECK( seesLight(pos(113, 113 + 1 + 26), aim, frame));
		CHECK(!seesLight(pos(114, 113 + 1 + 26), aim, frame));
	}
	SECTION("aiming at black never sees light") {
		ivec2 off(50, 50);
		for (int y = 0; y < FRAME_HEIGHT; ++y) {
			for (int x = 0; x < FRAME_WIDTH; x += 7) {
				CHECK(!seesLight(pos(x, y), off, frame));
			}
		}
	}
	SECTION("aim near the screen edge doesn't read outside the frame") {
		Frame corner{.x0 = 0, .y0 = 0, .size = 8};
		CHECK(seesLight(pos(2, 2), ivec2(0, 0), corner));
	}
}

// Target Plus (Dinamic 1988), routine at 0x98F2: after a HALT it reads the
// light sensor 1400 times, 145 Z80 cycles apart (= 870 VDP ticks, so about
// 3.4 NTSC frames), and counts a hit when more than 40 reads see light.
// Assumption: a 16x16 white target (size not measured from the game).
static int targetPlusLitSamples(ivec2 aim, int linesPerFrame)
{
	Frame frame{.x0 = 100, .y0 = 100, .size = 16};
	const int ticksPerFrame = linesPerFrame * TICKS_PER_LINE;
	constexpr int TICKS_PER_SAMPLE = 145 * 6;
	int lit = 0;
	for (int i = 0; i < 1400; ++i) {
		int ticks = (i * TICKS_PER_SAMPLE) % ticksPerFrame;
		if (seesLight(beamPosition(fromFrameStart(ticks)), aim, frame)) ++lit;
	}
	return lit;
}

TEST_CASE("Gunstick: Target Plus hit detection")
{
	SECTION("NTSC") {
		CHECK(targetPlusLitSamples(ivec2(108, 108), 262) > 40); // hit
		CHECK(targetPlusLitSamples(ivec2(200, 50), 262) == 0);  // miss
	}
	SECTION("PAL (e.g. Philips VG 8020)") {
		CHECK(targetPlusLitSamples(ivec2(108, 108), 313) > 40);
		CHECK(targetPlusLitSamples(ivec2(200, 50), 313) == 0);
	}
}

// Duck Hunt (Karoshi 2004): it blanks the screen, draws the duck in white and
// then reads the sensor during one frame; one read with light is a hit.
// The renderer hasn't drawn the pixels at and after the beam yet, so those
// still hold an older frame: the cyan sky. They must not be seen as light.
static int duckHuntLitSamples(ivec2 aim)
{
	Frame duck{.x0 = 100, .y0 = 100, .size = 8};
	constexpr int TICKS_PER_FRAME = 313 * TICKS_PER_LINE; // PAL
	int lit = 0;
	for (int ticks = 0; ticks < TICKS_PER_FRAME; ticks += 600) {
		int beam = beamPosition(fromFrameStart(ticks));
		auto getPixel = [&](int x, int y) {
			return (pos(x, y) >= beam) ? CYAN : duck(x, y);
		};
		if (seesLight(beam, aim, getPixel)) ++lit;
	}
	return lit;
}

TEST_CASE("Gunstick: Duck Hunt, pixels not yet drawn are ignored")
{
	CHECK(isBright(CYAN)); // the sky is bright, so a real gun would see it
	CHECK(duckHuntLitSamples(ivec2(200, 50)) == 0); // shooting at the sky: miss
	CHECK(duckHuntLitSamples(ivec2(104, 104)) > 0); // on the duck: hit
}

// Host mouse position -> RawFrame pixel, for a flat picture.
static std::optional<ivec2> mouseToFrame(
	vec2 mouse, vec2 msxPixelSize, vec2 viewOffset, vec2 viewSize,
	float hStretch, bool fullStretch)
{
	return viewToFrame(mouseToView(mouse, msxPixelSize, viewOffset, viewSize,
	                               hStretch, fullStretch),
	                   hStretch);
}

TEST_CASE("Gunstick: mouseToFrame")
{
	SECTION("window, scale factor 2, horizontal_stretch 320") {
		// msx pixel = 2x2 points, no offset
		auto p = mouseToFrame(vec2(100, 50), vec2(2, 2), vec2(0, 0), vec2(640, 480), 320.0f, false);
		REQUIRE(p);
		CHECK(*p == ivec2(50, 25));
		CHECK(!mouseToFrame(vec2(-1, 50), vec2(2, 2), vec2(0, 0), vec2(640, 480), 320.0f, false));
		CHECK(!mouseToFrame(vec2(100, 480), vec2(2, 2), vec2(0, 0), vec2(640, 480), 320.0f, false));
	}
	SECTION("horizontal_stretch 280 skips 20 RawFrame columns on each side") {
		vec2 pixel(640.0f / 280.0f, 2.0f);
		auto p = mouseToFrame(vec2(0, 0), pixel, vec2(0, 0), vec2(640, 480), 280.0f, false);
		REQUIRE(p);
		CHECK(*p == ivec2(20, 0));
		p = mouseToFrame(vec2(639, 0), pixel, vec2(0, 0), vec2(640, 480), 280.0f, false);
		REQUIRE(p);
		CHECK(*p == ivec2(299, 0));
	}
	SECTION("fullscreen 1920x1080 with letterbox") {
		// view port 1440x1080 at offset (240,0), points == pixels
		vec2 pixel(1440.0f / 320.0f, 1080.0f / 240.0f);
		CHECK(!mouseToFrame(vec2(239, 10), pixel, vec2(240, 0), vec2(1440, 1080), 320.0f, false));
		auto p = mouseToFrame(vec2(240, 0), pixel, vec2(240, 0), vec2(1440, 1080), 320.0f, false);
		REQUIRE(p);
		CHECK(*p == ivec2(0, 0));
	}
	SECTION("high-DPI: 2 physical pixels per point") {
		// same fullscreen setup, but the window is 960x540 points
		vec2 pixel(0.5f * 1440.0f / 320.0f, 0.5f * 1080.0f / 240.0f);
		auto p = mouseToFrame(vec2(120 + 10 * pixel.x, 20 * pixel.y), pixel,
		                      vec2(240, 0), vec2(1440, 1080), 320.0f, false);
		REQUIRE(p);
		CHECK(*p == ivec2(10, 20));
	}
	SECTION("full_stretch: the image covers the whole window") {
		vec2 pixel(1920.0f / 320.0f, 1080.0f / 240.0f);
		auto p = mouseToFrame(vec2(0, 0), pixel, vec2(240, 0), vec2(1440, 1080), 320.0f, true);
		REQUIRE(p);
		CHECK(*p == ivec2(0, 0));
	}
}

TEST_CASE("Gunstick: viewToFrame3D")
{
	// Draw the middle of some RawFrame pixels on the curved surface, the way
	// PostProcessor does, and map that position back.
	auto mvp = monitor3d::mvpMatrix();
	auto draw = [&](ivec2 pixel, float hStretch) {
		vec2 tex((float(pixel.x) + 0.5f) / FRAME_WIDTH,
		         1.0f - (float(pixel.y) + 0.5f) / FRAME_HEIGHT);
		// inverse of monitor3d::texCoord()
		float s = hStretch / FRAME_WIDTH;
		float b = (FRAME_WIDTH - hStretch) / (2.0f * FRAME_WIDTH);
		vec2 xy((tex.x - b) / s * 2.0f - 1.0f, tex.y * 2.0f - 1.0f);
		vec4 clip = mvp * vec4(xy, vec2(monitor3d::surfaceZ(xy), 1.0f));
		vec2 ndc = vec2(clip) / clip.w;
		return vec2((ndc.x + 1.0f) * 0.5f * hStretch,
		            (1.0f - ndc.y) * 0.5f * FRAME_HEIGHT);
	};
	for (float hStretch : {320.0f, 280.0f}) {
		for (ivec2 pixel : {ivec2(160, 120), ivec2(25, 0), ivec2(294, 239),
		                    ivec2(30, 200), ivec2(250, 10)}) {
			auto p = viewToFrame3D(draw(pixel, hStretch), hStretch);
			REQUIRE(p);
			CHECK(*p == pixel);
		}
	}
	SECTION("the picture doesn't fill the view port") {
		CHECK(!viewToFrame3D(vec2(0, 0), 320.0f));
		CHECK(!viewToFrame3D(vec2(320, 240), 320.0f));
	}
	SECTION("the middle of the view port isn't the middle of the picture") {
		auto p = viewToFrame3D(vec2(160, 120), 320.0f);
		REQUIRE(p);
		CHECK(*p != ivec2(160, 120));
	}
}
