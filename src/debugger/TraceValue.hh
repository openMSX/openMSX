#ifndef TRACE_VALUE_HH
#define TRACE_VALUE_HH

// TODO unittest

#include "one_of.hh"
#include "zstring_view.hh"

#include <algorithm>
#include <array>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <cstring>
#include <optional>
#include <utility>

// Stores a trace-value (void, int, double, string) in a memory efficient way.
// Goal is to be able to store thousands or even a few million of these objects
// without consuming a ton of memory.
//
// The current solution requires 16 bytes per value. Except for strings longer
// than 15 characters we need an additional heap-allocation equal to the size of
// that string plus one byte.
//
// Internally we have an array of 16 bytes. The last byte is a tag-byte
// indicating the type of the stored value. Small strings up to 15 characters
// are stored directly in this array, for long strings we instead store a
// pointer to a heap allocated string. In both cases the string is
// zero-terminated (tradeoff: allows to omit storing the length of the string,
// but we cannot store strings with embedded zeros).
//
// In case of a 15 character short string, the zero terminator would overwrite
// the tag-byte. For that reason we've choosen tag-value = 0 to mean "short
// string".

static constexpr auto TraceValueAlignment = std::max({
	alignof(char*), alignof(uint64_t), alignof(double)});
struct alignas(TraceValueAlignment) TraceValue {
public:
	enum class Tag : char {
		SSO = 0, // small string, raw[15]==0 acts as NUL terminator
		HeapStr = 1, // owning heap-allocated NUL-terminated string
		Monostate = 2, // no value (void)
		U64 = 3, // some integer (up to 64 bits), or bool
		Double = 4, // floating point
	};

	TraceValue() = default; // empty string (zero-filled payload, tag==SSO)

	TraceValue(std::string_view s)   { set_string(s); }
	TraceValue(std::monostate)       { set_monostate(); }
	TraceValue(std::integral auto x) { set_u64(uint64_t(x)); }
	TraceValue(double d)             { set_double(d); }

	TraceValue(const TraceValue& other) {
		if (other.tag() == Tag::HeapStr) {
			const char* src = other.read_payload<const char*>();
			assert(src != nullptr);
			size_t len = std::strlen(src);
			char* dst = new char[len + 1];
			memcpy(dst, src, len + 1);
			write_payload(dst);
		} else {
			memcpy(raw.data(), other.raw.data(), 16);
		}
	}

	TraceValue(TraceValue&& other) noexcept {
		memcpy(raw.data(), other.raw.data(), 16);
		other.raw.fill(0);
	}

	TraceValue& operator=(const TraceValue& other) {
		TraceValue tmp(other);
		swap(tmp);
		return *this;
	}

	TraceValue& operator=(TraceValue&& other) noexcept {
		swap(other);
		return *this;
	}

	~TraceValue() noexcept { cleanup(); }

	void swap(TraceValue& other) noexcept { raw.swap(other.raw); }

	TraceValue& operator=(std::string_view s) { set_string(s); return *this; }
	TraceValue& operator=(const char* s)      { set_string(std::string_view(s)); return *this; }
	TraceValue& operator=(std::monostate)     { set_monostate(); return *this; }
	TraceValue& operator=(uint64_t x)         { set_u64(x); return *this; }
	TraceValue& operator=(double d)           { set_double(d); return *this; }

	bool operator==(const TraceValue& other) const {
		 // must be the same type, e.g. integer 123 and string "123" don't match
		if (tag() != other.tag()) return false;

		switch (tag()) {
		case Tag::SSO:
		case Tag::HeapStr:   return get_string() == other.get_string();
		case Tag::Monostate: return true;
		case Tag::U64:       return get_u64()    == other.get_u64();
		case Tag::Double:    return get_double() == other.get_double();
		default: std::unreachable();
		}
	}

	[[nodiscard]] size_t index() const { return std::to_underlying(tag()); }

	template<typename T> [[nodiscard]] bool holds_alternative() const = delete;
	template<typename T> [[nodiscard]] T get() const = delete;
	template<typename T> [[nodiscard]] std::optional<T> get_if() const = delete;

