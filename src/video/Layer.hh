#ifndef LAYER_HH
#define LAYER_HH

namespace openmsx {

class OutputSurface;
class LayerListener;

/** Interface for display layers.
  */
class Layer
{
public:
	/** Determines stacking order of layers:
	  * layers with higher Z-indices are closer to the viewer.
	  */
	enum ZIndex {
		Z_DUMMY = -1,
		Z_BACKGROUND = 0,
		Z_MSX_PASSIVE = 30,
		Z_MSX_ACTIVE = 40,
		Z_OSDGUI = 50,
		Z_IMGUI = 60,
	};

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
	[[nodiscard]] bool isActive() const { return getZ() == Z_MSX_ACTIVE; }

	/** Query the coverage of this layer.
	 */
	[[nodiscard]] Coverage getCoverage() const { return coverage; }

	/** Store pointer to Display.
	  * Will be called by Display::addLayer().
	  */
	void setDisplay(LayerListener& display_) { display = &display_; }

protected:
	/** Construct a layer. */
	explicit Layer(Coverage coverage_ = Coverage::NONE, ZIndex z_ = Z_DUMMY)
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
	LayerListener* display = nullptr;

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
