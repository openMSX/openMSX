#ifndef IMGUI_PART_HH
#define IMGUI_PART_HH

#include "StringOp.hh"
#include "stl.hh"
#include "zstring_view.hh"

#include <initializer_list>
#include <string_view>

#include <imgui.h>

namespace openmsx {

class MSXMotherBoard;

class ImGuiPart
{
public:
	[[nodiscard]] virtual zstring_view iniName() const { return ""; }
	virtual void save(ImGuiTextBuffer& /*buf*/) {}
	virtual void loadStart() {}
	virtual void loadLine(std::string_view /*name*/, zstring_view /*value*/) {}
	virtual void loadEnd() {}

	virtual void showMenu(MSXMotherBoard* /*motherBoard*/) {}
	virtual void paint(MSXMotherBoard* /*motherBoard*/) {}
};

////

template<typename C, typename T> struct PersistentElementBase {
	constexpr PersistentElementBase(zstring_view name_, T C::*p_)
	        : name(name_), p(p_) {}

	zstring_view name;
	T C::*p;

	T& get(C& c) const { return c.*p; }
};

// 'PersistentElement' base-case, will be partially specialized for concrete 'T'.
template<typename C, typename T>
struct PersistentElement;
template<typename C, typename T>
PersistentElement(zstring_view, T C::*) -> PersistentElement<C, T>;

template<typename C>
struct PersistentElement<C, int> : PersistentElementBase<C, int> {
	using PersistentElementBase<C, int>::PersistentElementBase;
	void save(ImGuiTextBuffer& buf, C& c) const {
		buf.appendf("%s=%d\n", this->name.c_str(), this->get(c));
	}
	void load(C& c, zstring_view value) const {
		if (auto r = StringOp::stringTo<int>(value)) {
			this->get(c) = *r;
		}
	}
};

template<typename C>
struct PersistentElementMax : PersistentElement<C, int> {
	int max;
	constexpr PersistentElementMax(zstring_view name_, int C::*p_, int max_)
		: PersistentElement<C, int>(name_, p_), max(max_) {}

	void load(C& c, std::string_view value) const {
		if (auto r = StringOp::stringTo<int>(value)) {
			if (0 <= *r && *r < max) this->get(c) = *r;
		}
	}
};

template<typename C>
struct PersistentElementMinMax : PersistentElement<C, int> {
	int min, max;
	constexpr PersistentElementMinMax(zstring_view name_, int C::*p_, int min_, int max_)
		: PersistentElement<C, int>(name_, p_), min(min_), max(max_) {}

	void load(C& c, std::string_view value) const {
		if (auto r = StringOp::stringTo<int>(value)) {
			if (min <= *r && *r < max) this->get(c) = *r;
		}
	}
};

template<typename C>
struct PersistentElementEnum : PersistentElement<C, int> {
	std::initializer_list<int> valid;
	constexpr PersistentElementEnum(zstring_view name_, int C::*p_, std::initializer_list<int> valid_)
		: PersistentElement<C, int>(name_, p_), valid(valid_) {}

	void load(C& c, std::string_view value) const {
		if (auto r = StringOp::stringTo<int>(value)) {
			if (contains(valid, *r)) this->get(c) = *r;
		}
	}
};

template<typename C>
struct PersistentElement<C, bool> : PersistentElementBase<C, bool> {
	using PersistentElementBase<C, bool>::PersistentElementBase;
	void save(ImGuiTextBuffer& buf, C& c) const {
		buf.appendf("%s=%d\n", this->name.c_str(), this->get(c));
	}
	void load(C& c, zstring_view value) const {
		this->get(c) = StringOp::stringToBool(value);
	}
};

template<typename C>
struct PersistentElement<C, float> : PersistentElementBase<C, float> {
	using PersistentElementBase<C, float>::PersistentElementBase;
	void save(ImGuiTextBuffer& buf, C& c) const {
		buf.appendf("%s=%f\n", this->name.c_str(), this->get(c));
	}
	void load(C& c, zstring_view value) const {
		this->get(c) = strtof(value.c_str(), nullptr); // TODO error handling
	}
};

template<typename C>
struct PersistentElement<C, gl::vec4> : PersistentElementBase<C, gl::vec4> {
	using PersistentElementBase<C, gl::vec4>::PersistentElementBase;
	void save(ImGuiTextBuffer& buf, C& c) const {
		const auto& v = this->get(c);
		buf.appendf("%s=[ %f %f %f %f ]\n", this->name.c_str(), v[0], v[1], v[2], v[3]);
	}
	void load(C& c, zstring_view value) const {
		gl::vec4 t;
		if (sscanf(value.c_str(), "[ %f %f %f %f ]", &t[0], &t[1], &t[2], &t[3]) == 4) {
			this->get(c) = t;
		}
	}
};


template<typename C, typename... Elements>
void savePersistent(ImGuiTextBuffer& buf, C& c, const std::tuple<Elements...>& tup)
{
	std::apply([&](auto& ...elem){(..., elem.save(buf, c));}, tup);
}

template<typename C, typename Elem>
bool checkLoad(std::string_view name, zstring_view value,
               C& c, Elem& elem)
{
	if (name != elem.name) return false;
	elem.load(c, value);
	return true;
}
template<typename C, typename... Elements>
bool loadOnePersistent(std::string_view name, zstring_view value,
              C& c, const std::tuple<Elements...>& tup)
{
	return std::apply([&](auto& ...elem){ return (... || checkLoad(name, value, c, elem));}, tup);
}

} // namespace openmsx

#endif
