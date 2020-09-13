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
#include <iostream>

#include "property.h"

struct DefaultLinkPolicy: std::false_type {};
struct ReplaceLinkPolicy : std::true_type {};
template<class T>struct tag{using type=T;};

using mapType = std::map<std::string, std::shared_ptr<property>>;

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
	property& reg(const std::string &_name, property &_p);

	/* create a new property with starting value */
	property& reg(const std::string &_name,
			const property &_cp,
			property::write_hook_t w = nullptr,
			property::read_hook_t r = nullptr);

	/* unlink the property */
	bool unreg(const std::string &_name);

	/* set the property */
	virtual bool set(const std::string &name, const property_base &p);

	/* get the property, return default if not existing */
	property get(const std::string &name, const property& _default = {}) const;

	/* convert the property, return given value if not existing */
	template<typename T>
	T const get(const std::string &name, const std::string &key, const T &_default = T{}) const {
		property _p = get(name);
		property::const_iterator it = _p.find(key);
		if (it != _p.end()) {
			return std::move(mapbox::util::get<T>(it->second));
		} else {
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

	/* notify */
	virtual void onSetNotify(const std::string &name);

	std::string name() const {
		return m_repName;
	}

protected:
	std::string propName(const std::string &name) const;
	bool set(mapType::iterator it, const property_base &p);

private:
	std::string m_repName;
	mapType m_props;
	link_policy_t link_policy;
	std::mutex m_lock;

};

#endif /* MAIN_REPOSITORY_H_ */
