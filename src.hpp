#ifndef SJTU_DYNAMIC_BITSET_HPP
#define SJTU_DYNAMIC_BITSET_HPP

#include <vector>
#include <cstring>
#include <cstdint>
#include <iostream>
#include <algorithm>

namespace sjtu {

struct dynamic_bitset {
    using u64 = std::uint64_t;

    dynamic_bitset() = default;
    ~dynamic_bitset() = default;
    dynamic_bitset(const dynamic_bitset &) = default;
    dynamic_bitset &operator=(const dynamic_bitset &) = default;

    dynamic_bitset(std::size_t n) { nbits = n; /* lazy, do not allocate */ }

    dynamic_bitset(const std::string &str) {
        nbits = str.size();
        chunks.assign((nbits + 63) / 64, 0);
        for (std::size_t i = 0; i < nbits; ++i) if (str[i] == '1') set_bit(i);
    }

    bool operator[](std::size_t n) const {
        if (n >= nbits) return false;
        std::size_t idx = n >> 6, off = n & 63u;
        if (idx >= chunks.size()) return false;
        return (chunks[idx] >> off) & 1u;
    }

    dynamic_bitset &set(std::size_t n, bool val = true) {
        if (n >= nbits) return *this;
        std::size_t idx = n >> 6, off = n & 63u;
        ensure_capacity(nbits);
        if (val) chunks[idx] |= (u64(1) << off);
        else chunks[idx] &= ~(u64(1) << off);
        return *this;
    }

    dynamic_bitset &push_back(bool val) {
        ensure_capacity(nbits + 1);
        if (val) set_bit(nbits);
        ++nbits;
        return *this;
    }

    bool none() const {
        if (nbits == 0) return true;
        std::size_t full = nbits >> 6;
        std::size_t upto = std::min(full, chunks.size());
        for (std::size_t i = 0; i < upto; ++i) if (chunks[i]) return false;
        std::size_t rem = nbits & 63u;
        if (rem) {
            u64 mask = ((u64(1) << rem) - 1);
            if (chunks.size() > full && (chunks[full] & mask)) return false;
        }
        return true;
    }

    bool all() const {
        if (nbits == 0) return true;
        std::size_t full = nbits >> 6;
        if (chunks.size() < full) return false;
        for (std::size_t i = 0; i < full; ++i) if (chunks[i] != ~u64(0)) return false;
        std::size_t rem = nbits & 63u;
        if (rem) {
            if (chunks.size() <= full) return false;
            u64 mask = ((u64(1) << rem) - 1);
            if ((chunks[full] & mask) != mask) return false;
        }
        return true;
    }

    std::size_t size() const { return nbits; }

    dynamic_bitset &operator|=(const dynamic_bitset &o) { safe_op(o, [](u64 a,u64 b){return a|b;}); return *this; }
    dynamic_bitset &operator&=(const dynamic_bitset &o) { safe_op(o, [](u64 a,u64 b){return a&b;}); return *this; }
    dynamic_bitset &operator^=(const dynamic_bitset &o) { safe_op(o, [](u64 a,u64 b){return a^b;}); return *this; }

    dynamic_bitset &operator<<=(std::size_t n) {
        if (n==0) return *this;
        std::size_t old_n = nbits;
        std::vector<u64> old = chunks;
        if (old_n & 63u) {
            u64 mask = ((u64(1) << (old_n & 63u)) - 1);
            old[(old_n - 1) >> 6] &= mask;
        }
        nbits += n;
        ensure_capacity(nbits);
        std::fill(chunks.begin(), chunks.end(), 0);
        std::size_t word = n >> 6, rem = n & 63u;
        std::size_t old_chunks = (old_n + 63) / 64;
        for (std::size_t i = 0; i < old_chunks; ++i) {
            u64 v = old[i]; if (!v) continue;
            std::size_t dst = i + word;
            chunks[dst] |= rem ? (v << rem) : v;
            if (rem && dst + 1 < chunks.size()) chunks[dst+1] |= (v >> (64 - rem));
        }
        mask_to_size();
        return *this;
    }

