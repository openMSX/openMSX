#include "ImGuiAdjust.hh"

#include <imgui.h>

#include <optional>

namespace openmsx {

// Adjust one dimension. So call it once for X, once for Y.
[[nodiscard]] static std::optional<float> adjustWindowPositionForResizedViewPort(
	float oldViewPortSize, float newViewPortSize, float relWindowPos, float windowSize)
{
	// Window can no longer fit in new viewPort.
	if (newViewPortSize < windowSize) return {};

	// Closer to left or right border (or top/bottom)?
	float windowEndPos = relWindowPos + windowSize;
	float distanceToLeft = relWindowPos;
	float distanceToRight = oldViewPortSize - windowEndPos;

	if (distanceToLeft < distanceToRight) {
		// Maintain the distance to the closest border.
		float newDistanceToLeft = distanceToLeft;
		float newWindowPos = newDistanceToLeft;
		float newWindowEndPos = newWindowPos + windowSize;
		float newDistanceToRight = newViewPortSize - newWindowEndPos;

		// Check if the other border becomes the closer one.
		if (newDistanceToLeft > newDistanceToRight) {
			// If so, place in the middle instead.
			newWindowPos = (newViewPortSize - windowSize) * 0.5f;
		}
		return newWindowPos;
	} else {
		float newDistanceToRight = distanceToRight;
		float newWidowEndPos = newViewPortSize - newDistanceToRight;
		float newWindowPos = newWidowEndPos - windowSize;
		float newDistanceToLeft = newWindowPos;
		if (newDistanceToRight > newDistanceToLeft) {
			newWindowPos = (newViewPortSize - windowSize) * 0.5f;
		}
		return newWindowPos;
	}
}

void AdjustWindowInMainViewPort::pre()
{
	if (setMainViewPort) {
		setMainViewPort = false;
		auto* mainViewPort = ImGui::GetMainViewport();
		ImGui::SetNextWindowViewport(mainViewPort->ID);
	}
}

bool AdjustWindowInMainViewPort::post()
{
	auto* mainViewPort = ImGui::GetMainViewport();
	auto* windowViewPort = ImGui::GetWindowViewport();
	bool isOnMainViewPort = windowViewPort == mainViewPort;
	gl::vec2 newViewPortSize = mainViewPort->Size;
	gl::vec2 mainViewPortPos = mainViewPort->Pos;
	gl::vec2 relWindowPos = gl::vec2(ImGui::GetWindowPos()) - mainViewPortPos;

	if (oldIsOnMainViewPort && (oldViewPortSize != newViewPortSize)) {
		gl::vec2 windowSize = ImGui::GetWindowSize();
		if (!isOnMainViewPort) {
			// previous frame we were in the main view port,
			// but now, after main view port has resized,
			// we're not. Try to pull us back into the main
			// view port.
			setMainViewPort = true;
			isOnMainViewPort = true;
			relWindowPos = oldRelWindowPos;
		}
		auto newPosX = adjustWindowPositionForResizedViewPort(
			oldViewPortSize.x, newViewPortSize.x, relWindowPos.x, windowSize.x);
		auto newPosY = adjustWindowPositionForResizedViewPort(
			oldViewPortSize.y, newViewPortSize.y, relWindowPos.y, windowSize.y);
		if (newPosX && newPosY) {
			relWindowPos = gl::vec2{*newPosX, *newPosY};
			ImGui::SetWindowPos(mainViewPortPos + relWindowPos);
		}
	}

	// data for next frame
	oldViewPortSize = newViewPortSize;
	oldRelWindowPos = relWindowPos;
	oldIsOnMainViewPort = isOnMainViewPort;

	return isOnMainViewPort;
}

} // namespace openmsx
