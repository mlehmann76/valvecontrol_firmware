/*
 * circular_buffer.h
 *
 *  Created on: 26.10.2020
 *      Author: marco
 *
 *      based on P0059R4
 */

#ifndef LOGGER_CIRCULAR_BUFFER_H_
#define LOGGER_CIRCULAR_BUFFER_H_

#include <array>
#include <cassert>
#include <cstdlib>
#include <type_traits>

template <typename, bool> class ring_iterator;

template <typename T, std::size_t SIZE> class circular_buffer {
  public:
    using type = circular_buffer<T, SIZE>;
    using container_type = std::array<T, SIZE>;
    using size_type = std::size_t;
    using value_type = T;
    using pointer = T *;
    using reference = T &;
    using const_reference = const T &;
    using iterator = ring_iterator<type, false>;
    using const_iterator = ring_iterator<type, true>;

    friend class ring_iterator<type, false>;
    friend class ring_iterator<type, true>;

    circular_buffer();
    circular_buffer(circular_buffer &&) = default;
    circular_buffer &operator=(circular_buffer &&) = default;

    bool empty() const noexcept;
    bool full() const noexcept;
    size_type size() const noexcept;
    size_type capacity() const noexcept;
    reference front() noexcept;
    const_reference front() const noexcept;
    reference back() noexcept;
    const_reference back() const noexcept;
    iterator begin() noexcept;
    const_iterator begin() const noexcept;
    const_iterator cbegin() const noexcept;
    iterator end() noexcept;
    const_iterator end() const noexcept;
    const_iterator cend() const noexcept;
    template <bool b = true, typename = std::enable_if_t<
                                 b && std::is_copy_assignable<T>::value>>
    void push_back(const value_type &from_value) noexcept(
        std::is_nothrow_copy_assignable<T>::value);

    template <bool b = true, typename = std::enable_if_t<
                                 b && std::is_move_assignable<T>::value>>
    void push_back(value_type &&from_value) noexcept(
        std::is_nothrow_move_assignable<T>::value);

    template <class... FromType>
    void emplace_back(FromType &&... from_value) noexcept(
        std::is_nothrow_constructible<T, FromType...>::value
            &&std::is_nothrow_move_assignable<T>::value);

    T pop_front();

    void swap(type &rhs) noexcept;

  private:
    reference at(size_type idx) noexcept;
    const_reference at(size_type idx) const noexcept;
    size_type back_idx() const noexcept;
    void increase_size() noexcept;

    container_type m_rc;
    size_type m_size;
    size_type m_capacity;
    size_type m_front_idx;
};

template <typename T, std::size_t SIZE>
circular_buffer<T, SIZE>::circular_buffer()
    : m_size(0), m_capacity(SIZE), m_front_idx(0) {}

template <typename T, std::size_t SIZE>
bool circular_buffer<T, SIZE>::empty() const noexcept {
    return m_size == 0;
}

template <typename T, std::size_t SIZE>
bool circular_buffer<T, SIZE>::full() const noexcept {
    return m_size == m_capacity;
}

template <typename T, std::size_t SIZE>
typename circular_buffer<T, SIZE>::size_type
circular_buffer<T, SIZE>::size() const noexcept {
    return m_size;
}

template <typename T, std::size_t SIZE>
typename circular_buffer<T, SIZE>::size_type
circular_buffer<T, SIZE>::capacity() const noexcept {
    return m_capacity;
}

template <typename T, std::size_t SIZE>
typename circular_buffer<T, SIZE>::reference
circular_buffer<T, SIZE>::front() noexcept {
    return m_rc.at(m_front_idx);
}

template <typename T, std::size_t SIZE>
typename circular_buffer<T, SIZE>::const_reference
circular_buffer<T, SIZE>::front() const noexcept {
    return m_rc.at(m_front_idx);
}

template <typename T, std::size_t SIZE>
typename circular_buffer<T, SIZE>::reference
circular_buffer<T, SIZE>::back() noexcept {
    return *(--end());
}

template <typename T, std::size_t SIZE>
typename circular_buffer<T, SIZE>::const_reference
circular_buffer<T, SIZE>::back() const noexcept {
    return *(--end());
}

