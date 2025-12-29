#include "catch.hpp"
#include "timeline.hh"

#include <vector>

struct Event {
	double time;
	int value;  // for testing/display purposes
};

struct SimpleMapper {
	double scale; // zoom factor: pixels per time unit

	explicit SimpleMapper(double s = 1.0) : scale(s) {}

	[[nodiscard]] float timeToX(double time) const {
		return static_cast<float>(time * scale);
	}

	[[nodiscard]] double xToTime(float x) const {
		return x / scale;
	}
};

// helper to capture draw calls
struct DrawCall {
	enum Type { DETAILED, COARSE } type;
	float fromX;
	float toX;
	int eventValue; // only for DETAILED

	bool operator==(const DrawCall& other) const {
		if (type != other.type) return false;
		// use epsilon comparison for floating point
		constexpr float eps = 0.01f;
		if (std::abs(fromX - other.fromX) > eps) return false;
		if (std::abs(toX - other.toX) > eps) return false;
		if (type == DETAILED && eventValue != other.eventValue) return false;
		return true;
	}
};

// helper to run algorithm and capture calls
static std::vector<DrawCall> runAlgorithm(
	const std::vector<Event>& events,
	double endTime,
	const SimpleMapper& mapper = SimpleMapper(1.0),
	float threshold = 1.0f)
{
	std::vector<DrawCall> calls;

	auto onDetailed = [&](float fromX, float toX, const Event& event) {
		calls.push_back({DrawCall::DETAILED, fromX, toX, event.value});
	};
	auto onCoarse = [&](float fromX, float toX) {
		calls.push_back({DrawCall::COARSE, fromX, toX, -1});
	};

	openmsx::processTimeline(std::span<const Event>(events), endTime, mapper, onDetailed, onCoarse, threshold);
	return calls;
}

TEST_CASE("Timeline processing algorithm - basic scenarios", "[timeline]") {

	SECTION("empty events list") {
		auto calls = runAlgorithm({}, 20.0);
		REQUIRE(calls.empty());
	}

	SECTION("single event") {
		auto calls = runAlgorithm({{10.0, 42}}, 20.0);
		REQUIRE(calls.size() == 1);
		CHECK(calls[0] == DrawCall{DrawCall::DETAILED, 10.0f, 20.0f, 42});
	}

	SECTION("all detailed - well-spaced events") {
		auto calls = runAlgorithm({{10.0, 1}, {15.0, 2}, {22.0, 3}, {30.0, 4}}, 40.0);
		REQUIRE(calls.size() == 4);
		CHECK(calls[0] == DrawCall{DrawCall::DETAILED, 10.0f, 15.0f, 1});
		CHECK(calls[1] == DrawCall{DrawCall::DETAILED, 15.0f, 22.0f, 2});
		CHECK(calls[2] == DrawCall{DrawCall::DETAILED, 22.0f, 30.0f, 3});
		CHECK(calls[3] == DrawCall{DrawCall::DETAILED, 30.0f, 40.0f, 4});
	}

	SECTION("all coarse - dense cluster") {
		auto calls = runAlgorithm({{10.1, 1}, {10.3, 2}, {10.5, 3}, {10.7, 4}, {10.9, 5}, {20.0, 6}}, 25.0);
		REQUIRE(calls.size() == 3);
		CHECK(calls[0] == DrawCall{DrawCall::COARSE, 10.1f, 10.9f, -1});
		CHECK(calls[1] == DrawCall{DrawCall::DETAILED, 10.9f, 20.0f, 5});
		CHECK(calls[2] == DrawCall{DrawCall::DETAILED, 20.0f, 25.0f, 6});
	}

	SECTION("mixed pattern") {
		auto calls = runAlgorithm({{5.1, 1}, {5.3, 2}, {5.5, 3}, {10.0, 4}, {20.1, 5}, {20.3, 6}, {20.6, 7}, {30.0, 8}}, 35.0);
		REQUIRE(calls.size() == 6);
		CHECK(calls[0] == DrawCall{DrawCall::COARSE, 5.1f, 5.5f, -1});
		CHECK(calls[1] == DrawCall{DrawCall::DETAILED, 5.5f, 10.0f, 3});
		CHECK(calls[2] == DrawCall{DrawCall::DETAILED, 10.0f, 20.1f, 4});
		CHECK(calls[3] == DrawCall{DrawCall::COARSE, 20.1f, 20.6f, -1});
		CHECK(calls[4] == DrawCall{DrawCall::DETAILED, 20.6f, 30.0f, 7});
		CHECK(calls[5] == DrawCall{DrawCall::DETAILED, 30.0f, 35.0f, 8});
	}
}

