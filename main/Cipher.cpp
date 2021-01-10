/*
 * Cipher.cpp
 *
 *  Created on: 01.01.2021
 *      Author: marco
 */

#include "Cipher.h"
#include "mbedtls/aes.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include <optional>
#include <iomanip>
#include <string.h>

std::string AES128Key::to_hex() const {
	std::stringstream _s;
	for (const auto &e : m_key) {
		_s << std::uppercase << std::setfill('0') << std::setw(2) << std::hex
				<< unsigned(e);
	}
	return _s.str();
}


Cipher::~Cipher() {
	// TODO Auto-generated destructor stub
}

std::string Cipher::encrypt(std::string _text) const {
	std::string ret;
	size_t len = _text.length();
	size_t idx = 0;
	block_type _out;

	while (len > BLOCK_LEN) {
		std::string _buf(BLOCK_LEN, 0);
		encrypt(to_array<base_type, BLOCK_LEN>(_text.substr(idx, BLOCK_LEN)),
				_out);
		ret.append(toString(_out));
		idx += BLOCK_LEN;
		len -= BLOCK_LEN;
	}

	if (len) {
		std::string _sub = _text.substr(idx, BLOCK_LEN);
		_sub.resize(BLOCK_LEN);
		encrypt(to_array<base_type, BLOCK_LEN>(_sub), _out);
		ret.append(toString(_out));
	}

	return ret;
}

std::string Cipher::decrypt(std::string _crypt) const {
	std::string ret;
	size_t len = _crypt.length();
	size_t idx = 0;
	block_type _out;

	while (len > BLOCK_LEN) {
		decrypt(to_array<base_type, BLOCK_LEN>(_crypt.substr(idx, BLOCK_LEN)), _out);
		ret.append(toString(_out));
		idx += BLOCK_LEN;
		len -= BLOCK_LEN;
	}

	if (len) {
		std::string _sub = _crypt.substr(idx, BLOCK_LEN);
		_sub.resize(BLOCK_LEN);
		decrypt(to_array<base_type, BLOCK_LEN>(_sub), _out);
		ret.append(toString(_out));
	}

	const size_t cLen = strnlen(ret.c_str(), _crypt.length());
	if (ret.length() != cLen) {
		ret.resize(cLen);
	}

	return ret;
}

void Cipher::encrypt(const block_type &in, block_type &out) const {
	// encrypt buffer of length 16 characters
	mbedtls_aes_context aes;

	mbedtls_aes_init(&aes);
	mbedtls_aes_setkey_enc(&aes, m_key().data(), m_key.size() * 8);
	mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, in.data(), out.data());
	mbedtls_aes_free(&aes);
}

void Cipher::decrypt(const block_type &in, block_type &out) const {
	// decrypt buffer of length 16 characters
	mbedtls_aes_context aes;

	mbedtls_aes_init(&aes);
	mbedtls_aes_setkey_dec(&aes, m_key().data(), m_key.size() * 8);
	mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_DECRYPT, in.data(), out.data());
	mbedtls_aes_free(&aes);
}

std::optional<AES128Key> AES128Key::genRandomKey(const std::string &pers) {
	mbedtls_ctr_drbg_context ctr_drbg;
	mbedtls_entropy_context entropy;
	key_type key;
	mbedtls_entropy_init(&entropy);

	mbedtls_ctr_drbg_init(&ctr_drbg);

	if (mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
			reinterpret_cast<const unsigned char*>(pers.c_str()), pers.length())
			!= 0) {
		return {};
	}

	if (mbedtls_ctr_drbg_random(&ctr_drbg, key.data(), BLOCK_LEN) != 0) {
		return {};
	}

	return {key};
}

std::string Cipher::toString(const block_type &in) {
	return std::string(std::begin(in), std::end(in));
}
