/*
 * repository.cpp
 *
 *  Created on: 29.08.2020
 *      Author: marco
 */

#include "repository.h"
#include "utilities.h"
#include <iostream>
#include <sstream>
#include <string>
#include "json.hpp"

struct both_slashes {
    bool operator()(char a, char b) const {
        return a == '/' && b == '/';
    }
};

struct DebugVisitor {
	DebugVisitor(std::stringstream &s, const std::string &f) : _s(s), _f(f) {}

	void operator()(BoolType i) const {
		_s << "bool: " << _f <<" : " << i.value << "\n";
	}
	void operator()(int i) const {
		_s << "int: " << _f <<" : " << i << "\n";
	}
	void operator()(double f) const {
		_s << "float: " << _f <<" : " << f << "\n";
	}
	void operator()(const std::string &s) const {
		_s << "string: " << _f <<" : " << s << "\n";
	}
private:
	std::stringstream &_s;
	std::string _f;
};

class JsonVisitor {
public:
	JsonVisitor(const std::string &name, std::string key, nlohmann::json &j) :
			m_name(name), m_key(key), m_json(j) {
		m_parts = utilities::split(name, "/");
	}

	template<typename T>
	void operator()(const T &i) const {
		assert(m_parts.size() > 0);
		if (m_parts.size() == 1) {
			m_json[m_parts[0]][m_key] = i;
		} else if (m_parts.size() == 2) {
			m_json[m_parts[0]][m_parts[1]][m_key] = i;
		} else if (m_parts.size() == 3) {
			m_json[m_parts[0]][m_parts[1]][m_parts[2]][m_key] = i;
		} else if (m_parts.size() == 4) {
			m_json[m_parts[0]][m_parts[1]][m_parts[2]][m_parts[3]][m_key] = i;
		}
	}

private:

	std::string m_name, m_key;
	nlohmann::json &m_json;
	std::vector<std::string> m_parts;
};

//specialised bool version
template<>
void JsonVisitor::operator()(const BoolType &i) const {
	assert(m_parts.size() > 0);
	if (m_parts.size() == 1) {
		m_json[m_parts[0]][m_key] = i.value;
	} else if (m_parts.size() == 2) {
		m_json[m_parts[0]][m_parts[1]][m_key] = i.value;
	} else if (m_parts.size() == 3) {
		m_json[m_parts[0]][m_parts[1]][m_parts[2]][m_key] = i.value;
	} else if (m_parts.size() == 4) {
		m_json[m_parts[0]][m_parts[1]][m_parts[2]][m_parts[3]][m_key] = i.value;
	}
}

template<class UnaryFunction>
void recursive_iterate(const nlohmann::json &j, std::string path, UnaryFunction f) {
	for (auto it = j.begin(); it != j.end(); ++it) {
		if (it->is_structured()) {
			recursive_iterate(*it, path + "/" + it.key(), f);
		} else {
			f(path, it);
		}
	}
}

mapType repository::partial(const std::string &_part) {
	mapType ret;
	std::string part = propName(_part);
	for (mapType::iterator it = m_props.begin(); it != m_props.end(); ++it) {
		if (it->first.substr(0, part.length()) == part) {
			ret.emplace(it->first, it->second);
		}
	}
	return ret;
}

std::string repository::debug(const mapType &_m) {
	std::stringstream _ss;
	_ss << "map<property>" << std::endl;
	for (mapType::const_iterator it = _m.begin(); it != _m.end(); ++it) {
		_ss << it->first << " : ";
		for (auto &v : it->second.get()) {
			mapbox::util::apply_visitor(DebugVisitor(_ss, v.first), v.second);
		}
	}
	_ss << "End of map" << std::endl;
	return _ss.str();
}

std::string repository::stringify(const mapType &_m) {
	nlohmann::json root;
	for (mapType::const_iterator it = _m.begin(); it != _m.end(); ++it) {
		for (auto &v : it->second.get()) {
			mapbox::util::apply_visitor(JsonVisitor(it->first, v.first, root), v.second);
		}
	}
	return root.dump(4);
}

std::string repository::propName(const std::string &name) const {
	std::string path =  name.substr(0, m_repName.length()) != m_repName ? m_repName + "/" + name : name;
	path.erase(std::unique(path.begin(), path.end(), both_slashes()), path.end());
	return path;
}

