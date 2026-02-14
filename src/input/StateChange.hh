#ifndef STATECHANGE_HH
#define STATECHANGE_HH

#include "EmuTime.hh"
#include "TclObject.hh"

#include "dynarray.hh"
#include "serialize_meta.hh"

#include <variant>
#include <vector>

namespace openmsx {

/** Base class for all external MSX state changing events.
 * These are typically triggered by user input, like keyboard presses. The main
 * reason why these events exist is to be able to record and replay them.
 */
class StateChangeBase
{
public:
	[[nodiscard]] EmuTime getTime() const { return time; }

	template<typename Archive>
	void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.serialize("time", time);
	}

protected:
	StateChangeBase() = default; // for serialize
	explicit StateChangeBase(EmuTime time_)
		: time(time_) {}

private:
	EmuTime time = EmuTime::zero();
};
REGISTER_BASE_NAME_HELPER(StateChangeBase, "StateChange");


class EndLogEvent final : public StateChangeBase
{
public:
	EndLogEvent() = default; // for serialize
	explicit EndLogEvent(EmuTime time_)
		: StateChangeBase(time_) {}

	template<typename Archive> void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.template serializeBase<StateChangeBase>(*this);
	}
};


class KeyMatrixState final : public StateChangeBase
{
public:
	KeyMatrixState() = default; // for serialize
	KeyMatrixState(EmuTime time_, uint8_t row_, uint8_t press_, uint8_t release_)
		: StateChangeBase(time_)
		, row(row_), press(press_), release(release_)
	{
		// disallow useless events
		assert((press != 0) || (release != 0));
		// avoid confusion about what happens when some bits are both
		// set and reset (in other words: don't rely on order of and-
		// and or-operations)
		assert((press & release) == 0);
	}

	[[nodiscard]] auto getRow()     const { return row; }
	[[nodiscard]] auto getPress()   const { return press; }
	[[nodiscard]] auto getRelease() const { return release; }

	template<typename Archive> void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.template serializeBase<StateChangeBase>(*this);
		ar.serialize("row",     row,
		             "press",   press,
		             "release", release);
	}
private:
	uint8_t row, press, release;
};


class ArkanoidState final : public StateChangeBase
{
public:
	ArkanoidState() = default; // for serialize
	ArkanoidState(EmuTime time_, int delta_, bool press_, bool release_)
		: StateChangeBase(time_)
		, delta(delta_), press(press_), release(release_) {}

	[[nodiscard]] auto getDelta()   const { return delta; }
	[[nodiscard]] auto getPress()   const { return press; }
	[[nodiscard]] auto getRelease() const { return release; }

	template<typename Archive> void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.template serializeBase<StateChangeBase>(*this);
		ar.serialize("delta",   delta,
		             "press",   press,
		             "release", release);
	}
private:
	int delta;
	bool press, release;
};


/** This class is used to for Tcl commands that directly influence the MSX
  * state (e.g. plug, disk<x>, cassetteplayer, reset). It's passed via an
  * event because the recording needs to see these.
  */
class MSXCommandEvent final : public StateChangeBase
{
public:
	MSXCommandEvent() = default; // for serialize

	MSXCommandEvent(EmuTime time_, std::span<const TclObject> tokens_)
		: StateChangeBase(time_)
		, tokens(dynarray<TclObject>::construct_from_range_tag{}, tokens_)
	{
	}

	[[nodiscard]] const auto& getTokens() const { return tokens; }

	template<typename Archive>
	void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.template serializeBase<StateChangeBase>(*this);

		// serialize vector<TclObject> as vector<string>
		std::vector<std::string> str;
		if constexpr (!Archive::IS_LOADER) {
			str = to_vector(std::views::transform(
				tokens, [](auto& t) { return std::string(t.getString()); }));
		}
		ar.serialize("tokens", str);
		if constexpr (Archive::IS_LOADER) {
			assert(tokens.empty());
			tokens = dynarray<TclObject>(dynarray<TclObject>::construct_from_range_tag{},
				std::views::transform(str, [](const auto& s) { return TclObject(s); }));
		}
	}

