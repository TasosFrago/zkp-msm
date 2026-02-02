#pragma once

/**
 * @file bigint.hpp
 * @brief A template-based arbitrary-precision integer library with configurable radix.
 */

#include <cassert>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <print>
#include <string>
#include <vector>

#ifndef NDEBUG
#include <source_location>

/**
 * @brief Custom assertion macro providing source location and formatted messages.
 * * Only active when NDEBUG is not defined.
 * @param expr The expression to evaluate.
 * @param msg The format string for the error message.
 * @param ... Additional arguments for the format string.
 */
#define assertm(expr, msg, ...)                                                                       \
	do {                                                                                          \
		if(!(expr)) {                                                                         \
			const auto loc = std::source_location::current();                             \
			std::println(stderr, "\033[?25h");                                            \
			std::println(stderr, "Assertion failed: {}:{}", loc.file_name(), loc.line()); \
			std::println(stderr, msg __VA_OPT__(, ) __VA_ARGS__);                         \
			std::println("fun: {}", __PRETTY_FUNCTION__);                                 \
			std::abort();                                                                 \
		}                                                                                     \
	} while(0)
#else
#define assertm(expr, msg, ...)     \
	do {                        \
		(void)sizeof(expr); \
	} while(0)
#endif

/**
 * @namespace bga
 * @brief BigInt General Architecture - Contains the core BigInt implementation and type traits.
 */
namespace bga
{

/**
 * @brief Internal macro to define function templates for selecting integer types.
 * @internal
 */
#define SelectIntType(name, type8, type16, type32, type64)                              \
	template <size_t Bits>                                                          \
	constexpr auto name()                                                           \
	{                                                                               \
		static_assert(Bits > 0 && Bits <= 64, "Bits musth be between [1, 64]"); \
		if constexpr(Bits <= 8) {                                               \
			return type8{};                                                 \
		} else if constexpr(Bits <= 16) {                                       \
			return type16{};                                                \
		} else if constexpr(Bits <= 32) {                                       \
			return type32{};                                                \
		} else {                                                                \
			return type64{};                                                \
		}                                                                       \
	}

SelectIntType(SelectUIntType_fn, uint8_t, uint16_t, uint32_t, uint64_t);
SelectIntType(SelectUIntDoubleType_fn, uint16_t, uint32_t, uint64_t, __uint128_t);
SelectIntType(SelectIntType_fn, int8_t, int16_t, int32_t, int64_t);
SelectIntType(SelectIntDoubleType_fn, int16_t, int32_t, int64_t, __int128_t);

/**
 * @struct SelectIntType_t
 * @brief Compile-time trait to determine the optimal underlying types for a given bit width.
 * @tparam Bits The number of bits requested per chunk.
 */
template <size_t Bits>
struct SelectIntType_t {
	using uint_t = decltype(SelectUIntType_fn<Bits>());	   ///< Unsigned storage type.
	using uintd_t = decltype(SelectUIntDoubleType_fn<Bits>()); ///< Unsigned type for intermediate double-width calculations.