TEST_CASE("Timeline - threshold boundary behavior", "[timeline][threshold]") {

	SECTION("spacing exactly = 1.0 (aligned) -> coarse") {
		auto calls = runAlgorithm({{10.0, 1}, {11.0, 2}, {12.0, 3}, {13.0, 4}}, 20.0);
		REQUIRE(calls.size() == 2);
		CHECK(calls[0] == DrawCall{DrawCall::COARSE, 10.0f, 13.0f, -1});
		CHECK(calls[1] == DrawCall{DrawCall::DETAILED, 13.0f, 20.0f, 4});
	}

	SECTION("spacing exactly = 1.0 (non-aligned) -> coarse") {
		auto calls = runAlgorithm({{10.3, 1}, {11.3, 2}, {12.3, 3}, {13.3, 4}}, 20.0);
		REQUIRE(calls.size() == 2);
		CHECK(calls[0] == DrawCall{DrawCall::COARSE, 10.3f, 13.3f, -1});
		CHECK(calls[1] == DrawCall{DrawCall::DETAILED, 13.3f, 20.0f, 4});
	}

	SECTION("spacing = 1.1 (just above threshold) -> detailed") {
		auto calls = runAlgorithm({{10.0, 1}, {11.1, 2}, {12.2, 3}, {13.3, 4}}, 20.0);
		REQUIRE(calls.size() == 4);
		CHECK(calls[0] == DrawCall{DrawCall::DETAILED, 10.0f, 11.1f, 1});
		CHECK(calls[1] == DrawCall{DrawCall::DETAILED, 11.1f, 12.2f, 2});
		CHECK(calls[2] == DrawCall{DrawCall::DETAILED, 12.2f, 13.3f, 3});
		CHECK(calls[3] == DrawCall{DrawCall::DETAILED, 13.3f, 20.0f, 4});
	}

	SECTION("custom threshold = 2.0") {
		auto calls = runAlgorithm({{10.0, 1}, {11.5, 2}, {13.0, 3}, {16.0, 4}}, 20.0, SimpleMapper(1.0), 2.0f);
		REQUIRE(calls.size() == 3);
		CHECK(calls[0] == DrawCall{DrawCall::COARSE, 10.0f, 13.0f, -1});
		CHECK(calls[1] == DrawCall{DrawCall::DETAILED, 13.0f, 16.0f, 3});
		CHECK(calls[2] == DrawCall{DrawCall::DETAILED, 16.0f, 20.0f, 4});
	}

	SECTION("two very close events (spacing = 0.01)") {
		auto calls = runAlgorithm({{7.51, 1}, {7.52, 2}, {15.0, 3}}, 20.0);
		REQUIRE(calls.size() == 3);
		CHECK(calls[0] == DrawCall{DrawCall::COARSE, 7.51f, 7.52f, -1});
		CHECK(calls[1] == DrawCall{DrawCall::DETAILED, 7.52f, 15.0f, 2});
		CHECK(calls[2] == DrawCall{DrawCall::DETAILED, 15.0f, 20.0f, 3});
	}

	SECTION("multiple events at exact same time (spacing = 0)") {
		auto calls = runAlgorithm({{10.0, 1}, {10.0, 2}, {10.0, 3}, {15.0, 4}}, 20.0);
		REQUIRE(calls.size() == 3);
		CHECK(calls[0] == DrawCall{DrawCall::COARSE, 10.0f, 10.0f, -1});  // zero-width
		CHECK(calls[1] == DrawCall{DrawCall::DETAILED, 10.0f, 15.0f, 3});
		CHECK(calls[2] == DrawCall{DrawCall::DETAILED, 15.0f, 20.0f, 4});
	}
}