	template<typename Visitor>
	decltype(auto) visit(Visitor&& vis) const {
		switch (tag()) {
		case Tag::SSO:
		case Tag::HeapStr:   return std::forward<Visitor>(vis)(get_string());
		case Tag::Monostate: return std::forward<Visitor>(vis)(get_monostate());
		case Tag::U64:       return std::forward<Visitor>(vis)(get_u64());
		case Tag::Double:    return std::forward<Visitor>(vis)(get_double());
		default: std::unreachable();
		}
	}

	void set_string(std::string_view sv) {
		cleanup();
		size_t len = sv.size();
		if (len <= (raw.size() - 1)) {
			memcpy(raw.data(), sv.data(), len);
			raw[len] = 0; // possibly this overlaps with the tag byte, that's okay
			set_tag(Tag::SSO); // Tag::SSO == 0
		} else {
			char* p = new char[len + 1];
			memcpy(p, sv.data(), len);
			p[len] = '\0';
			write_payload<char*>(p);
			set_tag(Tag::HeapStr);
		}
	}
	[[nodiscard]] zstring_view get_string() const {
		if (tag() == Tag::SSO) {
			return {raw.data()};
		} else {
			assert(tag() == Tag::HeapStr);
			return {read_payload<const char*>()};
		}
	}

	void set_monostate() {
		set_tag(Tag::Monostate);
	}
	[[nodiscard]] std::monostate get_monostate() const {
		assert(tag() == Tag::Monostate);
		return {};
	}

	void set_u64(uint64_t v) {
		cleanup();
		write_payload<uint64_t>(v);
		set_tag(Tag::U64);
	}
	[[nodiscard]] uint64_t get_u64() const {
		assert(tag() == Tag::U64);
		return read_payload<uint64_t>();
	}

	void set_double(double d) {
		cleanup();
		write_payload<double>(d);
		set_tag(Tag::Double);
	}
	[[nodiscard]] double get_double() const {
		assert(tag() == Tag::Double);
		return read_payload<double>();
	}

private:
	[[nodiscard]] Tag tag() const { return Tag(raw.back()); }
	void set_tag(Tag t) { raw.back() = char(t); }

	// Helper templates to read/write typed values from the payload bytes.
	template<typename T> [[nodiscard]] T read_payload() const {
		T tmp{};
		memcpy(static_cast<void*>(&tmp), raw.data(), sizeof(T));
		return tmp;
	}
	template<typename T> void write_payload(const T& v) {
		memcpy(raw.data(), static_cast<const void*>(&v), sizeof(T));
	}

	void cleanup() {
		if (tag() == Tag::HeapStr) {
			delete[] read_payload<char*>();
		}
	}

private:
	std::array<char, 16> raw = {}; // raw[0..14] payload, raw[15] = tag
};


// explicit holds_alternative specializations
template<> inline bool TraceValue::holds_alternative<zstring_view>() const {
	return tag() == one_of(Tag::SSO, Tag::HeapStr);
}
template<> inline bool TraceValue::holds_alternative<std::monostate>() const {
	return tag() == Tag::Monostate;
}
template<> inline bool TraceValue::holds_alternative<uint64_t>() const {
	return tag() == Tag::U64;
}
template<> inline bool TraceValue::holds_alternative<double>() const {
	return tag() == Tag::Double;
}

// explicit get<T> specializations
template<> inline zstring_view TraceValue::get<zstring_view>() const {
	return get_string();
}
template<> inline std::monostate TraceValue::get<std::monostate>() const {
	return get_monostate();
}
template<> inline uint64_t TraceValue::get<uint64_t>() const {
	return get_u64();
}
template<> inline double TraceValue::get<double>() const {
	return get_double();
}

// explicit get_if<T> specializations
template<> inline std::optional<zstring_view> TraceValue::get_if<zstring_view>() const {
	if (tag() == one_of(Tag::SSO, Tag::HeapStr)) return get_string();
	return {};
}
template<> inline std::optional<std::monostate> TraceValue::get_if<std::monostate>() const {
	if (tag() == Tag::Monostate) return get_monostate();
	return {};
}
template<> inline std::optional<uint64_t> TraceValue::get_if<uint64_t>() const {
	if (tag() == Tag::U64) return get_u64();
	return {};
}
template<> inline std::optional<double> TraceValue::get_if<double>() const {
	if (tag() == Tag::Double) return get_double();
	return {};
}

#endif
