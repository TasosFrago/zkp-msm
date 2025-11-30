#pragma once

#include <cassert>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <print>
#include <string>
#include <vector>

#ifndef NDEBUG
#include <source_location>

#define assertm(expr, msg, ...)                                                                       \
	do {                                                                                          \
		if(!(expr)) {                                                                         \
			const auto loc = std::source_location::current();                             \
			std::println(stderr, "Assertion failed: {}:{}", loc.file_name(), loc.line()); \
			std::println(stderr, msg __VA_OPT__(, ) __VA_ARGS__);                         \
			std::abort();                                                                 \
		}                                                                                     \
	} while(0)
#else
#define assertm(expr, msg, ...)     \
	do {                        \
		(void)sizeof(expr); \
	} while(0)
#endif

namespace bga
{

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

template <size_t Bits>
struct SelectIntType_t {
	using uint_t = decltype(SelectUIntType_fn<Bits>());
	using uintd_t = decltype(SelectUIntDoubleType_fn<Bits>());

	using int_t = decltype(SelectIntType_fn<Bits>());
	using intd_t = decltype(SelectIntDoubleType_fn<Bits>());
};

namespace SelectIntType_dbg
{
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

template <size_t Bits>
class BigInt
{
private:
	using T = typename SelectIntType_t<Bits>::uint_t;
	using T_double = typename SelectIntType_t<Bits>::uintd_t;

	// Returns vector with chuncs from LSB to MSB
	std::vector<T> radix;
	size_t chunks = 0;
	size_t bits;
	bool m_is_negative;

	std::string decimal_repr;

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

public:
	BigInt() : bits(Bits), m_is_negative(false)
	{
	}

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

	BigInt(std::string a) : bits(Bits), decimal_repr(a)
	{
		chunks = 0;
		std::vector<bool> bitArray;

		m_is_negative = (a[0] == '-');
		if(m_is_negative) a.erase(0, 1);

		if(false) {

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
			chunks++;
			assertm(chunks == radix.size(), "chunks: {}, size: {}", chunks, radix.size());
		}
	}

	void push_bits(T num)
	{
		radix.push_back(num);
		chunks++;
		assertm(chunks == radix.size(), "chunks: {}, size: {}", chunks, radix.size());
	}

	using iterator = std::vector<T>::iterator;
	using const_iterator = std::vector<T>::const_iterator;
	iterator begin()
	{
		return radix.begin();
	}
	iterator end()
	{
		return radix.end();
	}

	template <typename InputIt>
	iterator insert(const_iterator pos, InputIt first, InputIt last)
	{
		auto res = radix.insert(pos, first, last);
		chunks = radix.size();
		return res;
	}

	void push_bits(T num, size_t idx)
	{
		radix[idx] = num;
	}

	void reserve(size_t n)
	{
		radix.reserve(n);
	}

	void resize(size_t n, size_t num)
	{
		radix.resize(n, num);
		chunks += n;
		assertm(chunks == radix.size(), "chunks: {}, size: {}", chunks, radix.size());
	}

	std::vector<T> &get()
	{
		return radix;
	}

	std::vector<T> get() const
	{
		return radix;
	}

	T get(size_t idx) const
	{
		return radix[idx];
	}

	std::vector<T> get_reversed() const
	{
		std::vector<T> tmp;
		for(auto it = radix.rbegin(); it != radix.rend(); ++it) {
			tmp.push_back(*it);
		}
		return tmp;
	}

	size_t get_bits() const
	{
		return bits;
	}
	size_t get_chunks() const
	{
		return chunks;
	}
	bool is_negative() const
	{
		return m_is_negative;
	}

	void set_sign(bool is_negative)
	{
		m_is_negative = is_negative;
	}

	bool is_zero() const
	{
		if(chunks == 0) return true;
		for(const auto el : radix) {
			if(el != 0) return false;
		}
		assert(!"Unreachable");
	}

	size_t effective_size() const
	{
		size_t s = chunks;
		while(s > 0 && radix[s - 1] == 0) {
			s--;
		}
		return s;
	}

	bool operator==(const BigInt<Bits> &other) const
	{
		size_t a_chunks = this->effective_size();
		size_t b_chunks = other.effective_size();

		if(a_chunks == 0 && b_chunks) return true;

		if(m_is_negative != other.is_negative()) return false;
		if(a_chunks != b_chunks) return false;

		for(size_t i = 0; i < a_chunks; i++) {
			if(radix[i] != other.get(i)) {
				return false;
			}
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

	void print_all()
	{
		std::println("Dec:\t{}", (*this));
		std::println("Bin:\t{:b}", (*this));
		std::println("Hex:\t{:x}", (*this));
		std::println("MSB:\t{:msb}", (*this));
		std::println("LSB:\t{:lsb}", (*this));
	}
	void print_all(const char *name)
	{
		std::println("Num {}:", name);
		std::println("\tDec:\t{}", (*this));
		std::println("\tBin:\t{:b}", (*this));
		std::println("\tHex:\t{:x}", (*this));
		std::println("\tMSB:\t{:msb}", (*this));
		std::println("\tLSB:\t{:lsb}", (*this));
	}
};

template <size_t Bits>
void pad_BigInts(BigInt<Bits> &a, BigInt<Bits> &b)
{
	if(a.get_chunks() == b.get_chunks()) {
		return;
	}
	BigInt<Bits> &needsPad = (a.get_chunks() > b.get_chunks()) ? b : a;
	uint32_t padding = (a.get_chunks() > b.get_chunks()) ? (a.get_chunks() - b.get_chunks()) : (b.get_chunks() - a.get_chunks());

	for(uint32_t i = 0; i < padding; i++) {
		needsPad.push_bits(0);
	}
}

} // namespace bga

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

