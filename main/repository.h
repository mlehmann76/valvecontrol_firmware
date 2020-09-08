/*
 * repository.h
 *
 *  Created on: 28.08.2020
 *      Author: marco
 */

#ifndef MAIN_REPOSITORY_H_
#define MAIN_REPOSITORY_H_

#include <string>
#include <map>
#include <memory>
#include <mutex>

#include "property.h"

struct DefaultLinkPolicy: std::false_type {};
struct ReplaceLinkPolicy : std::true_type {};
template<class T>struct tag{using type=T;};

using mapType = std::map<std::string, property_wrapper>;

class repository {

	struct link_policy_t {
		const bool value = false;
		constexpr link_policy_t(bool v) : value(v) {}
		constexpr bool operator ()() const { return value;}
	};

public:

	template<class linkPolicy>
	repository(const std::string &name, tag<linkPolicy> = tag<DefaultLinkPolicy>{}) :
			m_repName(name), link_policy(linkPolicy::value) {
	}

	virtual ~repository() = default;

	/* link a existing property in the repository */
	property& reg(const std::string &_name, property &_p, property_wrapper::write_hook_t w = nullptr,
			property_wrapper::read_hook_t r = nullptr);

	/* create a new property with starting value */
	property& reg(const std::string &_name, const property &_cp, property_wrapper::write_hook_t w = nullptr,
			property_wrapper::read_hook_t r = nullptr);

	/* create a new property with default value */
	property& reg(const std::string &_name,
			property_wrapper::write_hook_t w = nullptr,
			property_wrapper::read_hook_t r = nullptr) {
		return reg(_name, {}, w, r);
	}

	/* unlink the property */
	bool unreg(const std::string &_name);

	/* set the property */
	virtual bool set(const std::string &name, const property &p);

	/* get the propety, return default if not existing */
	property get(const std::string &name, const property& _default = {}) const;

	/* convert the property, return given value if not existing */
	template<typename T>
	T const get(const std::string &name, const std::string &key, const T &_default = T{}) const {
		property _p = get(name);
		property::const_iterator it = _p.find(key);
		if (it != _p.end()) {
			T _ret = std::move(mapbox::util::get<T>(it->second));
			//FIXME std::cout << "get: " << key << " found in repository : " << _ret << std::endl;
			return _ret;
		}else{
			//FIXME std::cout << "get: " << key << " not found in repository " << std::endl;
			return _default;
		}
	}

	/* create json representation of the repository */
	std::string stringify() {
		return stringify(m_props);
	}

	/* create json representation of a part the repository */
	std::string stringify(const mapType &_m);

	std::string debug() {
		return debug(m_props);
	}

	std::string debug(const mapType &_m);

	/* create a part the repository */
	mapType partial(const std::string &_part);

	/* parse a json string */
	void parse(const std::string &c);

private:

	std::string propName(const std::string &name) const;
	bool set(mapType::iterator it, const property &p);

	std::string m_repName;
	mapType m_props;
	link_policy_t link_policy;
	std::mutex m_lock;

};

#endif /* MAIN_REPOSITORY_H_ */
