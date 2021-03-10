/*
 * repository.cpp
 *
 *  Created on: 29.08.2020
 *      Author: marco
 */

#include "repository.h"
#include "json.hpp"
#include "logger.h"
#include "utilities.h"
#include <config_user.h>
#include <sstream>
#include <string>

#define TAG "repository"

struct both_slashes {
    bool operator()(char a, char b) const { return a == '/' && b == '/'; }
};

struct DebugVisitor {
    DebugVisitor(std::stringstream &s, const std::string &f) : _s(s), _f(f) {}

    void operator()(monostate) const {
        _s << "monostate: "
           << "\n";
    }
    void operator()(BoolType i) const {
        _s << "bool: " << _f << " : " << i << "\n";
    }
    void operator()(IntType i) const {
        _s << "int: " << _f << " : " << i << "\n";
    }
    void operator()(FloatType f) const {
        _s << "float: " << _f << " : " << f << "\n";
    }
    void operator()(const StringType &s) const {
        _s << "string: " << _f << " : " << s << "\n";
    }

  private:
    std::stringstream &_s;
    std::string _f;
};

class JsonVisitor {
  public:
    JsonVisitor(const std::string &name, std::string key, nlohmann::json &j)
        : m_name(name), m_key(key), m_json(j) {
        m_parts = utilities::split(name, "/");
    }

    template <typename T>
    void _setKey(size_t index, nlohmann::json &_p, const T &key) const {
        if (index == m_parts.size()) {
            _p[m_key] = key;
        } else {
            _setKey(index + 1, _p[m_parts[index]], key);
        }
    }

    template <typename T> void operator()(const T &value) const {
        assert(m_parts.size() > 0);
        _setKey(0, m_json, value);
    }

  private:
    std::string m_name, m_key;
    nlohmann::json &m_json;
    std::vector<std::string> m_parts;
};

template <> void JsonVisitor::operator()(const monostate &) const {}

template <class UnaryFunction>
void recursive_iterate(const nlohmann::json &j, std::string path,
                       UnaryFunction f) {
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
    StringMatch smatch(_part);
    auto it = m_properties.begin();
    const auto end = m_properties.end();
    for (; it != end; ++it) {
        if (smatch.match(it->first)) {
            ret.emplace(it->first, it->second);
        }
    }
    return ret;
}

std::string repository::debug(const mapType &_m) {
    std::stringstream _ss;
    _ss << "map<property>"
        << "\n";
    for (mapType::const_iterator it = _m.begin(), end = _m.end(); it != end;
         ++it) {
        for (auto &v : it->second->get()) {
            _ss << it->first << " : ";
            mapbox::util::apply_visitor(DebugVisitor(_ss, v.first), v.second);
        }
    }
    _ss << "End of map"
        << "\n";
    return _ss.str();
}

std::string repository::stringify(const mapType &_m, size_t spaces) const {
    nlohmann::json root;
    for (mapType::const_iterator it = _m.begin(), end = _m.end(); it != end;
         ++it) {
        for (auto &v : it->second->get()) {
            mapbox::util::apply_visitor(JsonVisitor(it->first, v.first, root),
                                        v.second);
        }
    }
    return root.dump(spaces);
}

std::string repository::stringify(const repository::StringMatch &_m,
                                  size_t spaces) const {
    nlohmann::json root;
    for (auto it = m_properties.begin(), end = m_properties.end();
    		it != end; ++it) {
        if (_m.match(it->first)) {
            for (auto &v : it->second->get()) {
                mapbox::util::apply_visitor(
                    JsonVisitor(it->first, v.first, root), v.second);
            }
        }
    }
    return root.dump(spaces);
}

std::string repository::propName(const std::string &name) const {
    //    std::string path = name.substr(0, m_repName.length()) != m_repName
    //                           ? m_repName + "/" + name
    //                           : name;
    std::string path = name;
    path.erase(std::unique(path.begin(), path.end(), both_slashes()),
               path.end());
    return path;
}