	using int_t = decltype(SelectIntType_fn<Bits>());	 ///< Signed storage type.
	using intd_t = decltype(SelectIntDoubleType_fn<Bits>()); ///< Signed type for intermediate double-width calculations.
};

/**
 * @namespace SelectIntType_dbg
 * @brief Utilities for debugging the type selection system.
 */
namespace SelectIntType_dbg
{
/**
 * @brief Returns a string representation of a type name at compile time.
 * @tparam T The type to inspect.
 */
template <typename T>
constexpr std::string_view type_name()
{
#if defined(__clang__)
	constexpr std::string_view prefix = "T = ";
	constexpr std::string_view suffix = "]";
	constexpr std::string_view function = __PRETTY_FUNCTION__;
#elif defined(__GNUC__)
	constexpr std::string_view prefix = "with T = ";
	constexpr std::string_view suffix = ";";
	constexpr std::string_view function = __PRETTY_FUNCTION__;
#elif defined(_MSC_VER)
	constexpr std::string_view prefix = "type_name<";
	constexpr std::string_view suffix = ">(void)";
	constexpr std::string_view function = __FUNCSIG__;
#else
#error "Unsupported compiler"
#endif

	const auto start = function.find(prefix) + prefix.size();
	const auto end = function.rfind(suffix);

	return function.substr(start, end - start);
}

/**
 * @brief Recursive template to print all selected types for bit widths doubling from 1 to 64.
 */
template <size_t Bits>
void debug_loop()
{
	std::println("Bits: {:02}\tu: {},\tud: {},\ti: {:11},\tid: {}",
		     Bits,
		     type_name<typename SelectIntType_t<Bits>::uint_t>(),
		     type_name<typename SelectIntType_t<Bits>::uintd_t>(),
		     type_name<typename SelectIntType_t<Bits>::int_t>(),
		     type_name<typename SelectIntType_t<Bits>::intd_t>());

	if constexpr(Bits < 64) {
		debug_loop<Bits * 2>();
	}
}
}; // namespace SelectIntType_dbg

/**
 * @class BigInt
 * @brief An arbitrary-precision integer class with a configurable radix (bit width per chunk).
 * * The class stores data in a Little-Endian format where radix[0] is the least significant chunk.
 * * @tparam Bits The number of bits per internal "digit" or chunk. Valid range [1, 64].
 */
template <size_t Bits>
class BigInt
{
private:
	using T = typename SelectIntType_t<Bits>::uint_t;
	using T_double = typename SelectIntType_t<Bits>::uintd_t;

	std::vector<T> radix; ///< Internal storage of integer chunks.
	size_t bits = Bits;   ///< The bit width of each chunk (constant Bits).
	bool m_is_negative;   ///< Flag indicating if the number is negative.

	/**
	 * @brief Divides a decimal string by 2 and returns the remainder.
	 * Used primarily for string-to-binary conversion.
	 * @param num The decimal string to be divided (modified in place).
	 * @return The remainder (0 or 1).
	 */
	static int devideStrBy2(std::string &num)
	{
		int remainder = 0;
		for(char &c : num) {
			int digit = c - '0';
			int val = digit + remainder * 10;
			c = (val / 2) + '0';
			remainder = val % 2;
		}
		while(num.length() > 1 && num[0] == '0') {
			num.erase(0, 1);
		}
		return remainder;
	}

	/**
	 * @brief Converts a hex character to its integer value.
	 * @param c The hex character ('0'-'9', 'A'-'F', 'a'-'f').
	 * @return The 8-bit integer value.
	 */
	constexpr static uint8_t hexCharToInt(char c)
	{
		if(c >= '0' && c <= '9') return c - '0';
		if(c >= 'A' && c <= 'F') return c - 'A' + 10;
		if(c >= 'a' && c <= 'f') return c - 'a' + 10;
		assertm(false, "Invalid argument given to function");
		return 0;
	}

public:
	/**
	 * @brief Default constructor. Initializes the value to 0.
	 */
	BigInt()
	    : bits(Bits), m_is_negative(false) {};

	/**
	 * @brief Constructs a BigInt from a 128-bit integer.
	 * @param n The source 128-bit value.
	 */
	BigInt(__int128_t n) : bits(Bits)
	{
		uint64_t value;
		if(n < 0) {
			m_is_negative = true;
			value = 0ULL - static_cast<uint64_t>(n);
		} else {
			m_is_negative = false;
			value = static_cast<uint64_t>(n);
		}

		T_double mask = (static_cast<T_double>(1) << Bits) - 1;

		do {
			this->push_bits(value & mask);
			if constexpr(Bits == 64) {
				value = 0;
			} else {
				value >>= bits;
			}
		} while(value != 0);
	}

	BigInt(const char *s) : BigInt(std::string(s)) {};

	template <std::integral T>
	BigInt(T n) : BigInt(static_cast<__int128_t>(n)){};

	BigInt(std::nullptr_t) = delete;