bool repository::set(const std::string &name, const property &p) {
	std::lock_guard<std::mutex> lock(m_lock);
	/*set works w/wo omitting repository name */
	std::string cname = propName(name);
	return set(m_props.find(cname), p);
}

bool repository::set(mapType::iterator it, const property &p) {
	bool ret = false;
	if (it != m_props.end()) {
		//FIXME std::cout << "set: " << it->first << " found in repository " << std::endl;
		it->second.set(p);
		ret = true;
	}
	return ret;
}

property repository::get(const std::string &name, const property &_default) const {
	/*get works w/wo omitting repository name */
	std::string cname = propName(name);
	if (m_props.find(cname) != m_props.end()) {
		//FIXME std::cout << "get: " << cname << " found in repository " << std::endl;
		return std::move(m_props.at(cname).get());
	} else {
		//FIXME std::cout << "get: " << cname << " not found in repository" << std::endl;
		return std::move(_default);
	}
}

property& repository::reg(const std::string &name, const property &_cp, property_wrapper::write_hook_t w,
		property_wrapper::read_hook_t r) {
	std::lock_guard<std::mutex> lock(m_lock);
	property_wrapper::pointer_t _p(new property(_cp));
	std::string cname = propName(name);
	std::pair<mapType::iterator, bool> ret = m_props.emplace(cname, property_wrapper(std::move(_p), w, r));

	if (false == ret.second) {
		//insert failed, set existing entry if allowed by link policy
		if (true == link_policy()) {
			//FIXME std::cout << cname << " found in repository, setting" << std::endl;
			set(ret.first, _cp);
		} else {
			//FIXME std::cout << cname << " found in repository, unchanged" << std::endl;
		}
	}

	return *_p.get();
}

property& repository::reg(const std::string &name, property &_p, property_wrapper::write_hook_t w,
		property_wrapper::read_hook_t r) {
	std::lock_guard<std::mutex> lock(m_lock);
	std::string cname = propName(name);
	std::pair<mapType::iterator, bool> ret = m_props.emplace(cname, property_wrapper(_p, w, r));
	if (false == ret.second) {
		if (true == link_policy()) {
			//insert failed, replace existing link if allowed by link policy
			//FIXME std::cout << cname << " found in repository, replacing" << std::endl;
			property old = (*ret.first).second.get();
			m_props.erase(cname);
			m_props.emplace(cname, property_wrapper(_p, w, r));
			set(ret.first, old);
		} else {
			//FIXME std::cout << cname << " found in repository, unchanged" << std::endl;
		}
	}
	return _p;
}

bool repository::unreg(const std::string &name) {
	std::lock_guard<std::mutex> lock(m_lock);
	std::string cname = propName(name);
	return m_props.erase(cname) != 0;
}

void repository::parse(const std::string &c) {
	//FIXME std::cout << "parsing : " << c << std::endl;
	try {
		nlohmann::json re = nlohmann::json::parse(c);

		std::string path;
		recursive_iterate(re, path, [this](std::string ipath, nlohmann::json::const_iterator it) {
			std::string propPath = propName(ipath);
			//FIXME std::cout << propPath << " , " << ipath << " : " << it.key() << " -> " << it.value() << std::endl;

			if (it.value().is_boolean()) {
				if (!set(propPath, {{it.key(),BoolType(it->get<bool>())}})) {
					reg(propPath, {{it.key(),BoolType(it->get<bool>())}});
				}
			} else if (it.value().is_number_integer()) {
				if (!set(propPath, {{it.key(),it->get<int>()}})) {
					reg(propPath, {{it.key(),it->get<int>()}});
				}
			} else if (it.value().is_number_float()) {
				if (!set(propPath, {{it.key(),it->get<double>()}})) {
					reg(propPath, {{it.key(),it->get<double>()}});
				}
			} else if (it.value().is_string()) {
				if (!set(propPath, {{it.key(),it->get<std::string>()}})) {
					reg(propPath, {{it.key(),it->get<std::string>()}});
				}
			} else {
				assert(false);
			}
		});
	} catch (const std::exception &e) {
		//FIXME std::cout << "exception on parse " << e.what() << std::endl;
	};
}
