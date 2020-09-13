/*
 * property.h
 *
 *  Created on: 28.08.2020
 *      Author: marco
 */

#ifndef MAIN_PROPERTY_H_
#define MAIN_PROPERTY_H_

#include <memory>
#include <unordered_map>
#include <mapbox/variant.hpp>
#include <assert.h>
#include <initializer_list>
#include <ostream>

struct BoolType {
	explicit constexpr BoolType(const bool v = false) : value(v) {}
	friend std::ostream& operator << (std::ostream &os, const BoolType _b) {
		os << _b.value;
		return os;
	}

	bool value;
};


using key_type = std::string;
using base_type = mapbox::util::variant<BoolType, int, double, std::string>;
using property_base = std::unordered_map<key_type,base_type>;

class repository;

class property {
public:

	using write_hook_t = std::function<void(const property_base &p)>;
	using read_hook_t = std::function<property_base()>;
	using pointer_t = std::shared_ptr<property_base>;
	using iterator = property_base::iterator;
	using const_iterator = property_base::const_iterator;

	struct base_type_proxy {
		property &m_p;
		const key_type &m_n;

		base_type_proxy(property &_p, const key_type &_n) : m_p(_p), m_n(_n) {}

		template<typename T>
		void operator=(T _v) {
			m_p.set(m_n, base_type{_v});
		}

		base_type operator()() const {
			property_base _b = m_p.get();
			return _b.at(m_n);
		}

		operator base_type() const {
			property_base _b = m_p.get();
			return _b.at(m_n);
		}
	};

    property() :
		property({}, nullptr, nullptr) {
	}

    property(const property_base &p, write_hook_t _w = nullptr, read_hook_t _r = nullptr) :
    	m_pProperty{p}, m_name(), m_repository(), write_hook(std::move(_w)), read_hook(std::move(_r)) {
    }

	property& reg(const std::string &name, repository &_repo);

	property_base get() const {
		return read_hook ? read_hook() : m_pProperty;
	}

	property& set(const property_base &p);
	property& set(const key_type &_n, base_type &&_b);
	property& set(const key_type &name, const repository& _rep);

	property& set(write_hook_t &&w) {
		write_hook = w;
		return *this;
	}

	property& set(read_hook_t &&r) {
		read_hook = r;
		return *this;
	}

	void operator=(const property_base &t) {
		set(t);
	}

	property_base operator()() const {
		return get();
	}

	operator property_base() const {
		return get();
	}

	/**
	 * map iterator proxy
	 */
	iterator find( const key_type& key ) {return m_pProperty.find(key);}
	const_iterator find( const key_type& key ) const {return m_pProperty.find(key);}
	iterator begin() noexcept { return m_pProperty.begin();}
	const_iterator begin() const noexcept { return m_pProperty.begin();}
	iterator end() noexcept { return m_pProperty.end();}
	const_iterator end() const noexcept { return m_pProperty.end();}
	/**
	 * map operator [] proxy
	 */
	base_type_proxy operator[]( const key_type& key ) {return {*this,key};}
	base_type_proxy operator[]( key_type&& key ){return {*this,key};}

private:

	property_base m_pProperty;
	std::string m_name;
	repository *m_repository;
	write_hook_t write_hook;
	read_hook_t read_hook;
};


#endif /* MAIN_PROPERTY_H_ */