	/**
	 * @brief Constructs a BigInt from a string (Decimal or Hex).
	 * Supports "0x" prefix for hexadecimal values.
	 * @param a The string representation of the number.
	 */
	BigInt(std::string a) : bits(Bits)
	{
		std::vector<bool> bitArray;

		m_is_negative = (a[0] == '-');
		if(m_is_negative) a.erase(0, 1); // remove sign

		if(a.size() >= 2 && a[0] == '0' && a[1] == 'x') {
			a.erase(0, 2); // remove the 0x prefix

			for(int64_t i = a.size() - 1; i >= 0; i--) {
				uint8_t val = hexCharToInt(a[i]);

				bitArray.push_back(val & 1);
				bitArray.push_back((val >> 1) & 1);
				bitArray.push_back((val >> 2) & 1);
				bitArray.push_back((val >> 3) & 1);
			}

		} else {
			std::string tmp = a;
			while(tmp != "0") {
				bitArray.push_back(devideStrBy2(tmp));
			}
		}

		int remainder = bitArray.size() % bits;
		if(remainder != 0) {
			int padding = bits - remainder;
			for(int i = 0; i < padding; i++) {
				bitArray.push_back(false);
			}
		}

		for(size_t i = 0; i < bitArray.size(); i += bits) {
			T chunkValue = 0;

			for(size_t b = 0; b < bits; b++) {
				if((i + b < bitArray.size()) && (bitArray[i + b])) {
					chunkValue |= (static_cast<T>(1) << b);
				}
			}
			radix.push_back(chunkValue);
		}
	}

	/**
	 * @brief Move constructor.
	 */
	BigInt(BigInt &&other) noexcept
	    : radix(std::move(other.radix)),
	      bits(other.bits),
	      m_is_negative(other.m_is_negative) {};

	/**
	 * @brief Move assignment.
	 */
	BigInt &operator=(BigInt &&other) noexcept
	{
		if(this != &other) {
			this->radix = std::move(other.radix);
			this->m_is_negative = other.m_is_negative;
			this->bits = other.bits;
		}
		return *this;
	}

	/**
	 * @brief Copy constructor.
	 */
	BigInt(const BigInt &other) noexcept
	    : radix(other.radix),
	      bits(other.bits),
	      m_is_negative(other.m_is_negative) {};

	/**
	 * @brief Copy assignment.
	 */
	BigInt &operator=(const BigInt &other) noexcept
	{
		if(this != &other) {
			this->radix = other.radix;
			this->m_is_negative = other.m_is_negative;
			this->bits = other.bits;
		}
		return *this;
	}

	/** @name Data Access and Iterators */
	///@{
	using iterator = std::vector<T>::iterator;
	using const_iterator = std::vector<T>::const_iterator;
	using reverse_iterator = std::vector<T>::reverse_iterator;
	using const_reverse_iterator = std::vector<T>::const_reverse_iterator;

	iterator begin()
	{
		return radix.begin();
	}
	iterator end()
	{
		return radix.end();
	}

	reverse_iterator rbegin()
	{
		return radix.rbegin();
	}
	reverse_iterator rend()
	{
		return radix.rend();
	}

	const_iterator begin() const
	{
		return radix.begin();
	}
	const_iterator end() const
	{
		return radix.end();
	}

	const_reverse_iterator rbegin() const
	{
		return radix.rbegin();
	}
	const_reverse_iterator rend() const
	{
		return radix.rend();
	}

	/** @brief Appends a new chunk to the most significant position. */
	void push_bits(T num)
	{
		radix.push_back(num);
	}

	/** @brief Inserts chunks into the internal storage. */
	template <typename InputIt>
	iterator insert(const_iterator pos, InputIt first, InputIt last)
	{
		return radix.insert(pos, first, last);
	}

	/** @brief Sets the value of a specific chunk. */
	void push_bits(T num, size_t idx)
	{
		radix[idx] = num;
	}

	/** @brief Reserves memory for the internal chunk vector. */
	void reserve(size_t n)
	{
		radix.reserve(n);
	}

	/** @brief Resizes the internal chunk vector. */
	void resize(size_t n)
	{
		radix.resize(n);
	}

	/** @brief Resizes the internal chunk vector and fills with a value. */
	void resize(size_t n, size_t num)
	{
		radix.resize(n, num);
	}

	/** @brief Returns a reference to the internal vector. */
	std::vector<T> &get()
	{
		return radix;
	}

	/** @brief Returns a copy of the internal vector. */
	std::vector<T> get() const
	{
		return radix;
	}

