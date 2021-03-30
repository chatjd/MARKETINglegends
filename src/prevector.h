#ifndef _BITCOIN_PREVECTOR_H_
#define _BITCOIN_PREVECTOR_H_

#include <util.h>

#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <iterator>

#pragma pack(push, 1)
/** Implements a drop-in replacement for std::vector<T> which stores up to N
 *  elements directly (without heap allocation). The types Size and Diff are
 *  used to store element counts, and can be any unsigned + signed type.
 *
 *  Storage layout is either:
 *  - Direct allocation:
 *    - Size _size: the number of used elements (between 0 and N)
 *    - T direct[N]: an array of N elements of type T
 *      (only the first _size are initialized).
 *  - Indirect allocation:
 *    - Size _size: the number of used elements plus N + 1
 *    - Size capacity: the number of allocated elements
 *    - T* indirect: a pointer to an array of capacity elements of type T
 *      (only the first _size are initialized).
 *
 *  The data type T must be movable by memmove/realloc(). Once we switch to C++,
 *  move constructors can be used instead.
 */
template<unsigned int N, typename T, typename Size = uint32_t, typename Diff = int32_t>
class prevector {
public:
    typedef Size size_type;
    typedef Diff difference_type;
    typedef T value_type;
    typedef value_type& reference;
    typedef const value_type& const_reference;
    typedef value_type* pointer;
    typedef const value_type* const_pointer;

    class iterator {
        T* ptr;
    public:
        typedef Diff difference_type;
        typedef T value_type;
        typedef T* pointer;
        typedef T& reference;
        typedef std::random_access_iterator_tag iterator_category;
        iterator(T* ptr_) : ptr(ptr_) {}
        T& operator*() const { return *ptr; }
        T* operator->() const { return ptr; }
        T& operator[](size_type pos) { return ptr[pos]; }
        const T& operator[](size_type pos) const { return ptr[pos]; }
        iterator& operator++() { ptr++; return *this; }
        iterator& operator--() { ptr--; return *this; }
        iterator operator++(int) { iterator copy(*this); ++(*this); return copy; }
        iterator operator--(int) { iterator copy(*this); --(*this); return copy; }
        difference_type friend operator-(iterator a, iterator b) { return (&(*a) - &(*b)); }
        iterator operator+(size_type n) { return iterator(ptr + n); }
        iterator& operator+=(size_type n) { ptr += n; return *this; }
        iterator operator-(size_type n) { return iterator(ptr - n); }
        iterator& operator-=(size_type n) { ptr -= n; return *this; }
        bool operator==(iterator x) const { return ptr == x.ptr; }
        bool operator!=(iterator x) const { return ptr != x.ptr; }
        bool operator>=(iterator x) const { return ptr >= x.ptr; }
        bool operator<=(iterator x) const { return ptr <= x.ptr; }
        bool operator>(iterator x) const { return ptr > x.ptr; }
        bool operator<(iterator x) const { return ptr < x.ptr; }
    };

    class reverse_iterator {
        T* ptr;
    public:
        typedef Diff difference_type;
        typedef T value_type;
        typedef T* pointer;
        typedef T& reference;
        typedef std::bidirectional_iterator_tag iterator_category;
        reverse_iterator(T* ptr_) : ptr(ptr_) {}
        T& operator*() { return *ptr; }
        const T& operator*() const { return *ptr; }
        T* operator->() { return ptr; }
        const T* operator->() const { return ptr; }
        reverse_iterator& operator--() { ptr++; return *this; }
        reverse_iterator& operator++() { ptr--; return *this; }
        reverse_iterator operator++(int) { reverse_iterator copy(*this); ++(*this); return copy; }
        reverse_iterator operator--(int) { reverse_iterator copy(*this); --(*this); return copy; }
        bool operator==(reverse_iterator x) const { return ptr == x.ptr; }
        bool operator!=(reverse_iterator x) const { return ptr != x.ptr; }
    };

    class const_iterator {
        const T* ptr;
    public:
        typedef Diff difference_type;
        typedef const T value_type;
        typedef const T* pointer;
        typedef const T& reference;
        typedef std::random_access_iterator_tag iterator_category;
        const_iterator(const T* ptr_) : ptr(ptr_) {}
        const_iterator(iterator x) : ptr(&(*x)) {}
        const T& operator*() const { return *ptr; }
        const T* operator->() const { return ptr; }
        const T& operator[](size_type pos) const { return ptr[pos]; }
        const_iterator& operator++() { ptr++; return *this; }
        const_iterator& operator--() { ptr--; return *this; }
        const_iterator operator++(int) { const_iterator copy(*this); ++(*this); return copy; }
        const_iterator operator--(int) { const_iterator copy(*this); --(*this); return copy; }
        difference_type friend operator-(const_iterator a, const_iterator b) { return (&(*a) - &(*b)); }
        const_iterator operator+(size_type n) { return const_iterator(ptr + n); }
        const_iterator& operator+=(size_type n) { ptr += n; return *this; }
        const_iterator operator-(size_type n) { return const_iterator(ptr - n); }
        const_iterator& operator-=(size_type n) { ptr -= n; return *this; }
        bool operator==(const_iterator x) const { return ptr == x.ptr; }
        bool operator!=(const_iterator x) const { return ptr != x.ptr; }
        bool operator>=(const_iterator x) const { return ptr >= x.ptr; }
        bool operator<=(const_iterator x) const { return ptr <= x.ptr; }
        bool operator>(const_iterator x) const { return ptr > x.ptr; }
        bool operator<(const_iterator x) const { return ptr < x.ptr; }
    };

    class const_reverse_iterator {
        const T* ptr;
    public:
        typedef Diff difference_type;
        typedef const T value_type;
        typedef const T* pointer;
        typedef const T& reference;
        typedef std::bidirectional_iterator_tag iterator_category;
        const_reverse_itera