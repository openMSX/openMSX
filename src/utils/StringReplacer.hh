#ifndef STRING_REPLACER_HH
#define STRING_REPLACER_HH

// First create a 'replacer' by passing a list of replacement-pairs
// (creation can fully be evaluated at-compile-time):
//     static constexpr auto replacer = StringReplacer::create(
//         "original", "replacement",
//         ...
//         "some string", "another string");
// Then use it to replace strings:
//     assert(replacer("original") == "replacement");    // enumerate strings are replaced
//     assert(replacer("not-in-list") == "not-in-list"); // not-enumerated strings are left unchanged
//
// So this is equivalent to
//     string_view input = ...
//     if      (input == "original"  ) input = "replacement";
//     ...
//     else if (input == "some string) input = "another string";
//     else // leave 'input' unchanged
// Though (for a long list of replacement-pairs) this utility may run faster
// than a manual chain of comparisons. This is because internally uses
// perfect-hashing to speed up the string comparisons.
//
// Note: the return type of StringReplacer::create() is intentionally
// unspecified (and it can indeed be different for different inputs).

#include "MinimalPerfectHash.hh"

#include <array>
#include <cassert>
#include <string_view>
#include <tuple>
#include <utility>

namespace StringReplacer {
namespace detail {

[[nodiscard]] inline constexpr unsigned fnvHash(std::string_view s)
{
	constexpr unsigned PRIME = 0x811C9DC5;
	unsigned hash = s.size();
	for (size_t i = 0; i < s.size(); ++i) {
		hash *= PRIME;
		hash ^= s[i];
	}
	return hash;
}


[[nodiscard]] inline constexpr auto create_simple_replacer()
{
    return [](std::string_view s) { return s; };
}

[[nodiscard]] inline constexpr auto create_simple_replacer(
    std::string_view from1, std::string_view to1)
{
    return [=](std::string_view s) {
        if (s == from1) return to1;
        return s;
    };
}

[[nodiscard]] inline constexpr auto create_simple_replacer(
    std::string_view from1, std::string_view to1,
    std::string_view from2, std::string_view to2)
{
    return [=](std::string_view s) {
        if (s == from1) return to1;
        if (s == from2) return to2;
        return s;
    };
}

[[nodiscard]] inline constexpr auto create_simple_replacer(
    std::string_view from1, std::string_view to1,
    std::string_view from2, std::string_view to2,
    std::string_view from3, std::string_view to3)
{
    return [=](std::string_view s) {
        if (s == from1) return to1;
        if (s == from2) return to2;
        if (s == from3) return to3;
        return s;
    };
}

[[nodiscard]] inline constexpr auto create_simple_replacer(
    std::string_view from1, std::string_view to1,
    std::string_view from2, std::string_view to2,
    std::string_view from3, std::string_view to3,
    std::string_view from4, std::string_view to4)
{
    return [=](std::string_view s) {
        if (s == from1) return to1;
        if (s == from2) return to2;
        if (s == from3) return to3;
        if (s == from4) return to4;
        return s;
    };
}

struct FromTo {
    std::string_view from;
    std::string_view to;
};

template<size_t N, typename PMH>
struct PmhReplacer {
    std::array<FromTo, N> map;
    PMH pmh;

    constexpr std::string_view operator()(std::string_view input) const
    {
        auto idx = pmh.lookupIndex(input);
        assert(idx < map.size());

        if (map[idx].from == input) {
            return map[idx].to;
        }
        return input;
    }
};

template<typename Tuple, size_t... Is>
[[nodiscard]] inline constexpr auto create_pmh_replacer(Tuple&& tuple, std::index_sequence<Is...>)
{
    constexpr size_t N = sizeof...(Is);
    std::array<FromTo, N> arr{FromTo{std::get<2 * Is>(tuple), std::get<2 * Is + 1>(tuple)}...};

    auto hash = [](std::string_view s) { return fnvHash(s); };
    auto getKey = [&](size_t i) { return arr[i].from; };
    auto pmh = PerfectMinimalHash::create<N>(hash, getKey);

    return PmhReplacer<N, decltype(pmh)>{arr, pmh};
}

} // namespace detail

template<typename ...Args>
[[nodiscard]] inline constexpr auto create(Args ...args)
{
    constexpr size_t N2 = sizeof...(args);
    static_assert((N2 & 1) == 0, "Must pass an even number of strings");
    if constexpr (N2 <= 2 * 4) {
        return detail::create_simple_replacer(args...);
    } else {
        return detail::create_pmh_replacer(std::tuple(args...), std::make_index_sequence<N2 / 2>());
    }
}

} // namespace StringReplacer

#endif