template <typename T, std::size_t SIZE>
typename circular_buffer<T, SIZE>::iterator
circular_buffer<T, SIZE>::begin() noexcept {
    return iterator(m_front_idx, this);
}

template <typename T, std::size_t SIZE>
typename circular_buffer<T, SIZE>::const_iterator
circular_buffer<T, SIZE>::begin() const noexcept {
    return const_iterator(m_front_idx, this);
}

template <typename T, std::size_t SIZE>
typename circular_buffer<T, SIZE>::const_iterator
circular_buffer<T, SIZE>::cbegin() const noexcept {
    return const_iterator(m_front_idx, this);
}

template <typename T, std::size_t SIZE>
typename circular_buffer<T, SIZE>::iterator
circular_buffer<T, SIZE>::end() noexcept {
    return iterator(size() + m_front_idx, this);
}

template <typename T, std::size_t SIZE>
typename circular_buffer<T, SIZE>::const_iterator
circular_buffer<T, SIZE>::end() const noexcept {
    return const_iterator(size() + m_front_idx, this);
}

template <typename T, std::size_t SIZE>
typename circular_buffer<T, SIZE>::const_iterator
circular_buffer<T, SIZE>::cend() const noexcept {
    return const_iterator(size() + m_front_idx, this);
}

template <typename T, std::size_t SIZE>
template <bool b, typename>
void circular_buffer<T, SIZE>::push_back(const value_type &value) noexcept(
    std::is_nothrow_copy_assignable<T>::value) {
    if (full())
        pop_front();
    m_rc[back_idx()] = value;
    increase_size();
}

template <typename T, std::size_t SIZE>
template <bool b, typename>
void circular_buffer<T, SIZE>::push_back(value_type &&value) noexcept(
    std::is_nothrow_move_assignable<T>::value) {
    if (full())
        pop_front();
    m_rc[back_idx()] = std::move(value);
    increase_size();
}

template <typename T, std::size_t SIZE>
template <class... FromType>
void circular_buffer<T, SIZE>::emplace_back(FromType &&... from_value) noexcept(
    std::is_nothrow_constructible<T, FromType...>::value
        &&std::is_nothrow_move_assignable<T>::value) {
    m_rc[back_idx()] = T(std::forward<FromType>(from_value)...);
    increase_size();
}

template <typename T, std::size_t SIZE>
T circular_buffer<T, SIZE>::pop_front() {
    assert(m_size != 0);
    auto old_front_idx = m_front_idx;
    m_front_idx = (m_front_idx + 1) % m_capacity;
    --m_size;
    return std::move(m_rc[old_front_idx]);
}

template <typename T, std::size_t SIZE>
void circular_buffer<T, SIZE>::swap(type &rhs) noexcept {
    using std::swap;
    swap(m_rc, rhs.m_rc);
    swap(m_size, rhs.m_size);
    swap(m_capacity, rhs.m_capacity);
    swap(m_front_idx, rhs.m_front_idx);
}

template <typename T, std::size_t SIZE>
typename circular_buffer<T, SIZE>::reference
circular_buffer<T, SIZE>::at(size_type idx) noexcept {
    return m_rc.at(idx % m_capacity);
}

template <typename T, std::size_t SIZE>
typename circular_buffer<T, SIZE>::const_reference
circular_buffer<T, SIZE>::at(size_type idx) const noexcept {
    return m_rc.at(idx % m_capacity);
}

template <typename T, std::size_t SIZE>
typename circular_buffer<T, SIZE>::size_type
circular_buffer<T, SIZE>::back_idx() const noexcept {
    return (m_front_idx + m_size) % m_capacity;
}

template <typename T, std::size_t SIZE>
void circular_buffer<T, SIZE>::increase_size() noexcept {
    if (++m_size > m_capacity) {
        m_size = m_capacity;
    }
}

/*
 *
 */
