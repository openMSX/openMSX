#ifndef OPTIONAL_FIXED_SPAN_HH
#define OPTIONAL_FIXED_SPAN_HH

#include <optional>
#include <span>

// This class acts like std::optional<std::span<T, N>>, but only stores a single
// pointer (which can be nullptr). A plain std::span<T, N> is not sufficient
// because it cannot represent an invalid state (nullptr). This class provides
// a lightweight way to optionally hold a fixed-size span without the overhead
// of std::optional.
//
// An alternative could be to use a plain pointer (T*). Though that doesn't give
// the guarantee that it points to N elements. This class enforces that on
// construction, and retains that information on usage.
//
// In the past we tried to use std::span<T, N> initialized with
//     std::span<T, N>(static_cast<T*>(nullptr), N)
// That appeared to work, but it's actually undefined behavior. And with gcc
// debug STL mode it indeed triggers an error.
template<typename T, size_t N> class optional_fixed_span
{
public:
	// Constructor for invalid state
	constexpr optional_fixed_span() noexcept = default;

	// Constructor for valid state
	constexpr optional_fixed_span(std::span<T, N> span) noexcept
	        : ptr(span.data()) {}

	// Named function to get optional span
	[[nodiscard]] constexpr std::optional<std::span<T, N>> asOptional() const noexcept {
		if (ptr) {
			return std::span<T, N>(ptr, N);
		}
		return {};
	}

private:
	T* ptr = nullptr;
};

#endif
