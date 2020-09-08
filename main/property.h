/*
 * property.h
 *
 *  Created on: 28.08.2020
 *      Author: marco
 */

#ifndef MAIN_PROPERTY_H_
#define MAIN_PROPERTY_H_

#include <iostream>
#include <memory>
#include <unordered_map>
#include <mapbox/variant.hpp>
#include <assert.h>

struct BoolType {
	explicit constexpr BoolType(const bool v = false) : value(v) {}
    friend std::ostream & operator<<(std::ostream &os, const BoolType& b) {
    	return os << b.value;
    }
	bool value;
};

using property = std::unordered_map<std::string,mapbox::util::variant<BoolType, int, double, std::string>>;

/**
 * the wrapper does not own the property, but uses read/write hooks that can be overridden
 */
class property_wrapper {
public:

	using write_hook_t = std::function<void(const property &p)>;
	using read_hook_t = std::function<property()>;
	using pointer_t = std::shared_ptr<property>;

	explicit property_wrapper() :
			property_wrapper(pointer_t(new property()), nullptr, nullptr) {
	}

	explicit property_wrapper(property &p, write_hook_t _w, read_hook_t _r) :
			property_wrapper(pointer_t(new property(p)), _w, _r) {
	}

	explicit property_wrapper(pointer_t p, write_hook_t _w, read_hook_t _r) :
			m_pProperty(p), write_hook(std::move(_w)), read_hook(std::move(_r)) {
	}

	property get() const {
		assert(m_pProperty.get() != nullptr);
		return read_hook ? read_hook() : *(m_pProperty.get());
	}

	void set(const property &p) {
		assert(m_pProperty.get() != nullptr);
		if (write_hook != nullptr) {
			write_hook(p);
		} else {
			for (auto &v : p) {
				//FIXME std::cout << "property set: " << v.first << std::endl;
				m_pProperty.get()->emplace(v.first, v.second).first->second = v.second;
			}
		}
	}

	void operator=(const property &t) {
		set(t);
	}

	property operator()() const {
		return get();
	}

	operator property() const {
		return get();
	}

private:
	pointer_t m_pProperty;
	const write_hook_t write_hook;
	const read_hook_t read_hook;
};


#endif /* MAIN_PROPERTY_H_ */