template <typename Ring, bool is_const> class ring_iterator {
  public:
    using type = ring_iterator<Ring, is_const>;
    using size_type = typename Ring::size_type;
    using reference = typename Ring::reference;
    using const_reference = typename Ring::const_reference;

    ring_iterator(size_type idx, Ring *rv) noexcept;
    ring_iterator(size_type idx, const Ring *rv) noexcept;

    template <bool C>
    bool operator==(const ring_iterator<Ring, C> &rhs) const noexcept;
    template <bool C>
    bool operator!=(const ring_iterator<Ring, C> &rhs) const noexcept;
    template <bool C>
    bool operator<(const ring_iterator<Ring, C> &rhs) const noexcept;
    template <typename R_ = Ring,
              typename = std::enable_if_t<!std::is_const_v<R_>>>
    reference operator*() const noexcept;
    template <typename R_ = Ring,
              typename = std::enable_if_t<std::is_const_v<R_>>>
    const_reference operator*() const noexcept;
    type &operator++() noexcept;
    type operator++(int) noexcept;
    type &operator--() noexcept;
    type operator--(int) noexcept;

    friend type &operator+=(type &it, int i) noexcept{
        it.m_idx += i;
        return it;
    }

    friend type &operator-=(type &it, int i) noexcept{
        it.m_idx -= i;
        return it;
    }

    size_type modulo_capacity(size_type idx) const;

  private:
    size_type m_idx;
    Ring *m_rv;
};
/**
 *
 */
template <typename Ring, bool is_const>
ring_iterator<Ring, is_const>::ring_iterator(size_type idx, Ring *rv) noexcept
    : m_idx(idx), m_rv(rv) {}

template <typename Ring, bool is_const>
ring_iterator<Ring, is_const>::ring_iterator(size_type idx,
                                             const Ring *rv) noexcept
    : m_idx(idx), m_rv(const_cast<Ring *>(rv)) {}

template <typename Ring, bool is_const>
template <bool C>
bool ring_iterator<Ring, is_const>::operator==(
    const ring_iterator<Ring, C> &rhs) const noexcept {
    return (modulo_capacity(m_idx) == rhs.modulo_capacity(rhs.m_idx)) &&
           (m_rv == rhs.m_rv);
}

template <typename Ring, bool is_const>
template <bool C>
bool ring_iterator<Ring, is_const>::operator!=(
    const ring_iterator<Ring, C> &rhs) const noexcept {
    return !(this->operator==(rhs));
}

template <typename Ring, bool is_const>
template <bool C>
bool ring_iterator<Ring, is_const>::operator<(
    const ring_iterator<Ring, C> &rhs) const noexcept {
    return (modulo_capacity(m_idx) < rhs.modulo_capacity(rhs.m_idx)) &&
           (m_rv == rhs.m_rv);
}

template <typename Ring, bool is_const>
template <typename R_, typename>
inline typename ring_iterator<Ring, is_const>::reference
ring_iterator<Ring, is_const>::operator*() const noexcept {
    return m_rv->at(m_idx);
}

template <typename Ring, bool is_const>
template <typename R_, typename>
inline typename ring_iterator<Ring, is_const>::const_reference
ring_iterator<Ring, is_const>::operator*() const noexcept {
    return m_rv->at(m_idx);
}

template <typename Ring, bool is_const>
ring_iterator<Ring, is_const> &
ring_iterator<Ring, is_const>::operator++() noexcept {
    ++m_idx;
    return *this;
}

template <typename Ring, bool is_const>
ring_iterator<Ring, is_const>
ring_iterator<Ring, is_const>::operator++(int) noexcept {
    auto r(*this);
    ++*this;
    return r;
}

template <typename Ring, bool is_const>
ring_iterator<Ring, is_const> &
ring_iterator<Ring, is_const>::operator--() noexcept {
    ++m_idx;
    return *this;
}

template <typename Ring, bool is_const>
ring_iterator<Ring, is_const>
ring_iterator<Ring, is_const>::operator--(int) noexcept {
    auto r(*this);
    ++*this;
    return r;
}

template <typename Ring, bool is_const>
typename ring_iterator<Ring, is_const>::size_type
ring_iterator<Ring, is_const>::modulo_capacity(size_type idx) const {
    return idx % m_rv->capacity();
}

/*
 *
 */

#endif /* LOGGER_CIRCULAR_BUFFER_H_ */