TEST_CASE("Timeline - dense clusters (binary search)", "[timeline][performance]") {

	SECTION("many events tightly clustered (15 events in 0.15 units)") {
		std::vector<Event> events;
		for (int i = 0; i < 15; ++i) {
			events.push_back({5.01 + i * 0.01, i + 1});
		}
		events.push_back({10.0, 16});

		auto calls = runAlgorithm(events, 15.0);
		REQUIRE(calls.size() == 3);
		CHECK(calls[0] == DrawCall{DrawCall::COARSE, 5.01f, 5.15f, -1});
		CHECK(calls[1] == DrawCall{DrawCall::DETAILED, 5.15f, 10.0f, 15});
		CHECK(calls[2] == DrawCall{DrawCall::DETAILED, 10.0f, 15.0f, 16});
	}

	SECTION("dense events spanning multiple pixels (100 events in 2 units)") {
		std::vector<Event> events;
		for (int i = 0; i < 100; ++i) {
			events.push_back({10.0 + i * 0.02, 100 + i});
		}
		events.push_back({20.0, 200});

		auto calls = runAlgorithm(events, 25.0);
		REQUIRE(calls.size() == 3);
		CHECK(calls[0] == DrawCall{DrawCall::COARSE, 10.0f, 11.98f, -1});
		CHECK(calls[1] == DrawCall{DrawCall::DETAILED, 11.98f, 20.0f, 199});
		CHECK(calls[2] == DrawCall{DrawCall::DETAILED, 20.0f, 25.0f, 200});
	}

	SECTION("cluster right before pixel boundary") {
		auto calls = runAlgorithm(
			{{9.91, 1}, {9.92, 2}, {9.93, 3}, {9.94, 4}, {9.95, 5},
			 {9.96, 6}, {9.97, 7}, {9.98, 8}, {9.99, 9}, {15.0, 10}},
			20.0);
		REQUIRE(calls.size() == 3);
		CHECK(calls[0] == DrawCall{DrawCall::COARSE, 9.91f, 9.99f, -1});
		CHECK(calls[1] == DrawCall{DrawCall::DETAILED, 9.99f, 15.0f, 9});
		CHECK(calls[2] == DrawCall{DrawCall::DETAILED, 15.0f, 20.0f, 10});
	}
}

TEST_CASE("Timeline - edge cases", "[timeline][edge]") {

	SECTION("last events are coarse") {
		auto calls = runAlgorithm({{5.0, 1}, {10.1, 2}, {10.2, 3}, {10.3, 4}, {10.4, 5}}, 10.5);
		REQUIRE(calls.size() == 2);
		CHECK(calls[0] == DrawCall{DrawCall::DETAILED, 5.0f, 10.1f, 1});
		CHECK(calls[1] == DrawCall{DrawCall::COARSE, 10.1f, 10.5f, -1});
	}

	SECTION("dense cluster at very end of timeline") {
		auto calls = runAlgorithm({{5.0, 1}, {99.1, 2}, {99.2, 3}, {99.3, 4}, {99.4, 5}, {99.5, 6}}, 99.6);
		REQUIRE(calls.size() == 2);
		CHECK(calls[0] == DrawCall{DrawCall::DETAILED, 5.0f, 99.1f, 1});
		CHECK(calls[1] == DrawCall{DrawCall::COARSE, 99.1f, 99.6f, -1});
	}

	SECTION("endTimeequals last event time") {
		auto calls = runAlgorithm({{10.0, 1}, {15.5, 2}, {23.7, 3}}, 23.7);
		REQUIRE(calls.size() == 3);
		CHECK(calls[0] == DrawCall{DrawCall::DETAILED, 10.0f, 15.5f, 1});
		CHECK(calls[1] == DrawCall{DrawCall::DETAILED, 15.5f, 23.7f, 2});
		CHECK(calls[2] == DrawCall{DrawCall::COARSE, 23.7f, 23.7f, -1});  // zero-width
	}

	SECTION("fractional endTime") {
		auto calls = runAlgorithm({{14.1, 1}, {14.9, 2}, {23.6, 3}}, 30.27);
		REQUIRE(calls.size() == 3);
		CHECK(calls[0] == DrawCall{DrawCall::COARSE, 14.1f, 14.9f, -1});
		CHECK(calls[1] == DrawCall{DrawCall::DETAILED, 14.9f, 23.6f, 2});
		CHECK(calls[2] == DrawCall{DrawCall::DETAILED, 23.6f, 30.27f, 3});
	}
}

