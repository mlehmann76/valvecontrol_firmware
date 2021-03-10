/*
 * repository.h
 *
 *  Created on: 28.08.2020
 *      Author: marco
 */

#ifndef MAIN_REPOSITORY_H_
#define MAIN_REPOSITORY_H_

#include "property.h"

#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>

#include <mapbox/variant.hpp>

struct DefaultLinkPolicy : std::false_type {};
struct ReplaceLinkPolicy : std::true_type {};
template <class T> struct tag { using type = T; };

class repository {

    using keyType = std::string;
    using mappedType = std::shared_ptr<property>;
    using mapType = std::map<keyType, mappedType>;
    using notifyFuncType = std::function<void(const std::string &s)>;
    using notifyType = std::pair<std::string, notifyFuncType>;

    struct link_policy_t {
        const bool value = false;
        constexpr link_policy_t(bool v) : value(v) {}
        constexpr bool operator()() const { return value; }
    };

public:
    struct StringMatch {
        std::vector<std::string> m_keys;
        StringMatch(const std::string &key);
        StringMatch(std::string &&key);
        bool match(const std::string str) const;
    };

  public:
    using iterator = mapType::iterator;
    using const_iterator = mapType::const_iterator;

    template <class linkPolicy>
    repository(const std::string &name,
               tag<linkPolicy> = tag<DefaultLinkPolicy>{})
        : m_repName(name), link_policy(linkPolicy::value) {}

    virtual ~repository() = default;

    /* link a existing property in the repository */
    property &link(const keyType &_name, property &_p);

    /* unlink the property */
    bool unlink(const keyType &_name);

    /* create a new property with starting value, if property already exists
     * this will return the existing property and do noting!!! */
    property &create(const keyType &_name, const property &_cp = {});

    /* create properties from json string */
    void append(const std::string& _json);

    /* delete properties from json string */
    void remove(const std::string& _json);

    /* set the property */
    virtual bool set(const keyType &name, const property::property_base &p);

    /* convert the property, return given value if not existing */
    template <typename T>
    T const get(const keyType &name, const std::string &key,
                const T &_default = T{}) {
        iterator _it = find(propName(name));
        if (_it != end() && (*_it->second).find(key) != (*_it->second).end()) {
            return mapbox::util::get_unchecked<T>(
                (*_it->second).find(key)->second);
        } else {
            return _default;
        }
    }

    /* create json representation of the repository */
    std::string stringify() const { return stringify(m_properties); }

    /* create json representation of a part the repository */
    std::string stringify(const mapType &_m, size_t spaces = 4) const;
    std::string stringify(const StringMatch &_m, size_t spaces = 4) const;

    std::string debug() { return debug(m_properties); }

    std::string debug(const mapType &_m);

    /* create a part the repository */
    mapType partial(const std::string &_part);

    /* parse a json string */
    void parse(const std::string &c);

    /* notify */
    void notify(const std::string &);
    void addNotify(const std::string& key, notifyFuncType &&func);
    //TODO void remNotify(const std::string& key);

    std::string name() const { return m_repName; }

    property &operator[](const keyType &key);
    property &operator[](keyType &&key);

    iterator begin() { return m_properties.begin(); }
    iterator end() { return m_properties.end(); }
    iterator find(const keyType &key) {
        return m_properties.find(propName(key));
    }
    const_iterator begin() const { return m_properties.begin(); }
    const_iterator end() const { return m_properties.end(); }
    const_iterator find(const keyType &key) const {
        return m_properties.find(propName(key));
    }

  protected:
    std::string propName(const std::string &name) const;
    bool set(mapType::iterator it, const property::property_base &p);

  private:
    std::string m_repName;
    mapType m_properties;
    link_policy_t link_policy;
    std::mutex m_lock;
    std::vector<notifyType> m_notify;
};

#endif /* MAIN_REPOSITORY_H_ */