	/** @brief Gets a chunk at idx with assertion checking. */
	T get(size_t idx) const
	{
		assertm(idx < radix.size(), "Index out of bounds. idx: {}", idx);
		return radix[idx];
	}

	/** @brief Gets a mutable chunk at idx with assertion checking. */
	T &get(size_t idx)
	{
		assertm(idx < radix.size(), "Index out of bounds. idx: {}", idx);
		return radix[idx];
	}

	/** @brief Gets a chunk safely; returns 0 if out of bounds. */
	T get_safe(size_t idx) const
	{
		return (idx < radix.size()) ? radix[idx] : 0;
	}

	/** @brief Returns a vector of chunks from MSB to LSB. */
	std::vector<T> get_reversed() const
	{
		std::vector<T> tmp;
		for(auto it = radix.rbegin(); it != radix.rend(); ++it) {
			tmp.push_back(*it);
		}
		return tmp;
	}
	///@}

	/** @name Metadata and Sign */
	///@{
	size_t get_bits() const
	{
		return bits;
	}
	size_t get_chunks() const
	{
		return radix.size();
	}
	bool is_negative() const
	{
		return m_is_negative;
	}
	void set_sign(bool is_negative)
	{
		m_is_negative = is_negative;
	}
	///@}

	/**
	 * @brief Checks if the number is numerically zero.
	 */
	bool is_zero() const
	{
		if(radix.size() == 0) return true;
		for(const auto el : radix) {
			if(el != 0) return false;
		}
		return true;
	}

	/**
	 * @brief Trims leading zero chunks from the internal storage.
	 */
	void trim()
	{
		while(radix.size() > 1 && radix.back() == 0) {
			radix.pop_back();
		}
	}

	/**
	 * @brief Returns the number of chunks required to represent the number (ignoring leading zeros).
	 */
	size_t effective_size() const
	{
		size_t s = radix.size();
		while(s > 0 && radix[s - 1] == 0) {
			s--;
		}
		return s;
	}

	/** @name Operators */
	///@{
	bool operator==(const BigInt<Bits> &other) const
	{
		size_t a_chunks = this->effective_size();
		size_t b_chunks = other.effective_size();

		if(a_chunks != b_chunks) return false;
		if(a_chunks == 0) return true;
		if(m_is_negative != other.is_negative()) return false;

		for(size_t i = 0; i < a_chunks; i++) {
			if(radix[i] != other.get(i)) return false;
		}
		return true;
	}

	std::strong_ordering operator<=>(const BigInt<Bits> &other) const
	{
		size_t a_chunks = this->effective_size();
		size_t b_chunks = other.effective_size();

		if(a_chunks == 0 && b_chunks == 0) return std::strong_ordering::equal;

		if(m_is_negative != other.is_negative()) {
			return m_is_negative ? std::strong_ordering::less : std::strong_ordering::greater;
		}

		std::strong_ordering res = std::strong_ordering::equal;

		if(a_chunks != b_chunks) {
			res = (a_chunks <=> b_chunks);
		} else {
			for(int64_t i = a_chunks - 1; i >= 0; i--) {
				if(radix[i] != other.get(i)) {
					res = (radix[i] <=> other.get(i));
					break;
				}
			}
		}

		return m_is_negative ? (0 <=> res) : res;
	}
	///@}

	/** @name DEBUG Printing Utilities */
	///@{
	__attribute__((noinline, used)) void printme()
	{
		std::println("{}", (*this));
	}
	__attribute__((noinline, used)) std::string str()
	{
		return std::format("{}", (*this));
	}
	__attribute__((noinline, used)) void print_all();
	void print_all(const char *name);
	///@}
};

/**
 * @brief Utility alias for BigInt.
 */
template <size_t Bits>
using bgint = BigInt<Bits>;

/**
 * @brief Pads two BigInts with leading zero chunks so they have the same size.
 */
template <size_t Bits>
void pad_BigInts(BigInt<Bits> &a, BigInt<Bits> &b)
{
	if(a.get_chunks() == b.get_chunks()) return;

	BigInt<Bits> &needsPad = (a.get_chunks() > b.get_chunks()) ? b : a;
	uint32_t padding = (a.get_chunks() > b.get_chunks()) ? (a.get_chunks() - b.get_chunks()) : (b.get_chunks() - a.get_chunks());

	for(uint32_t i = 0; i < padding; i++) {
		needsPad.push_bits(0);
	}
}

} // namespace bga

