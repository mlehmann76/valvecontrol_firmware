/*
 * property.cpp
 *
 *  Created on: 09.09.2020
 *      Author: marco
 */

#include "property.h"
#include "repository.h"
#include <optional>

property &property::reg(const std::string &name, repository &_repo) {
    set(name, _repo);
    m_repository->link(name, *this);
    return *this;
}

property &property::set(const property_base &p) {
    property _temp(p);
    _temp.m_name = this->m_name;
    std::optional<property> ret = {};

    if (write_hook != nullptr) {
        ret = write_hook(_temp);
    }

    if (ret) {
        for (auto &v : *ret)
            m_pProperty.emplace(v.first, v.second).first->second = v.second;
    } else {
        for (auto &v : p)
            m_pProperty.emplace(v.first, v.second).first->second = v.second;
    }
    notify();

    return *this;
}

property &property::set(const key_type &name, const repository &_rep) {
    m_name = name;
    m_repository = &const_cast<repository &>(_rep);
    return *this;
}

void property::notify() {
    if (m_repository != nullptr)
        m_repository->onSetNotify(m_name);
}
