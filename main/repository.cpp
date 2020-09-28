/*
 * repository.cpp
 *
 *  Created on: 29.08.2020
 *      Author: marco
 */

#include "repository.h"
#include "utilities.h"
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

	void operator()(monostate) const {
		_s << "monostate: " << "\n";
	}
	void operator()(BoolType i) const {
		_s << "bool: " << _f <<" : " << i << "\n";
	}
	void operator()(IntType i) const {
		_s << "int: " << _f <<" : " << i << "\n";
	}
	void operator()(FloatType f) const {
		_s << "float: " << _f <<" : " << f << "\n";
	}
	void operator()(const StringType &s) const {
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

template<> void JsonVisitor::operator()(const monostate &) const {}

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

repository::mapType repository::partial(const std::string &_part) {
	mapType ret;
	std::string part = propName(_part);
	for (mapType::iterator it = m_properties.begin(), end = m_properties.end(); it != end; ++it) {
		if (it->first.substr(0, part.length()) == part) {
			ret.emplace(it->first, it->second);
		}
	}
	return ret;
}

std::string repository::debug(const mapType &_m) {
	std::stringstream _ss;
	_ss << "map<property>" << "\n";
	for (mapType::const_iterator it = _m.begin(), end = _m.end(); it != end; ++it) {
		for (auto &v : it->second->get()) {
			_ss << it->first << " : ";
			mapbox::util::apply_visitor(DebugVisitor(_ss, v.first), v.second);
		}
	}
	_ss << "End of map" << "\n";
	return _ss.str();
}

std::string repository::stringify(const mapType &_m) const {
	nlohmann::json root;
	for (mapType::const_iterator it = _m.begin(), end = _m.end(); it != end; ++it) {
		for (auto &v : it->second->get()) {
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

bool repository::set(const std::string &_name, const property::property_base &p) {
	std::lock_guard<std::mutex> lock(m_lock);
	/*set works w/wo omitting repository name */
	//FIXME std::cout << "rep:" << name() << " -> " << _name << " set " << "\n";
	return set(find(propName(_name)), p);
}

bool repository::set(mapType::iterator it, const property::property_base &p) {
	bool ret = false;
	if (it != m_properties.end()) {
		it->second->set(p);
		ret = true;
	}
	return ret;
}

property& repository::link(const std::string &name, property &_p) {
	std::lock_guard<std::mutex> lock(m_lock);
	std::string cname = propName(name);
	std::pair<mapType::iterator, bool> ret = m_properties.emplace(cname, nullptr);
	_p.set(name, *this);

	if (false == ret.second) {
		//insert failed, set existing entry if allowed by link policy
		if (true == link_policy()) {
			//FIXME std::cout << cname << " found in repository, setting" << "\n";
			set(ret.first, _p);
		} else {
			//FIXME std::cout << cname << " found in repository, unchanged" << "\n";
		}
	} else {
		ret.first->second = mappedType(&_p, [](property*){});
	}

	return *ret.first->second;
}

property& repository::create(const std::string &name, const property &_cp) {
	std::lock_guard<std::mutex> lock(m_lock);
	std::string cname = propName(name);
	std::pair<mapType::iterator, bool> ret = m_properties.emplace(cname, nullptr);
	if (true == ret.second) {
		ret.first->second = std::make_shared<property>(_cp);
		ret.first->second->set(name, *this); //TODO name or cname
	}
	return *ret.first->second;
}

bool repository::unlink(const std::string &name) {
	std::lock_guard<std::mutex> lock(m_lock);
	return m_properties.erase(propName(name)) != 0;
}

void repository::parse(const std::string &c) {
	//FIXME std::cout << "parsing : " << c << "\n";
	try {
		nlohmann::json re = nlohmann::json::parse(c);

		std::string path;
		recursive_iterate(re, path, [this](std::string ipath, nlohmann::json::const_iterator it) {
			std::string propPath = propName(ipath);
			//FIXME std::cout << propPath << " : " << it.key() << " < " << it.value() << "\n";
			if (it.value().is_boolean()) {
				create(propPath, property())[it.key()] = it->get<bool>();
			} else if (it.value().is_number_integer()) {
				create(propPath, property())[it.key()] = it->get<int>();
			} else if (it.value().is_number_float()) {
				create(propPath, property())[it.key()] = it->get<double>();
			} else if (it.value().is_string()) {
				create(propPath, property())[it.key()] = it->get<std::string>();
			} else {
				assert(false);
			}
		});
	} catch (const std::exception &e) {
		//FIXME std::cout << "exception on parse " << e.what() << "\n";
	};
}

property& repository::operator [](const std::string &key) {
	return create(key, property());
}

property& repository::operator [](std::string &&key) {
	return create(key, property());
}