/**
 * @brief Custom std::formatter for BigInt.
 * * Supports:
 * - `{}`: Decimal
 * - `{:b}`: Binary
 * - `{:x}`: Hexadecimal
 * - `{:lsb}`: Binary chunks from LSB to MSB
 * - `{:msb}`: Binary chunks from MSB to LSB
 */
template <size_t Bits>
struct std::formatter<bga::BigInt<Bits>> {
private:
	using T = typename bga::SelectIntType_t<Bits>::uint_t;
	using T_double = typename bga::SelectIntType_t<Bits>::uintd_t;

	enum class Format { DECIMAL,
			    BINARY,
			    HEX,
			    CHUNKS_LSB,
			    CHUNKS_MSB };
	Format format_spec = Format::DECIMAL;

	/**
	 * @brief Precomputed power of 10 to speed up decimal conversion.
	 */
	struct PowerOf10 {
		static constexpr auto data = []() consteval {
			T val = 1;
			int digits = 0;
			T max = std::numeric_limits<T>::max();

			while(val <= max / 10) {
				val *= 10;
				digits++;
			}
			return std::pair{ val, digits };
		}();
		static constexpr T value = data.first;
		static constexpr T digits = data.second;
	};

	static T divideVecByBase(std::vector<T> &num, size_t bits, T divisor)
	{
		T_double remainder = 0;

		for(auto it = num.rbegin(); it != num.rend(); ++it) {
			T_double val = (remainder << bits) | (*it);

			*it = static_cast<T>(val / divisor);
			remainder = val % divisor;
		}

		return static_cast<T>(remainder);
	}

	// Helper: Divides a radix vector by 10, returns remainder (0-9)
	static T divideVecBy10(std::vector<T> &num, size_t bits)
	{
		T_double remainder = 0;
		for(auto it = num.rbegin(); it != num.rend(); ++it) {
			T_double val = (remainder << bits) + (*it);
			*it = static_cast<T>(val / 10);
			remainder = val % 10;
		}
		return static_cast<T>(remainder);
	}

	static bool isVecZero(const std::vector<T> &num)
	{
		for(T chunk : num)
			if(chunk != 0) return false;
		return true;
	}

public:
	// --- 1. The 'parse' method ---
	// Reads the format string (e.g., ":b")
	constexpr auto parse(std::format_parse_context &ctx)
	{
		auto it = ctx.begin();
		auto end = ctx.end();

		if(it != end && *it == ':') ++it;
		if(it != end && *it != '}') {
			// Check the first character after ':'
			if(*it == 'b') {
				format_spec = Format::BINARY;
				++it;
			} else if(*it == 'x') {
				format_spec = Format::HEX;
				++it;
			} else if(*it == 'l' && (it + 1) != end && *(it + 1) == 's' && (it + 2) != end && *(it + 2) == 'b') {
				format_spec = Format::CHUNKS_LSB;
				it += 3;
			} else if(*it == 'm' && (it + 1) != end && *(it + 1) == 's' && (it + 2) != end && *(it + 2) == 'b') {
				format_spec = Format::CHUNKS_MSB;
				it += 3;
			} else {
				throw std::format_error("Invalid format specifier for BigInt");
			}
		}

		// Must point to '}' or end
		if(it != end && *it != '}') {
			throw std::format_error("Invalid format specifier for BigInt");
		}

		return it;
	}