private:
	dynarray<TclObject> tokens;
};


class TrackballState final : public StateChangeBase
{
public:
	TrackballState() = default; // for serialize
	TrackballState(EmuTime time_, int deltaX_, int deltaY_,
	               uint8_t press_, uint8_t release_)
		: StateChangeBase(time_)
		, deltaX(deltaX_), deltaY(deltaY_)
		, press(press_), release(release_) {}

	[[nodiscard]] auto getDeltaX()  const { return deltaX; }
	[[nodiscard]] auto getDeltaY()  const { return deltaY; }
	[[nodiscard]] auto getPress()   const { return press; }
	[[nodiscard]] auto getRelease() const { return release; }

	template<typename Archive> void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.template serializeBase<StateChangeBase>(*this);
		ar.serialize("deltaX",  deltaX,
		             "deltaY",  deltaY,
		             "press",   press,
		             "release", release);
	}
private:
	int deltaX, deltaY;
	uint8_t press, release;
};


enum class AutofireID : uint8_t { RENSHATURBO, UNKNOWN };
inline std::string_view nameForId(AutofireID id)
{
	switch (id) {
		case AutofireID::RENSHATURBO: return "renshaturbo";
		default: return "unknown-autofire";
	}
}
class AutofireStateChange final : public StateChangeBase
{
public:
	AutofireStateChange() = default; // for serialize
	AutofireStateChange(EmuTime time_, AutofireID id_, int value_)
		: StateChangeBase(time_)
		, id(id_), value(value_) {}

	[[nodiscard]] auto getId()    const { return id; }
	[[nodiscard]] auto getValue() const { return value; }

	template<typename Archive> void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.template serializeBase<StateChangeBase>(*this);
		// for backwards compatibility serialize 'id' as 'name'
		std::string name = Archive::IS_LOADER ? "" : std::string(nameForId(id));
		ar.serialize("name",  name,
		             "value", value);
		if constexpr (Archive::IS_LOADER) {
			using enum AutofireID;
			id = (name == nameForId(RENSHATURBO)) ? RENSHATURBO : UNKNOWN;
		}
	}
private:
	AutofireID id;
	int value;
};


class MSXJoyState final : public StateChangeBase
{
public:
	MSXJoyState() = default; // for serialize
	MSXJoyState(EmuTime time_, uint8_t id_,
	            uint8_t press_, uint8_t release_)
		: StateChangeBase(time_), id(id_)
		, press(press_), release(release_) {}

	[[nodiscard]] auto getId()      const { return id; }
	[[nodiscard]] auto getPress()   const { return press; }
	[[nodiscard]] auto getRelease() const { return release; }

	template<typename Archive> void serialize(Archive& ar, unsigned /*version*/) {
		ar.template serializeBase<StateChangeBase>(*this);
		ar.serialize("id",      id,
		             "press",   press,
		             "release", release);
	}
private:
	uint8_t id, press, release;
};


class JoyMegaState final : public StateChangeBase
{
public:
	JoyMegaState() = default; // for serialize
	JoyMegaState(EmuTime time_, uint8_t id_,
	             unsigned press_, unsigned release_)
		: StateChangeBase(time_)
		, press(press_), release(release_), id(id_) {}

	[[nodiscard]] auto getId()       const { return id; }
	[[nodiscard]] auto getPress()    const { return press; }
	[[nodiscard]] auto getRelease()  const { return release; }

	template<typename Archive> void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.template serializeBase<StateChangeBase>(*this);
		ar.serialize("id",      id,
		             "press",   press,
		             "release", release);
	}
private:
	unsigned press, release;
	uint8_t id;
};


class PaddleState final : public StateChangeBase
{
public:
	PaddleState() = default; // for serialize
	PaddleState(EmuTime time_, int delta_)
		: StateChangeBase(time_), delta(delta_) {}

	[[nodiscard]] auto getDelta() const { return delta; }

	template<typename Archive> void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.template serializeBase<StateChangeBase>(*this);
		ar.serialize("delta", delta);
	}
private:
	int delta;
};


