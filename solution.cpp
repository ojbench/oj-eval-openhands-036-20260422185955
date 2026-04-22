#include <vector>
#include <cstring>
#include <cstdint>
#include <iostream>
#include <algorithm>

struct dynamic_bitset {
    using u64 = std::uint64_t;

    dynamic_bitset() = default;
    ~dynamic_bitset() = default;
    dynamic_bitset(const dynamic_bitset &) = default;
    dynamic_bitset &operator=(const dynamic_bitset &) = default;

    dynamic_bitset(std::size_t n) { resize_zero(n); }

    dynamic_bitset(const std::string &str) {
        nbits = str.size();
        chunks.assign((nbits + 63) / 64, 0);
        for (std::size_t i = 0; i < nbits; ++i) {
            if (str[i] == '1') set_bit(i);
        }
    }

    bool operator[](std::size_t n) const {
        if (n >= nbits) return false;
        std::size_t idx = n >> 6;
        std::size_t off = n & 63u;
        return (chunks[idx] >> off) & 1u;
    }

    dynamic_bitset &set(std::size_t n, bool val = true) {
        if (n >= nbits) return *this;
        if (val) set_bit(n); else clear_bit(n);
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
        for (std::size_t i = 0; i < full; ++i) if (chunks[i] != 0) return false;
        std::size_t rem = nbits & 63u;
        if (rem) {
            u64 mask = rem == 64 ? ~u64(0) : ((u64(1) << rem) - 1);
            if ((chunks[full] & mask) != 0) return false;
        }
        return true;
    }

    bool all() const {
        if (nbits == 0) return true;
        std::size_t full = nbits >> 6;
        for (std::size_t i = 0; i < full; ++i) if (chunks[i] != ~u64(0)) return false;
        std::size_t rem = nbits & 63u;
        if (rem) {
            u64 mask = (rem == 64) ? ~u64(0) : ((u64(1) << rem) - 1);
            if ((chunks[full] & mask) != mask) return false;
        }
        return true;
    }

    std::size_t size() const { return nbits; }

    dynamic_bitset &operator|=(const dynamic_bitset &other) {
        operate_inplace(other, [](u64 a, u64 b){ return a | b; });
        return *this;
    }

    dynamic_bitset &operator&=(const dynamic_bitset &other) {
        operate_inplace(other, [](u64 a, u64 b){ return a & b; });
        return *this;
    }

    dynamic_bitset &operator^=(const dynamic_bitset &other) {
        operate_inplace(other, [](u64 a, u64 b){ return a ^ b; });
        return *this;
    }

    dynamic_bitset &operator<<=(std::size_t n) {
        if (n == 0 || nbits == 0) { nbits += n; return *this; }
        std::size_t old_n = nbits;
        std::size_t word = n >> 6;
        std::size_t rem = n & 63u;
        std::vector<u64> old = chunks;
        // mask off unused bits in old last chunk
        if (old_n & 63u) {
            u64 mask = ((u64(1) << (old_n & 63u)) - 1);
            old[(old_n - 1) >> 6] &= mask;
        }
        nbits += n;
        ensure_capacity(nbits);
        std::fill(chunks.begin(), chunks.end(), 0);
        std::size_t old_chunks = (old_n + 63) / 64;
        for (std::size_t i = 0; i < old_chunks; ++i) {
            u64 v = old[i];
            if (v == 0) continue;
            std::size_t dst = i + word;
            chunks[dst] |= (rem ? (v << rem) : v);
            if (rem && dst + 1 < chunks.size()) {
                chunks[dst + 1] |= (v >> (64 - rem));
            }
        }
        // mask new last chunk to nbits
        mask_to_size();
        return *this;
    }

    dynamic_bitset &operator>>=(std::size_t n) {
        if (n == 0) return *this;
        if (n >= nbits) {
            nbits = 0; chunks.clear(); return *this;
        }
        std::size_t word = n >> 6;
        std::size_t rem = n & 63u;
        std::size_t old_chunks = chunks.size();
        std::vector<u64> old = chunks;
        std::size_t new_chunks = old_chunks - word;
        chunks.assign(new_chunks, 0);
        for (std::size_t i = 0; i < new_chunks; ++i) {
            u64 v = old[i + word];
            u64 res = (rem ? (v >> rem) : v);
            if (rem && i + word + 1 < old_chunks) {
                res |= (old[i + word + 1] << (64 - rem));
            }
            chunks[i] = res;
        }
        nbits -= n;
        mask_to_size();
        return *this;
    }

    dynamic_bitset &set() {
        if (nbits == 0) return *this;
        std::size_t full = nbits >> 6;
        for (std::size_t i = 0; i < full; ++i) chunks[i] = ~u64(0);
        std::size_t rem = nbits & 63u;
        if (rem) chunks[full] = ((u64(1) << rem) - 1);
        return *this;
    }

    dynamic_bitset &flip() {
        std::size_t chunks_cnt = chunks.size();
        for (std::size_t i = 0; i < chunks_cnt; ++i) chunks[i] = ~chunks[i];
        mask_to_size();
        return *this;
    }

    dynamic_bitset &reset() {
        std::fill(chunks.begin(), chunks.end(), 0);
        return *this;
    }

private:
    std::vector<u64> chunks;
    std::size_t nbits = 0;

    void ensure_capacity(std::size_t n) {
        std::size_t need = (n + 63) / 64;
        if (chunks.size() < need) chunks.resize(need, 0);
    }

    void resize_zero(std::size_t n) {
        nbits = n;
        chunks.assign((nbits + 63) / 64, 0);
    }

    void set_bit(std::size_t n) {
        std::size_t idx = n >> 6; std::size_t off = n & 63u;
        ensure_capacity(nbits);
        chunks[idx] |= (u64(1) << off);
    }
    void clear_bit(std::size_t n) {
        std::size_t idx = n >> 6; std::size_t off = n & 63u;
        chunks[idx] &= ~(u64(1) << off);
    }

    void mask_to_size() {
        if (nbits == 0) return;
        std::size_t last_idx = (nbits - 1) >> 6;
        std::size_t rem = nbits & 63u;
        if (rem) {
            u64 mask = ((u64(1) << rem) - 1);
            chunks[last_idx] &= mask;
        }
        // truncate extra chunks if any
        chunks.resize((nbits + 63) / 64);
    }

    template <class Op>
    void operate_inplace(const dynamic_bitset &other, Op op) {
        std::size_t overlap = std::min(nbits, other.nbits);
        if (overlap == 0) return;
        std::size_t full = overlap >> 6;
        for (std::size_t i = 0; i < full; ++i) {
            chunks[i] = op(chunks[i], other.chunks.size() > i ? other.chunks[i] : u64(0));
        }
        std::size_t rem = overlap & 63u;
        if (rem) {
            u64 mask = ((u64(1) << rem) - 1);
            u64 a = chunks[full];
            u64 b = (other.chunks.size() > full ? other.chunks[full] : u64(0));
            u64 within = op(a, b) & mask;
            chunks[full] = (a & ~mask) | within;
        }
    }
};
