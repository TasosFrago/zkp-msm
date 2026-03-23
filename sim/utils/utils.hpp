#pragma once

#include <cstddef>

#include <verilated.h>

#include "big_arth.hpp"

namespace vtb
{

template <size_t Bits, size_t Words>
bga::BigInt<Bits> to_bgint(const VlWide<Words> &in)
{
	constexpr size_t vl_bits_s = sizeof(decltype(in.m_storage[0])) * 8;
	constexpr size_t vl_total = vl_bits_s * Words;
	constexpr size_t bg_chunks = (vl_total + Bits - 1) / Bits;

	bga::BigInt<Bits> res;

	using InType = decltype(in.m_storage[0]);
	using OutType = decltype(res.get_safe(0));

	res.reserve(bg_chunks);

	OutType current_chunk = 0;
	size_t bits_in_chunk = 0;

	for(size_t i = 0; i < in.size(); i++) {
		InType w = in.at(i);
		size_t bits_left_in_w = vl_bits_s;

		while(bits_left_in_w > 0) {
			size_t bits_needed = Bits - bits_in_chunk;
			size_t take = std::min(bits_left_in_w, bits_needed);

			size_t shift_w = vl_bits_s - bits_left_in_w;

			OutType piece = static_cast<OutType>(w >> shift_w);
			if(take < (sizeof(OutType) * 8)) {
				OutType mask = (static_cast<OutType>(1) << take) - 1;
				piece &= mask;
			}

			current_chunk |= (piece << bits_in_chunk);

			bits_in_chunk += take;
			bits_left_in_w -= take;

			if(bits_in_chunk == Bits) {
				res.push_bits(current_chunk);
				current_chunk = 0;
				bits_in_chunk = 0;
			}
		}
	}
	if(bits_in_chunk > 0) {
		res.push_bits(current_chunk);
	}
	return res;
}

template <size_t Bits, size_t Words>
void to_vlwide(const bga::BigInt<Bits> &in, VlWide<Words> &out)
{
	using InType = std::remove_reference_t<decltype(in.get_safe(0))>;
	using OutType = std::remove_reference_t<decltype(out.m_storage[0])>;

	constexpr size_t out_bits_s = sizeof(OutType) * 8;

	const size_t max_in_chunks = in.get_chunks();

	OutType current_word = 0;
	size_t bits_in_word = 0;
	size_t word_index = 0;

	for(size_t i = 0; i < max_in_chunks; i++) {
		InType chunk = in.get_safe(i);
		size_t bits_left_in_chunk = Bits;

		while(bits_left_in_chunk > 0 && word_index < Words) {
			size_t bits_needed = out_bits_s - bits_in_word;
			size_t take = std::min(bits_left_in_chunk, bits_needed);

			InType piece = chunk;
			if(take < (sizeof(InType) * 8)) {
				InType mask = (static_cast<InType>(1) << take) - 1;
				piece &= mask;
			}

			current_word |= (static_cast<OutType>(piece) << bits_in_word);

			bits_in_word += take;
			bits_left_in_chunk -= take;

			if(bits_left_in_chunk > 0) {
				chunk >>= take;
			}

			if(bits_in_word == out_bits_s) {
				out.m_storage[word_index++] = current_word;
				current_word = 0;
				bits_in_word = 0;
			}
		}
	}

	if(bits_in_word > 0 && word_index < Words) {
		out.m_storage[word_index++] = current_word;
	}

	while(word_index < Words) {
		out.m_storage[word_index++] = 0;
	}
}

}; // namespace vtb
