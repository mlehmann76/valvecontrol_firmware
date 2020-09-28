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

struct monostate {  };

using BoolType = bool;
using FloatType = double;
using IntType = int;
using StringType = std::string;

class repository;

class property {
public:

	using key_type = std::string;
	using mapped_type = mapbox::util::variant<monostate, BoolType, IntType, FloatType, StringType>;
	using property_base = std::unordered_map<key_type,mapped_type>;

	using write_hook_t = std::function<void(const property &p)>;
	using read_hook_t = std::function<property&()>;
	using iterator = property_base::iterator;
	using const_iterator = property_base::const_iterator;

	struct mapped_type_proxy {
		property &m_p;
		const key_type &m_n;

		constexpr mapped_type_proxy(property &_p, const key_type &_n) : m_p(_p), m_n(_n) {}

		template<typename T>
		void operator=(T&& _v) {
			m_p.set({{m_n, _v}});
		}

		mapped_type operator()() const {
			return m_p.get().at(m_n);
		}

		operator mapped_type() const {
			return m_p.get().at(m_n);
		}
	};

	/**
	 * map operator [] proxy
	 */
	mapped_type_proxy operator[]( const key_type& key ) {return {*this,key};}
	mapped_type_proxy operator[]( key_type&& key ){return {*this,key};}

	iterator begin() { return m_pProperty.begin(); }
	iterator end() { return m_pProperty.end(); }
	iterator find(const key_type& key) { return m_pProperty.find(key); }
	const_iterator begin() const { return m_pProperty.begin(); }
	const_iterator end() const { return m_pProperty.end(); }
	const_iterator find(const key_type& key) const { return m_pProperty.find(key); }
	const_iterator cbegin() const { return m_pProperty.cbegin(); }
	const_iterator cend() const { return m_pProperty.cend(); }

	template<typename T0>
	property(const std::initializer_list<T0> t0) :
		m_pProperty{t0}, m_name(), m_repository() {}

    property() :
    	m_pProperty(), m_name(), m_repository(), write_hook(), read_hook() {
	}

	property(const property_base &p) :
		m_pProperty{p}, m_name(), m_repository(), write_hook(), read_hook() {
	}

	property& reg(const std::string &name, repository &_repo);

	property_base get() const {
		return read_hook ? read_hook().m_pProperty : m_pProperty;
	}

	property& set(const property_base &p);

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

	std::string name() const { return m_name; }

private:

	void notify();

	property_base m_pProperty;
	std::string m_name;
	repository *m_repository;
	write_hook_t write_hook;
	read_hook_t read_hook;
};


#endif /* MAIN_PROPERTY_H_ */