class TouchpadState final : public StateChangeBase
{
public:
	TouchpadState() = default; // for serialize
	TouchpadState(EmuTime time_,
	              uint8_t x_, uint8_t y_, bool touch_, bool button_)
		: StateChangeBase(time_)
		, x(x_), y(y_), touch(touch_), button(button_) {}

	[[nodiscard]] auto getX()      const { return x; }
	[[nodiscard]] auto getY()      const { return y; }
	[[nodiscard]] auto getTouch()  const { return touch; }
	[[nodiscard]] auto getButton() const { return button; }

	template<typename Archive> void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.template serializeBase<StateChangeBase>(*this);
		ar.serialize("x",      x,
		             "y",      y,
		             "touch",  touch,
		             "button", button);
	}
private:
	uint8_t x, y;
	bool touch, button;
};


class MouseState final : public StateChangeBase
{
public:
	MouseState() = default; // for serialize
	MouseState(EmuTime time_, int deltaX_, int deltaY_,
	           uint8_t press_, uint8_t release_)
		: StateChangeBase(time_)
		, deltaX(deltaX_), deltaY(deltaY_)
		, press(press_), release(release_) {}

	[[nodiscard]] auto getDeltaX()  const { return deltaX; }
	[[nodiscard]] auto getDeltaY()  const { return deltaY; }
	[[nodiscard]] auto getPress()   const { return press; }
	[[nodiscard]] auto getRelease() const { return release; }

	template<typename Archive> void serialize(Archive& ar, unsigned version)
	{
		ar.template serializeBase<StateChangeBase>(*this);
		ar.serialize("deltaX",  deltaX,
		             "deltaY",  deltaY,
		             "press",   press,
		             "release", release);
		if (ar.versionBelow(version, 2)) {
			assert(Archive::IS_LOADER);
			// Old versions stored host (=unscaled) mouse movement
			// in the replay-event-log. Apply the (old) algorithm
			// to scale host to msx mouse movement.
			// In principle the code snippet below does:
			//    delta{X,Y} /= SCALE
			// except that it doesn't accumulate rounding errors
			static constexpr int SCALE = 2;
			int oldMsxX = absHostX / SCALE;
			int oldMsxY = absHostY / SCALE;
			absHostX += deltaX;
			absHostY += deltaY;
			int newMsxX = absHostX / SCALE;
			int newMsxY = absHostY / SCALE;
			deltaX = newMsxX - oldMsxX;
			deltaY = newMsxY - oldMsxY;
		}
	}
private:
	int deltaX, deltaY; // msx mouse movement
	uint8_t press, release;
public:
	inline static int absHostX = 0, absHostY = 0; // (only) for old savestates
};
SERIALIZE_CLASS_VERSION(MouseState, 2);


using StateChange = std::variant<
	EndLogEvent,
	KeyMatrixState,
	ArkanoidState,
	MSXCommandEvent,
	TrackballState,
	AutofireStateChange,
	MSXJoyState,
	PaddleState,
	TouchpadState,
	MouseState,
	JoyMegaState
>;

inline auto getTime(const StateChange& event)
{
	return std::visit(
		[](const StateChangeBase& e) { return e.getTime(); },
		event);
}


template<> struct Serializer<StateChange> : VariantSerializer<StateChange> {
	static constexpr auto stateChangeInfo = std::to_array<enum_string<size_t>>({
		{"EndLog",              index<EndLogEvent>        },
		{"KeyMatrixState",      index<KeyMatrixState>     },
		{"ArkanoidState",       index<ArkanoidState>      },
		{"MSXCommandEvent",     index<MSXCommandEvent>    },
		{"TrackballState",      index<TrackballState>     },
		{"AutofireStateChange", index<AutofireStateChange>},
		{"MSXJoyState",         index<MSXJoyState>        },
		{"PaddleState",         index<PaddleState>        },
		{"TouchpadState",       index<TouchpadState>      },
		{"MouseState",          index<MouseState>         },
		{"JoyMegaState",        index<JoyMegaState>       }
	});
	static constexpr std::span<const enum_string<size_t>> info() {
		return stateChangeInfo;
	}
};

} // namespace openmsx

#endif
