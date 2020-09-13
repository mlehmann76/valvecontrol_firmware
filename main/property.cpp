/*
 * property.cpp
 *
 *  Created on: 09.09.2020
 *      Author: marco
 */

#include "repository.h"
#include "property.h"

property& property::reg(const std::string &name, repository &_repo) {
	set(name, _repo);
	m_repository->reg(name, *this);
	return *this;
}

property& property::set(const property_base &p) {
	if (write_hook != nullptr) {
		write_hook(p);
	}

	for (auto &v : p) {
		m_pProperty.emplace(v.first, v.second).first->second = v.second;
	}

	if (m_repository != nullptr)
		m_repository->onSetNotify(m_name);

	return *this;
}

property& property::set(const key_type &name, const repository &_rep) {
	m_name = name;
	m_repository = &const_cast<repository&>(_rep);
	return *this;
}

property& property::set(const key_type &_n, base_type &&_b) {
	return set({{_n, _b}});
}
