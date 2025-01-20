#ifndef LAYER_HH
#define LAYER_HH

#include <utility>

namespace openmsx {

class Display;
class OutputSurface;

/** Interface for display layers.
  */
class Layer
{
public:
	/** Determines stacking order of layers:
	  * layers with higher Z-indices are closer to the viewer.
	  */
	enum class ZIndex {
		BACKGROUND,
		MSX_PASSIVE,
		MSX_ACTIVE,
		OSDGUI,
		IMGUI,
	};
	[[nodiscard]] friend auto operator<=>(ZIndex x, ZIndex y) { return std::to_underlying(x) <=> std::to_underlying(y); }

	/** Describes how much of the screen is currently covered by a particular
	  * layer.
	  */
	enum class Coverage {
		/** Layer fully covers the screen: any underlying layers are invisible.
		  */
		FULL,
		/** Layer partially covers the screen: it may cover only part of the
		  * screen area, or it may be (semi-)transparent in places.
		  */
		PARTIAL,
		/** Layer is not visible, that is completely transparent.
		  */
		NONE
	};

	virtual ~Layer() = default;

	/** Paint this layer.
	  */
	virtual void paint(OutputSurface& output) = 0;

	/** Query the Z-index of this layer.
	  */
	[[nodiscard]] ZIndex getZ() const { return z; }
	[[nodiscard]] bool isActive() const { return getZ() == ZIndex::MSX_ACTIVE; }

	/** Query the coverage of this layer.
	 */
	[[nodiscard]] Coverage getCoverage() const { return coverage; }

	/** Store pointer to Display.
	  * Will be called by Display::addLayer().
	  */
	void setDisplay(Display& display_) { display = &display_; }

protected:
	/** Construct a layer. */
	explicit Layer(Coverage coverage_, ZIndex z_)
		: coverage(coverage_), z(z_)
	{
	}

	/** Changes the current coverage of this layer.
	  */
	void setCoverage(Coverage coverage_) { coverage = coverage_; }

	/** Changes the current Z-index of this layer.
	  */
	void setZ(ZIndex z);

private:
	/** The display this layer is part of. */
	Display* display = nullptr;

	/** Inspected by Display to determine which layers to paint. */
	Coverage coverage;

	/** Inspected by Display to determine the order in which layers are
	  * painted.
	  */
	ZIndex z;

	friend class ScopedLayerHider; // for setCoverage()
};


class ScopedLayerHider
{
public:
	explicit ScopedLayerHider(Layer& layer);
	ScopedLayerHider(const ScopedLayerHider&) = delete;
	ScopedLayerHider(ScopedLayerHider&&) = delete;
	ScopedLayerHider& operator=(const ScopedLayerHider&) = delete;
	ScopedLayerHider& operator=(ScopedLayerHider&&) = delete;
	~ScopedLayerHider();
private:
	Layer& layer;
	Layer::Coverage originalCoverage;
};

} // namespace openmsx

#endif
