/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019 Giovanni Petrantoni */

#pragma once

#include "chainblocks.h"

template <typename T> class PtrIterator {
public:
  typedef T value_type;

  PtrIterator(T *ptr) : ptr(ptr) {}

  PtrIterator &operator+=(std::ptrdiff_t s) {
    ptr += s;
    return *this;
  }
  PtrIterator &operator-=(std::ptrdiff_t s) {
    ptr -= s;
    return *this;
  }

  PtrIterator operator+(std::ptrdiff_t s) {
    PtrIterator it(ptr);
    return it += s;
  }
  PtrIterator operator-(std::ptrdiff_t s) {
    PtrIterator it(ptr);
    return it -= s;
  }

  PtrIterator operator++() {
    ++ptr;
    return *this;
  }

  PtrIterator operator++(int s) {
    ptr += s;
    return *this;
  }

  std::ptrdiff_t operator-(PtrIterator const &other) const {
    return ptr - other.ptr;
  }

  bool operator!=(const PtrIterator &other) const { return ptr != other.ptr; }
  const T &operator*() const { return *ptr; }

private:
  T *ptr;
};

template <typename T, typename Allocator = std::allocator<T>>
class IterableStb {
public:
  typedef T *seq_type;
  typedef T value_type;
  typedef size_t size_type;

  typedef PtrIterator<T> iterator;
  typedef PtrIterator<const T> const_iterator;

  IterableStb() : _seq(nullptr), _owned(true) {}

  IterableStb(size_t s) : _seq(nullptr), _owned(true) {
    stbds_arrsetlen(_seq, s);
  }

  IterableStb(size_t s, T v) : _seq(nullptr), _owned(true) {
    stbds_arrsetlen(_seq, s);
    for (auto i = 0; i < s; i++) {
      _seq[i] = v;
    }
  }

  IterableStb(const_iterator first, const_iterator last)
      : _seq(nullptr), _owned(true) {
    size_t size = last - first;
    stbds_arrsetlen(_seq, size);
    for (auto i = 0; i < size; i++) {
      _seq[i] = *first++;
    }
  }

  IterableStb(const IterableStb &other) : _seq(nullptr), _owned(true) {
    size_t size = stbds_arrlen(other._seq);
    stbds_arrsetlen(_seq, size);
    for (auto i = 0; i < size; i++) {
      _seq[i] = other._seq[i];
    }
  }

  // Not OWNING!
  IterableStb(seq_type seq) : _seq(seq), _owned(false) {}

  IterableStb &operator=(IterableStb &other) {
    std::swap(_seq, other._seq);
    std::swap(_owned, other._owned);
    return *this;
  }

  IterableStb &operator=(const IterableStb &other) {
    size_t size = stbds_arrlen(other._seq);
    stbds_arrsetlen(_seq, size);
    for (auto i = 0; i < size; i++) {
      _seq[i] = other._seq[i];
    }
    return *this;
  }

  ~IterableStb() {
    if (_owned) {
      stbds_arrfree(_seq);
    }
  }

private:
  seq_type _seq;
  bool _owned;

public:
  iterator begin() { return iterator(&_seq[0]); }
  const_iterator begin() const { return const_iterator(&_seq[0]); }
  const_iterator cbegin() const { return const_iterator(&_seq[0]); }
  iterator end() { return iterator(&_seq[0] + stbds_arrlen(_seq)); }
  const_iterator end() const {
    return const_iterator(&_seq[0] + stbds_arrlen(_seq));
  }
  const_iterator cend() const {
    return const_iterator(&_seq[0] + stbds_arrlen(_seq));
  }
  T &operator[](int index) { return _seq[index]; }
  const T &operator[](int index) const { return _seq[index]; }
  T &front() { return _seq[0]; }
  const T &front() const { return _seq[0]; }
  T &back() { return _seq[stbds_arrlen(_seq) - 1]; }
  const T &back() const { return _seq[stbds_arrlen(_seq) - 1]; }
  T *data() { return _seq; }
  size_t size() const { return stbds_arrlen(_seq); }
  bool empty() const { return _seq == nullptr || stbds_arrlen(_seq) == 0; }
  void resize(size_t nsize) { stbds_arrsetlen(_seq, nsize); }
};

namespace chainblocks {
using IterableSeq = IterableStb<CBVar>;
using IterableExposedInfo = IterableStb<CBExposedTypeInfo>;
using TensorShape = IterableStb<uint64_t>;
}; // namespace chainblocks

// needed by xtensor_adaptor etc
namespace std {
template <> class iterator_traits<chainblocks::TensorShape::iterator> {
public:
  using difference_type = std::ptrdiff_t;
  using size_type = std::size_t;
  using value_type = uint64_t;
  using pointer = uint64_t *;
  using reference = uint64_t &;
  using iterator_category = std::random_access_iterator_tag;
};
template <typename T> class iterator_traits<PtrIterator<T>> {
public:
  using difference_type = std::ptrdiff_t;
  using size_type = std::size_t;
  using value_type = T;
  using pointer = T *;
  using reference = T &;
  using iterator_category = std::random_access_iterator_tag;
};
}; // namespace std