bool repository::set(const std::string &_name,
                     const property::property_base &p) {
    std::lock_guard<std::mutex> lock(m_lock);
    /*set works w/wo omitting repository name */
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

property &repository::link(const std::string &name, property &_p) {
    std::lock_guard<std::mutex> lock(m_lock);
    std::string cname = propName(name);
    std::pair<mapType::iterator, bool> ret =
        m_properties.emplace(cname, nullptr);
    _p.set(name, *this);

    if (false == ret.second) {
        // insert failed, set existing entry if allowed by link policy
        if (true == link_policy()) {
             set(ret.first, _p);
        }
    } else {
        ret.first->second = mappedType(&_p, [](property *) {});
    }

    return *ret.first->second;
}

property &repository::create(const std::string &name, const property &_cp) {
    std::lock_guard<std::mutex> lock(m_lock);
    std::string cname = propName(name);
    std::pair<mapType::iterator, bool> ret =
        m_properties.emplace(cname, nullptr);

    if (true == ret.second) {
        ret.first->second = std::make_shared<property>(_cp);
        ret.first->second->set(name, *this); // TODO name or cname
        //FIXME log_inst.debug(TAG, "create {}", ret.first->second->name());
    }

    return *ret.first->second;
}

bool repository::unlink(const std::string &name) {
    std::lock_guard<std::mutex> lock(m_lock);
    bool isErased = m_properties.erase(propName(name)) != 0;
    if (isErased) {
    	notify(name);
    }
    return isErased;
}

void repository::parse(const std::string &c) {
	append(c);
}

void repository::append(const std::string& _json) {
	#if (defined(__cpp_exceptions))
	    try {
	        nlohmann::json re = nlohmann::json::parse(_json);
	#else
	    nlohmann::json re = nlohmann::json::parse(_json, nullptr, false, true);
	    // allow_exceptions=false, ignore_comments = true
	    if (!re.is_discarded()) {
	#endif
	        std::string path;
	        recursive_iterate(
	            re, path,
	            [this](std::string ipath, nlohmann::json::const_iterator it) {
	                std::string propPath = propName(ipath);
	                std::stringstream ss;
	                ss << it.value();
	                log_inst.debug(TAG, "{} : {} < {}", propPath, it.key(),
	                               ss.str());
	                property &_p = create(propPath, property());

	                if (it.value().is_boolean()) {
	                    _p[it.key()] = it->get<bool>();
	                } else if (it.value().is_number_integer()) {
	                    _p[it.key()] = it->get<int>();
	                } else if (it.value().is_number_float()) {
	                    _p[it.key()] = it->get<double>();
	                } else if (it.value().is_string()) {
	                    _p[it.key()] = it->get<std::string>();
	                } else {
	                    assert(false);
	                }
	            });
	#if (defined(__cpp_exceptions))
	    } catch (const std::exception &e) {
	        log_inst.error(TAG, "exception on create {}\n", e.what());
	    };
	#else
	    }
	#endif
}

void repository::remove(const std::string& _json) {
#if (defined(__cpp_exceptions))
    try {
        nlohmann::json re = nlohmann::json::parse(_json);
#else
    nlohmann::json re = nlohmann::json::parse(_json, nullptr, false, true);
    // allow_exceptions=false, ignore_comments = true
    if (!re.is_discarded()) {
#endif
        std::string path;
        recursive_iterate(
            re, path,
            [this](std::string ipath, nlohmann::json::const_iterator it) {
                std::string propPath = propName(ipath);

                bool ret = unlink(propPath);
                log_inst.debug(TAG, "delete {} : {}", propPath, ret);
            });
#if (defined(__cpp_exceptions))
    } catch (const std::exception &e) {
        log_inst.error(TAG, "exception on remove {}\n", e.what());
    };
#else
    }
#endif

}

property &repository::operator[](const std::string &key) {
    return create(key, property());
}

property &repository::operator[](std::string &&key) {
    return create(key, property());
}

bool repository::StringMatch::match(const std::string str) const{
    bool ret = true;
    std::vector<std::string> m_parts = utilities::split(str, "/");
    for (size_t i = 0; (i < m_keys.size()) && (i < m_parts.size()); i++) {
        if ((m_keys[i][0] == '*') || (m_keys[i].length() == 0) ||
            (m_keys[i] == m_parts[i])) {
            continue;
        } else {
            ret = false;
            break;
        }
    }
    return ret;
}

repository::StringMatch::StringMatch(const std::string &key) {
    m_keys = utilities::split(key, "/");
}

repository::StringMatch::StringMatch(std::string &&key) {
    m_keys = utilities::split(key, "/");
}

void repository::notify(const std::string &s) {
	for (auto &i : m_notify) {
		StringMatch m(i.first);
		if (m.match(s)) {
			i.second(s);
		}
	}
}

void repository::addNotify(const keyType& key, notifyFuncType &&func) {
	m_notify.emplace_back(key, func);
}
