#ifndef FIXED_CAPACITY_BITSET_HH
#define FIXED_CAPACITY_BITSET_HH

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>

template<size_t MAX_SIZE>
class fixed_capacity_bitset
{
	static_assert(MAX_SIZE < 256);

public:
	explicit constexpr fixed_capacity_bitset(size_t size)
	        : sz(static_cast<uint8_t>(size))
	{
		assert(size <= MAX_SIZE);
	}

	[[nodiscard]] constexpr size_t size() const noexcept
	{
		return sz;
	}

	[[nodiscard]] static constexpr size_t max_size() noexcept
	{
		return MAX_SIZE;
	}

	// Proxy reference class for writable operator[]
	class reference {
		uint8_t* ptr;
		uint8_t mask;

	public:
		constexpr reference(uint8_t* ptr_, size_t bitIdx) noexcept
		        : ptr(ptr_), mask(static_cast<uint8_t>(1U << bitIdx)) {}

		[[nodiscard]] constexpr operator bool() const noexcept {
			return (*ptr & mask) != 0;
		}

		constexpr reference& operator=(bool value) noexcept {
			if (value) {
				*ptr |= mask;
			} else {
				*ptr &= static_cast<uint8_t>(~mask);
			}
			return *this;
		}
	};

	[[nodiscard]] constexpr reference operator[](size_t pos)
	{
		assert(pos < size());
		return reference(&storage[pos / 8], pos % 8);
	}

	[[nodiscard]] constexpr bool operator[](size_t pos) const
	{
		assert(pos < size());
		return (storage[pos / 8] & (1U << (pos % 8))) != 0;
	}

private:
	static constexpr auto STORAGE_SIZE = (MAX_SIZE + 7) / 8;
	std::array<uint8_t, STORAGE_SIZE> storage{};
	uint8_t sz;
};

#endif
