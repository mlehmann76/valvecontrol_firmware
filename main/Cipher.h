/*
 * Cipher.h
 *
 *  Created on: 01.01.2021
 *      Author: marco
 */

#ifndef MAIN_CIPHER_H_
#define MAIN_CIPHER_H_

#include <string>
#include <array>
#include <cassert>
#include <optional>
#include <sstream>

template<class T, size_t N, class V>
std::array<T, N> to_array(const V& v)
{
    assert(v.size() == N);
    std::array<T, N> d;
    std::copy( std::begin(v), std::end(v), std::begin(d) );
    return d;
}

class AES128Key {
public:
	static constexpr size_t BLOCK_LEN = 16;
	using base_type = unsigned char;
	using key_type = std::array<base_type,BLOCK_LEN>;

	AES128Key() : m_key() {}
	AES128Key(const key_type &_k) : m_key(_k) {}
	AES128Key(key_type &&_k) : m_key(std::move(_k)) {}
	AES128Key(const std::string& _k) : m_key(to_array<unsigned char,16>(_k)) {}
	AES128Key(const AES128Key &other) = default;
	AES128Key(AES128Key &&other) = default;
	AES128Key& operator=(const AES128Key &other) = default;
	AES128Key& operator=(AES128Key &&other) = default;
	//
	operator key_type() const { return m_key; }
	key_type operator()() const { return m_key; }

	std::string to_string() const {
		return std::string(std::begin(m_key), std::end(m_key));
	}

	std::string to_hex() const;

	size_t size() const { return 16; }
	static std::optional<AES128Key> genRandomKey(const std::string& pers);
private:
	key_type m_key;
};

class Cipher {
	static constexpr size_t BLOCK_LEN = 16;
	using base_type = unsigned char;
	using block_type = std::array<base_type,BLOCK_LEN>;
public:
	Cipher(const AES128Key& _key ) : m_key(_key) {}
	Cipher(AES128Key&& _key ) : m_key(std::move(_key)) {}
	virtual ~Cipher();
	Cipher(const Cipher &other) = default;
	Cipher(Cipher &&other) = default;
	Cipher& operator=(const Cipher &other) = default;
	Cipher& operator=(Cipher &&other) = default;
	//
	std::string encrypt(std::string _text) const;
	//
	std::string decrypt(std::string _crypt) const;

private:

	void encrypt(const block_type& in, block_type& out) const;
	void decrypt(const block_type& in, block_type& out) const;

	static std::string toString(const block_type& in);

	AES128Key m_key;
};

std::string toHex(const std::string& in) ;
std::string fromHex(const std::string& in) ;

#endif /* MAIN_CIPHER_H_ */
