#pragma once

#include <cstdint>
#include <type_traits>

// http://en.wikipedia.org/wiki/Endianness

#if defined(__BYTE_ORDER__)&&(__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
	#define bigEndianHost true
#endif
#if defined(__BYTE_ORDER__)&&(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
	#define bigEndianHost false
#endif

static inline uint16_t SWAP_UINT16(uint16_t x)
{
	return ((x << 8) | (x >> 8));
}

static inline uint32_t SWAP_UINT32(uint32_t x)
{
	return (((x >> 24) & 0x000000ff) | ((x <<  8) & 0x00ff0000) |
			((x >>  8) & 0x0000ff00) | ((x << 24) & 0xff000000));
}

// TODO: use SWAP_UINT16() and SWAP_UINT32() instead of following
template <typename T>
T swap(const T& arg, bool bigInMem)
{
	if (bigEndianHost == bigInMem) {
		// no byte-swapping needed
		return arg;
	} else {
		// swap bytes
		T ret;

		char* dst = reinterpret_cast<char*>(&ret);
		const char* src = reinterpret_cast<const char*>(&arg + 1);

		for (size_t i = 0; i < sizeof(T); i++) {
			*dst++ = *--src;
		}

		return ret;
	}
}

template <typename T>
class BigEndian
{
	public:
		struct IncompleteType;
		BigEndian() { }
		BigEndian(const T& t) : rep(swap(t, true)) { }
		operator T() const { return swap(rep, true); }
#if 0
		// TODO:
		constexpr operator typename std::conditional_t<sizeof(T) == 4, T, IncompleteType>() const {
			return SWAP_UINT32(rep);
		}

		constexpr BigEndian(std::conditional_t<sizeof(T) == 4, T, IncompleteType> val) {
			rep = SWAP_UINT32(val);
		}

		constexpr operator typename std::conditional_t<sizeof(T) == 2, T, IncompleteType>() const {
			return SWAP_UINT16(rep);
		}

		constexpr BigEndian(std::conditional_t<sizeof(T) == 2, T, IncompleteType> val) {
			rep = SWAP_UINT32(val);
		}
#endif
	private:
		T rep;
} __attribute__((packed));

template <typename T>
class LittleEndian
{
	public:
		T rep;
//		LittleEndian() { }
		LittleEndian(const T& t) : rep(swap(t, false)) { }
		operator T() const { return swap(rep, false); }
};

// Big endian storage types
using beint16_t = BigEndian<int16_t>;
using beint32_t = BigEndian<int32_t>;
using beuint16_t = BigEndian<uint16_t>;
using beuint32_t = BigEndian<uint32_t>;

//static_assert(sizeof(buint8_t) == 1);
static_assert(sizeof(beint16_t) == 2);
static_assert(sizeof(beint32_t) == 4);
static_assert(sizeof(beuint16_t) == 2);
static_assert(sizeof(beuint32_t) == 4);
