#ifndef TIMELINE_HH
#define TIMELINE_HH

#include <algorithm>
#include <cmath>
#include <concepts>
#include <optional>
#include <span>

namespace openmsx {

// concept for types that have a "time" member of type Time
template<typename T, typename Time>
concept HasTime = requires(T t) {
	{ t.time } -> std::convertible_to<Time>;
};

// concept for timeline coordinate mapper (time <-> screen space)
template<typename T, typename Time>
concept TimelineMapper = requires(T c, Time time, float x) {
	{ c.timeToX(time) } -> std::convertible_to<float>;
	{ c.xToTime(x) } -> std::convertible_to<Time>;
};

/**
 * Classifies and processes timeline events as either DETAILED or COARSE regions.
 *
 * Motivation: When drawing millions of events on a limited screen (e.g., 1000 pixels wide),
 * many events will be smaller than a single pixel. This algorithm efficiently handles this by:
 * - Drawing individual events where there's space (DETAILED regions)
 * - Grouping dense clusters into coarse representations (COARSE regions)
 * This allows rendering large datasets (e.g., 10M events) efficiently on any zoom level.
 *
 * This function uses a streaming algorithm to efficiently process events on a zoomable timeline,
 * deciding for each event whether it should be processed individually (DETAILED) or grouped into
 * a coarse region (COARSE) based on available screen space.
 *
 * Algorithm:
 * - Iterates through events in a single pass (streaming)
 * - For each event, calculates its screen-space width (spacing to next event)
 * - If spacing > threshold: process event as DETAILED
 * - If spacing <= threshold: accumulates into a COARSE region and uses binary search
 *   to jump to the next pixel boundary, skipping dense clusters efficiently
 * - Merges adjacent coarse regions into single callback invocations
 *
 * Time Complexity: O(R log E)
 * - R = number of output regions (detailed + coarse)
 * - E = number of events
 * - R is bounded by: min(E, X) where X = number of screen pixels covered
 * - Best case: R = E (all events well-spaced, all detailed)
 * - Worst case: R = X (dense events, one coarse region per pixel)
 * - The binary search allows efficient skipping of dense event clusters
 *
 * @tparam Time       Time type (e.g., int, std::chrono::duration, EmuTime)
 * @tparam Event      Event type (must satisfy HasTime<Event, Time> concept)
 * @tparam Mapper     Mapper type (must satisfy TimelineMapper<Mapper, Time> concept)
 * @param events      Sorted list of events (must be sorted by time, ascending)
 * @param endTime     End time of the visible timeline (must be >= last event time)
 * @param mapper      Coordinate mapper (time <-> screen space)
 * @param onDetailed  Callable invoked for detailed events: (float fromX, float toX, const Event&)
 *                    - Called when an event has > threshold pixels of space
 *                    - [fromX, toX) defines the half-open interval in screen space
 * @param onCoarse    Callable invoked for coarse regions: (float fromX, float toX)
 *                    - Called when multiple events are densely packed (spacing <= threshold)
 *                    - Adjacent coarse pixels are automatically merged into single calls
 *                    - [fromX, toX) defines the half-open interval in screen space
 * @param threshold   Minimum spacing (in pixels) to process event as detailed (default: 1.0)
 *                    - spacing > threshold → DETAILED
 *                    - spacing <= threshold → COARSE
 *
 * Preconditions:
 * - events must be sorted by time (ascending order)
 * - endTime >= events.back().time (if events not empty)
 */
template<typename Time, HasTime<Time> Event, TimelineMapper<Time> Mapper>
void processTimeline(
	std::span<const Event> events,
	Time endTime,
	const Mapper& mapper,
	std::invocable<float, float, const Event&> auto onDetailed,
	std::invocable<float, float> auto onCoarse,
	float threshold = 1.0)
{
	if (events.empty()) return;

	std::optional<float> coarseStart; // start of current coarse region
	auto flushCoarse = [&](float endPixel) {
		if (coarseStart) {
			onCoarse(*coarseStart, endPixel);
			coarseStart = {};
		}
	};

	auto currentEvent = events.begin();
	auto currentPixel = mapper.timeToX(currentEvent->time);
	while (currentEvent != events.end()) {
		// get next time (either next event or endTime)
		const auto nextEvent = std::next(currentEvent);
		const auto nextTimestamp = (nextEvent != events.end())
		                           ? nextEvent->time
		                           : endTime;
		const auto nextPixel = mapper.timeToX(nextTimestamp);
		const auto spacing = nextPixel - currentPixel; // event spans [currentPixel, nextPixel)

		if (spacing > threshold) {
			// DETAILED: flush pending coarse region first
			flushCoarse(currentPixel);
			onDetailed(currentPixel, nextPixel, *currentEvent);
			currentEvent = nextEvent; // reuse for next iteration
			currentPixel = nextPixel;
		} else {
			// COARSE: start/continue coarse region
			if (!coarseStart) coarseStart = currentPixel;

			// binary search: jump to next integer pixel boundary
			const auto targetPixel = std::floor(currentPixel) + 1.0f;
			const auto targetTime = mapper.xToTime(targetPixel);

			// Find last event at or before the next pixel boundary.
			// upper_bound gives first event > targetTime, then back up one.
			// Important: Start from next event to ensure we make progress.
			if (auto it = std::ranges::upper_bound(nextEvent, events.end(), targetTime, {}, &Event::time);
			    it != nextEvent) {
				currentEvent = std::prev(it);
				currentPixel = mapper.timeToX(currentEvent->time);
			} else {
				// no more events in this pixel, jump to next event
				currentEvent = nextEvent;
				currentPixel = nextPixel;
			}
		}
	}

	// flush final coarse region if any
	flushCoarse(currentPixel); // at this point: currentPixel == mapper.timeToX(endTime)
}

} // namespace openmsx

#endif