	// --- 2. The 'format' method ---
	auto format(const bga::BigInt<Bits> &num, std::format_context &ctx) const
	{
		auto out = ctx.out();
		if(num.is_zero()) return std::format_to(out, "0");

		const auto &num_radix = num.get();
		const size_t chunk_bits = num.get_bits();
		const bool is_negative = num.is_negative();

		switch(format_spec) {

		// --- Decimal Logic ---
		case Format::DECIMAL: {
			std::vector<T> tempRadix = num_radix;
			std::vector<T> parts;
			parts.reserve(num_radix.size());

			while(!isVecZero(tempRadix)) {
				parts.push_back(divideVecByBase(tempRadix, chunk_bits, PowerOf10::value));
			}
			if(is_negative) out = std::format_to(out, "-");
			auto it = parts.rbegin();
			out = std::format_to(out, "{}", *it);
			++it;

			std::string fmt_str = "{:0" + std::to_string(PowerOf10::digits) + "}";

			for(; it != parts.rend(); ++it) {
				T val = *it;
				char buff[PowerOf10::digits + 1];
				for(int i = PowerOf10::digits - 1; i >= 0; --i) {
					buff[i] = '0' + (val % 10);
					val /= 10;
				}
				buff[PowerOf10::digits] = '\0';
				out = std::format_to(out, "{}", buff);
			}
			return out;
		}

		// --- Binary Logic ---
		case Format::BINARY: {
			if(is_negative) out = std::format_to(out, "-");

			bool leadingZeros = true;
			for(auto it = num_radix.rbegin(); it != num_radix.rend(); ++it) {
				T chunk = *it;
				for(int b = chunk_bits - 1; b >= 0; b--) {
					bool bit = (chunk >> b) & 1;
					if(!leadingZeros || bit || chunk_bits == 1) {
						out = std::format_to(out, "{}", (bit ? '1' : '0'));
						leadingZeros = false;
					}
				}
			}
			return out;
		}

		// --- Hex Logic (MSB to LSB) ---
		case Format::HEX: {
			if(is_negative) out = std::format_to(out, "-");

			out = std::format_to(out, "0x");
			bool leadingZeros = true;

			// FIX: When printing values of Bits lower than 4bits i.e a hex digit it padds the value with leading zeros and the value printed is wrong. Solution detect that and combine the values.
			for(auto it = num_radix.rbegin(); it != num_radix.rend(); ++it) {
				T chunk = *it;
				size_t hex_digits = (chunk_bits + 3) / 4;

				for(int h = hex_digits - 1; h >= 0; h--) {
					int nibble = (chunk >> (h * 4)) & 0xF;
					if(!leadingZeros || nibble != 0) {
						out = std::format_to(out, "{:x}", nibble);
						leadingZeros = false;
					}
				}
			}
			if(leadingZeros) out = std::format_to(out, "0");
			return out;
		}

		// --- Chunks (LSB to MSB) ---
		case Format::CHUNKS_LSB: {
			out = std::format_to(out, "[LSB] ");
			if(is_negative) out = std::format_to(out, "-");
			for(const T &chunk : num_radix) {
				for(int b = chunk_bits - 1; b >= 0; b--) {
					out = std::format_to(out, "{}", ((chunk >> b) & 1 ? '1' : '0'));
				}
				out = std::format_to(out, " ");
			}
			return std::format_to(out, "[MSB]");
		}

		// --- Chunks (MSB to LSB) ---
		case Format::CHUNKS_MSB: {
			out = std::format_to(out, "[MSB] ");
			if(is_negative) out = std::format_to(out, "-");
			for(auto it = num_radix.rbegin(); it != num_radix.rend(); ++it) {
				// Print each chunk as binary with leading zeros
				for(int b = chunk_bits - 1; b >= 0; b--) {
					out = std::format_to(out, "{}", ((*it >> b) & 1 ? '1' : '0'));
				}
				out = std::format_to(out, " ");
			}
			return std::format_to(out, "[LSB]");
		}
		}

		return out;
	}
};

/** @internal Implementation of non-inline printing functions */
template <size_t Bits>
void bga::BigInt<Bits>::print_all()
{
	std::println("Dec:\t{}", (*this));
	std::println("Bin:\t{:b}", (*this));
	std::println("Hex:\t{:x}", (*this));
	std::println("MSB:\t{:msb}", (*this));
	std::println("LSB:\t{:lsb}", (*this));
}

template <size_t Bits>
void bga::BigInt<Bits>::print_all(const char *name)
{
	std::println("Num {}:", name);
	std::println("\tDec:\t{}", (*this));
	std::println("\tBin:\t{:b}", (*this));
	std::println("\tHex:\t{:x}", (*this));
	std::println("\tMSB:\t{:msb}", (*this));
	std::println("\tLSB:\t{:lsb}", (*this));
}
