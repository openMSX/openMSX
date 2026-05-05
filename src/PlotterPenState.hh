#ifndef PLOTTER_PEN_STATE_HH
#define PLOTTER_PEN_STATE_HH

namespace openmsx {

class PlotterPenState
{
public:
	[[nodiscard]] static PlotterPenState downForDrawing()
	{
		PlotterPenState state;
		state.penDown = true;
		return state;
	}

	[[nodiscard]] bool isDown() const
	{
		return penDown;
	}

	[[nodiscard]] bool lower()
	{
		if (penDown) return false;
		penDown = true;
		return true;
	}

	void lift()
	{
		penDown = false;
	}

private:
	bool penDown = false;
};

} // namespace openmsx

#endif