    dynamic_bitset &operator>>=(std::size_t n) {
        if (n==0) return *this;
        if (n >= nbits) { nbits = 0; chunks.clear(); return *this; }
        std::size_t word = n >> 6, rem = n & 63u;
        std::vector<u64> old = chunks;
        std::size_t old_chunks = old.size();
        chunks.assign(old_chunks - word, 0);
        for (std::size_t i = 0; i + word < old_chunks; ++i) {
            u64 v = old[i + word];
            u64 res = rem ? (v >> rem) : v;
            if (rem && i + word + 1 < old_chunks) res |= (old[i + word + 1] << (64 - rem));
            chunks[i] = res;
        }
        nbits -= n;
        mask_to_size();
        return *this;
    }

    dynamic_bitset &set() {
        if (nbits==0) return *this;
        std::size_t full = nbits >> 6;
        for (std::size_t i = 0; i < full; ++i) chunks[i] = ~u64(0);
        std::size_t rem = nbits & 63u;
        if (rem) chunks[full] = ((u64(1) << rem) - 1);
        return *this;
    }

    dynamic_bitset &flip() {
        for (auto &x: chunks) x = ~x;
        mask_to_size();
        return *this;
    }

    template<class Op>
    void safe_op(const dynamic_bitset &o, Op F){
        std::size_t overlap = std::min(nbits, o.nbits);
        if (overlap==0) return;
        std::size_t full_bits = overlap & ~std::size_t(63);
        std::size_t full_chunks = full_bits >> 6;
        std::size_t a_chunks = chunks.size();
        std::size_t b_chunks = o.chunks.size();
        std::size_t work_full = std::min(full_chunks, a_chunks);
        for (std::size_t i=0;i<work_full;++i){
            u64 a = chunks[i];
            u64 b = (i<b_chunks? o.chunks[i] : 0);
            chunks[i] = F(a,b);
        }
        std::size_t rem = overlap & 63u;
        if (rem){
            std::size_t idx = full_chunks;
            if (idx < a_chunks){
                u64 mask = ((u64(1) << rem) - 1);
                u64 a = chunks[idx];
                u64 b = (idx<b_chunks? o.chunks[idx] : 0);
                u64 within = F(a,b) & mask;
                chunks[idx] = (a & ~mask) | within;
            }
        }
    }

    dynamic_bitset &reset() { std::fill(chunks.begin(), chunks.end(), 0); return *this; }

private:
    std::vector<u64> chunks;
    std::size_t nbits = 0;

    void ensure_capacity(std::size_t n) { std::size_t need = (n + 63) / 64; if (chunks.size() < need) chunks.resize(need, 0); }
    void resize_zero(std::size_t n) { nbits = n; chunks.assign((nbits + 63) / 64, 0); }
    void set_bit(std::size_t n) { std::size_t idx = n >> 6, off = n & 63u; ensure_capacity(nbits?nbits:1); chunks[idx] |= (u64(1) << off); }
    void mask_to_size() {
        if (nbits==0) return; std::size_t last = (nbits - 1) >> 6; std::size_t rem = nbits & 63u;
        if (rem) chunks[last] &= ((u64(1) << rem) - 1);
        chunks.resize((nbits + 63) / 64);
    }
    template<class Op>
    void op(const dynamic_bitset &o, Op F){
        std::size_t overlap = std::min(nbits, o.nbits);
        std::size_t full = overlap >> 6; for (std::size_t i=0;i<full;++i) chunks[i] = F(chunks[i], (i<o.chunks.size()?o.chunks[i]:0));
        std::size_t rem = overlap & 63u; if (rem){ u64 mask=((u64(1)<<rem)-1); u64 a=chunks[full]; u64 b=(full<o.chunks.size()?o.chunks[full]:0); u64 within=F(a,b)&mask; chunks[full]=(a&~mask)|within; }
    }
};

} // namespace sjtu

#endif // SJTU_DYNAMIC_BITSET_HPP