TEST_CASE("Timeline - zoom behavior", "[timeline][zoom]") {
	// same events at different zoom levels
	std::vector<Event> events = {{10.0, 1}, {10.1, 2}, {10.2, 3}, {10.3, 4}, {10.4, 5}};
	double endTime = 15.0;

	SECTION("zoom 1x (identity) - events 0.1 apart -> coarse") {
		auto calls = runAlgorithm(events, endTime, SimpleMapper(1.0));
		REQUIRE(calls.size() == 2);
		CHECK(calls[0] == DrawCall{DrawCall::COARSE, 10.0f, 10.4f, -1});
		CHECK(calls[1] == DrawCall{DrawCall::DETAILED, 10.4f, 15.0f, 5});
	}

	SECTION("zoom 2x - spacing becomes 0.2 pixels -> still coarse") {
		auto calls = runAlgorithm(events, endTime, SimpleMapper(2.0));
		REQUIRE(calls.size() == 2);
		CHECK(calls[0] == DrawCall{DrawCall::COARSE, 20.0f, 20.8f, -1});
		CHECK(calls[1] == DrawCall{DrawCall::DETAILED, 20.8f, 30.0f, 5});
	}

	SECTION("zoom 15x - spacing becomes 1.5 pixels -> all detailed") {
		auto calls = runAlgorithm(events, endTime, SimpleMapper(15.0));
		REQUIRE(calls.size() == 5);
		CHECK(calls[0] == DrawCall{DrawCall::DETAILED, 150.0f, 151.5f, 1});
		CHECK(calls[1] == DrawCall{DrawCall::DETAILED, 151.5f, 153.0f, 2});
		CHECK(calls[2] == DrawCall{DrawCall::DETAILED, 153.0f, 154.5f, 3});
		CHECK(calls[3] == DrawCall{DrawCall::DETAILED, 154.5f, 156.0f, 4});
		CHECK(calls[4] == DrawCall{DrawCall::DETAILED, 156.0f, 225.0f, 5});
	}

	SECTION("zoom 0.01x (zoom out) - everything in same pixel -> single coarse") {
		auto calls = runAlgorithm(events, endTime, SimpleMapper(0.01));
		REQUIRE(calls.size() == 1);
		CHECK(calls[0] == DrawCall{DrawCall::COARSE, 0.1f, 0.15f, -1});
	}
}

TEST_CASE("Timeline - complex realistic scenarios", "[timeline][integration]") {

	SECTION("alternating dense and sparse regions") {
		auto calls = runAlgorithm(
			{{10.1, 1}, {10.2, 2}, {10.3, 3}, {20.0, 4},
			 {30.1, 5}, {30.2, 6}, {30.3, 7}, {40.0, 8},
			 {50.1, 9}, {50.2, 10}, {60.0, 11}},
			70.0);
		REQUIRE(calls.size() == 9);
		CHECK(calls[0] == DrawCall{DrawCall::COARSE, 10.1f, 10.3f, -1});
		CHECK(calls[1] == DrawCall{DrawCall::DETAILED, 10.3f, 20.0f, 3});
		CHECK(calls[2] == DrawCall{DrawCall::DETAILED, 20.0f, 30.1f, 4});
		CHECK(calls[3] == DrawCall{DrawCall::COARSE, 30.1f, 30.3f, -1});
		CHECK(calls[4] == DrawCall{DrawCall::DETAILED, 30.3f, 40.0f, 7});
		CHECK(calls[5] == DrawCall{DrawCall::DETAILED, 40.0f, 50.1f, 8});
		CHECK(calls[6] == DrawCall{DrawCall::COARSE, 50.1f, 50.2f, -1});
		CHECK(calls[7] == DrawCall{DrawCall::DETAILED, 50.2f, 60.0f, 10});
		CHECK(calls[8] == DrawCall{DrawCall::DETAILED, 60.0f, 70.0f, 11});
	}

	SECTION("large dataset with mixed regions") {
		std::vector<Event> events;
		// dense cluster at start (50 events, 0.1 apart)
		for (int i = 0; i < 50; ++i) {
			events.push_back({0.0 + i * 0.1, 1000 + i});
		}
		// well-spaced events
		events.push_back({10.0, 2000});
		events.push_back({15.5, 2001});
		events.push_back({23.7, 2002});
		events.push_back({35.2, 2003});
		events.push_back({48.9, 2004});
		// another dense cluster (30 events, 0.066 apart)
		for (int i = 0; i < 30; ++i) {
			events.push_back({50.0 + i * 0.066, 3000 + i});
		}
		// final well-spaced events
		events.push_back({60.0, 4000});
		events.push_back({75.5, 4001});
		events.push_back({90.0, 4002});

		auto calls = runAlgorithm(events, 100.0);
		REQUIRE(calls.size() == 12);
		CHECK(calls[0].type == DrawCall::COARSE);  // first dense cluster
		CHECK(calls[1].type == DrawCall::DETAILED);
		// well-spaced section
		CHECK(calls[2].type == DrawCall::DETAILED);
		CHECK(calls[3].type == DrawCall::DETAILED);
		CHECK(calls[4].type == DrawCall::DETAILED);
		CHECK(calls[5].type == DrawCall::DETAILED);
		CHECK(calls[6].type == DrawCall::DETAILED);
		// second dense cluster
		CHECK(calls[7].type == DrawCall::COARSE);
		// final well-spaced section
		CHECK(calls[8].type == DrawCall::DETAILED);
		CHECK(calls[9].type == DrawCall::DETAILED);
		CHECK(calls[10].type == DrawCall::DETAILED);
		CHECK(calls[11].type == DrawCall::DETAILED);
	}
}
