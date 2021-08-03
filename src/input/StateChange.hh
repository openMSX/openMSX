#ifndef STATECHANGE_HH
#define STATECHANGE_HH

#include "EmuTime.hh"
#include "TclObject.hh"
#include "dynarray.hh"
#include "openmsx.hh"
#include "serialize_meta.hh"
#include "view.hh"
#include <type_traits>
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
	[[nodiscard]] EmuTime::param getTime() const
	{
		return time;
	}

	template<typename Archive>
	void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.serialize("time", time);
	}

protected:
	StateChangeBase() : time(EmuTime::zero()) {} // for serialize
	explicit StateChangeBase(EmuTime::param time_)
		: time(time_)
	{
	}

private:
	EmuTime time;
};
REGISTER_BASE_NAME_HELPER(StateChangeBase, "StateChange");


class EndLogEvent final : public StateChangeBase
{
public:
	EndLogEvent() = default; // for serialize
	explicit EndLogEvent(EmuTime::param time_)
		: StateChangeBase(time_)
	{
	}

	template<typename Archive> void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.template serializeBase<StateChangeBase>(*this);
	}
};


class KeyMatrixState final : public StateChangeBase
{
public:
	KeyMatrixState() = default; // for serialize
	KeyMatrixState(EmuTime::param time_, byte row_, byte press_, byte release_)
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
	[[nodiscard]] byte getRow()     const { return row; }
	[[nodiscard]] byte getPress()   const { return press; }
	[[nodiscard]] byte getRelease() const { return release; }

	template<typename Archive> void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.template serializeBase<StateChangeBase>(*this);
		ar.serialize("row",     row,
		             "press",   press,
		             "release", release);
	}
private:
	byte row, press, release;
};


class ArkanoidState final : public StateChangeBase
{
public:
	ArkanoidState() = default; // for serialize
	ArkanoidState(EmuTime::param time_, int delta_, bool press_, bool release_)
		: StateChangeBase(time_)
		, delta(delta_), press(press_), release(release_) {}
	[[nodiscard]] int  getDelta()   const { return delta; }
	[[nodiscard]] bool getPress()   const { return press; }
	[[nodiscard]] bool getRelease() const { return release; }

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

	MSXCommandEvent(EmuTime::param time_, span<const TclObject> tokens_)
		: StateChangeBase(time_), tokens(tokens_.size())
	{
		for (auto i : xrange(tokens.size())) {
			tokens[i] = tokens_[i];
		}
	}

	[[nodiscard]] const auto& getTokens() const { return tokens; }

	template<typename Archive>
	void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.template serializeBase<StateChangeBase>(*this);

		// serialize vector<TclObject> as vector<string>
		std::vector<std::string> str;
		if constexpr (!Archive::IS_LOADER) {
			str = to_vector(view::transform(
				tokens, [](auto& t) { return std::string(t.getString()); }));
		}
		ar.serialize("tokens", str);
		if constexpr (Archive::IS_LOADER) {
			assert(tokens.empty());
			size_t n = str.size();
			tokens = dynarray<TclObject>(n);
			for (auto i : xrange(n)) {
				tokens[i] = TclObject(str[i]);
			}
		}
	}

private:
	dynarray<TclObject> tokens;
};


enum class KeyJoyID { ID1, ID2, UNKNOWN };
inline std::string_view nameForId(KeyJoyID id)
{
	switch (id) {
		case KeyJoyID::ID1: return "keyjoystick1";
		case KeyJoyID::ID2: return "keyjoystick2";
		default: return "unknown-keyjoystick";
	}
}
class KeyJoyState final : public StateChangeBase
{
public:
	KeyJoyState() = default; // for serialize
	KeyJoyState(EmuTime::param time_, KeyJoyID id_,
	            byte press_, byte release_)
		: StateChangeBase(time_)
		, id(id_), press(press_), release(release_) {}
	[[nodiscard]] auto getId() const { return id; }
	[[nodiscard]] byte getPress()   const { return press; }
	[[nodiscard]] byte getRelease() const { return release; }
	template<typename Archive> void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.template serializeBase<StateChangeBase>(*this);
		// for backwards compatibility serialize 'id' as 'name'
		std::string name = Archive::IS_LOADER ? "" : std::string(nameForId(id));
		ar.serialize("name",    name,
		             "press",   press,
		             "release", release);
		if constexpr (Archive::IS_LOADER) {
			id = (name == nameForId(KeyJoyID::ID1)) ? KeyJoyID::ID1
			   : (name == nameForId(KeyJoyID::ID2)) ? KeyJoyID::ID2
			   :                                      KeyJoyID::UNKNOWN;
		}
	}

private:
	KeyJoyID id;
	byte press, release;
};


class TrackballState final : public StateChangeBase
{
public:
	TrackballState() = default; // for serialize
	TrackballState(EmuTime::param time_, int deltaX_, int deltaY_,
	                                     byte press_, byte release_)
		: StateChangeBase(time_)
		, deltaX(deltaX_), deltaY(deltaY_)
		, press(press_), release(release_) {}
	[[nodiscard]] int  getDeltaX()  const { return deltaX; }
	[[nodiscard]] int  getDeltaY()  const { return deltaY; }
	[[nodiscard]] byte getPress()   const { return press; }
	[[nodiscard]] byte getRelease() const { return release; }

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
	byte press, release;
};