	struct PowerOf10 {
		static constexpr T value = []() {
			if constexpr(sizeof(T) >= 8)
				return 10000000000000000000ULL;
			else if constexpr(sizeof(T) >= 4)
				return 1000000000UL;
			else if constexpr(sizeof(T) >= 2)
				return 10000U;
			else
				return 100U;
		}();

		static constexpr int digits = []() {
			if constexpr(sizeof(T) >= 8)
				return 19;
			else if constexpr(sizeof(T) >= 4)
				return 9;
			else if constexpr(sizeof(T) >= 2)
				return 4;
			else
				return 2;
		}();
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

	// Helper: Checks if a vector<T> is all zero
	static bool isVecZero(const std::vector<T> &num)
	{
		for(T chunk : num) {
			if(chunk != 0) return false;
		}
		return true;
	}

public:
	// --- 1. The 'parse' method ---
	// Reads the format string (e.g., ":b")
	constexpr auto parse(std::format_parse_context &ctx)
	{
		auto it = ctx.begin();
		auto end = ctx.end();

		if(it != end && *it == ':') {
			++it;
		}

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

		if(isVecZero(num.get())) {
			return std::format_to(out, "0");
		}

		const auto &num_radix = num.get();
		const size_t chunk_bits = num.get_bits();
		const bool is_negative = num.is_negative();

		switch(format_spec) {

		// --- Decimal Logic ---
		case Format::DECIMAL: {
			std::vector<T> tempRadix = num_radix;

			// size_t max_digits = (num_radix.size() * chunk_bits * 30103) / 100000 + 2;

			// std::string result;
			// result.reserve(max_digits);

			std::vector<T> parts;
			parts.reserve(num_radix.size());

			while(!isVecZero(tempRadix)) {
				// T part = divideVecByBase(tempRadix, chunk_bits, PowerOf10::value);
				parts.push_back(divideVecByBase(tempRadix, chunk_bits, PowerOf10::value));

				// char buff[PowerOf10::digits];
				// for(int i = PowerOf10::digits - 1; i >= 0; --i) {
				// 	buff[i] = '0' + (part % 10);
				// 	part /= 10;
				// }
				// result.append(buff, PowerOf10::digits);
			}

			// size_t first_nonzero = result.find_first_not_of('0');
			// if(first_nonzero != std::string::npos) {
			// 	result.erase(0, first_nonzero);
			// } else {
			// 	result = "0";
			// }

			if(is_negative) {
				// result.insert(0, "-");
				out = std::format_to(out, "-");
			}

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
				// out = std::vformat_to(out, std::string_view(fmt_str), std::make_format_args(*it));
			}
			// std::reverse(result.begin() + (is_negative ? 1 : 0), result.end());
			// return std::format_to(out, "{}", result);
			return out;
		}

		// --- Binary Logic ---
		case Format::BINARY: {
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
			out = std::format_to(out, "0x");
			bool leadingZeros = true;

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
			if(leadingZeros) {
				out = std::format_to(out, "0");
			}
			return out;
		}

		// --- Chunks (LSB to MSB) ---
		case Format::CHUNKS_LSB: {
			out = std::format_to(out, "[LSB] ");
			for(const T &chunk : num_radix) {
				// Print each chunk as binary with leading zeros
				for(int b = chunk_bits - 1; b >= 0; b--) {
					bool bit = (chunk >> b) & 1;
					out = std::format_to(out, "{}", (bit ? '1' : '0'));
				}
				out = std::format_to(out, " ");
			}
			return std::format_to(out, "[MSB]");
		}

		// --- Chunks (MSB to LSB) ---
		case Format::CHUNKS_MSB: {
			out = std::format_to(out, "[MSB] ");
			for(auto it = num_radix.rbegin(); it != num_radix.rend(); ++it) {
				// Print each chunk as binary with leading zeros
				for(int b = chunk_bits - 1; b >= 0; b--) {
					bool bit = (*it >> b) & 1;
					out = std::format_to(out, "{}", (bit ? '1' : '0'));
				}
				out = std::format_to(out, " ");
			}
			return std::format_to(out, "[LSB]");
		}
		}

		return out;
	}
};
