#pragma once

#include <unordered_set>
#include <utility>

template<typename T> void hash_combine(size_t & seed, T const& v) {
  seed ^= (((size_t)v)^0xdeadbeef) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

struct pair_hash {
  template <class T1, class T2>
    std::size_t operator () (const std::pair<T1,T2> &p) const {
      auto h1 = std::hash<T1>{}(p.first);
      auto h2 = std::hash<T2>{}(p.second);
      hash_combine(h1, h2);
      return h1;
    }
};