enum class AutofireID { RENSHATURBO, UNKNOWN };
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
	AutofireStateChange(EmuTime::param time_, AutofireID id_, int value_)
		: StateChangeBase(time_)
		, id(id_), value(value_) {}
	[[nodiscard]] auto getId() const { return id; }
	[[nodiscard]] int getValue() const { return value; }
	template<typename Archive> void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.template serializeBase<StateChangeBase>(*this);
		// for backwards compatibility serialize 'id' as 'name'
		std::string name = Archive::IS_LOADER ? "" : std::string(nameForId(id));
		ar.serialize("name",  name,
		             "value", value);
		if constexpr (Archive::IS_LOADER) {
			id = (name == nameForId(AutofireID::RENSHATURBO))
			   ? AutofireID::RENSHATURBO
			   : AutofireID::UNKNOWN;
		}
	}

private:
	AutofireID id;
	int value;
};


class JoyState final : public StateChangeBase
{
public:
	JoyState() = default; // for serialize
	JoyState(EmuTime::param time_, unsigned joyNum_, byte press_, byte release_)
		: StateChangeBase(time_)
		, joyNum(joyNum_), press(press_), release(release_)
	{
		assert((press != 0) || (release != 0));
		assert((press & release) == 0);
	}
	[[nodiscard]] unsigned getJoystick() const { return joyNum; }
	[[nodiscard]] byte     getPress()    const { return press; }
	[[nodiscard]] byte     getRelease()  const { return release; }

	template<typename Archive> void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.template serializeBase<StateChangeBase>(*this);
		ar.serialize("joyNum",  joyNum,
		             "press",   press,
		             "release", release);
	}
private:
	unsigned joyNum;
	byte press, release;
};


class PaddleState final : public StateChangeBase
{
public:
	PaddleState() = default; // for serialize
	PaddleState(EmuTime::param time_, int delta_)
		: StateChangeBase(time_), delta(delta_) {}
	[[nodiscard]] int getDelta() const { return delta; }

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
	TouchpadState(EmuTime::param time_,
	              byte x_, byte y_, bool touch_, bool button_)
		: StateChangeBase(time_)
		, x(x_), y(y_), touch(touch_), button(button_) {}
	[[nodiscard]] byte getX()      const { return x; }
	[[nodiscard]] byte getY()      const { return y; }
	[[nodiscard]] bool getTouch()  const { return touch; }
	[[nodiscard]] bool getButton() const { return button; }

	template<typename Archive> void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.template serializeBase<StateChangeBase>(*this);
		ar.serialize("x",      x,
		             "y",      y,
		             "touch",  touch,
		             "button", button);
	}
private:
	byte x, y;
	bool touch, button;
};


class MouseState final : public StateChangeBase
{
public:
	MouseState() = default; // for serialize
	MouseState(EmuTime::param time_, int deltaX_, int deltaY_,
	           byte press_, byte release_)
		: StateChangeBase(time_)
		, deltaX(deltaX_), deltaY(deltaY_)
		, press(press_), release(release_) {}
	[[nodiscard]] int  getDeltaX()  const { return deltaX; }
	[[nodiscard]] int  getDeltaY()  const { return deltaY; }
	[[nodiscard]] byte getPress()   const { return press; }
	[[nodiscard]] byte getRelease() const { return release; }
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
	byte press, release;
};


class JoyMegaState final : public StateChangeBase
{
public:
	JoyMegaState() = default; // for serialize
	JoyMegaState(EmuTime::param time_, unsigned joyNum_,
	             unsigned press_, unsigned release_)
		: StateChangeBase(time_)
		, joyNum(joyNum_), press(press_), release(release_)
	{
		assert((press != 0) || (release != 0));
		assert((press & release) == 0);
	}
	[[nodiscard]] unsigned getJoystick() const { return joyNum; }
	[[nodiscard]] unsigned getPress()    const { return press; }
	[[nodiscard]] unsigned getRelease()  const { return release; }

	template<typename Archive> void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.template serializeBase<StateChangeBase>(*this);
		ar.serialize("joyNum",  joyNum,
		             "press",   press,
		             "release", release);
	}
private:
	unsigned joyNum;
	unsigned press, release;
};

using StateChange = std::variant<
	EndLogEvent,
	KeyMatrixState,
	ArkanoidState,
	MSXCommandEvent,
	KeyJoyState,
	TrackballState,
	AutofireStateChange,
	JoyState,
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
	static constexpr std::initializer_list<enum_string<size_t>> list = {
		{"EndLog",              index<EndLogEvent>        },
		{"KeyMatrixState",      index<KeyMatrixState>     },
		{"ArkanoidState",       index<ArkanoidState>      },
		{"MSXCommandEvent",     index<MSXCommandEvent>    },
		{"KeyJoyState",         index<KeyJoyState>        },
		{"TrackballState",      index<TrackballState>     },
		{"AutofireStateChange", index<AutofireStateChange>},
		{"JoyState",            index<JoyState>           },
		{"PaddleState",         index<PaddleState>        },
		{"TouchpadState",       index<TouchpadState>      },
		{"MouseState",          index<MouseState>         },
		{"JoyMegaState",        index<JoyMegaState>       }};
};

} // namespace openmsx

#endif
