/*
 * Json.h
 *
 *  Created on: 14.08.2020
 *      Author: marco
 */

#ifndef MAIN_JSON_H_
#define MAIN_JSON_H_

#include <cJSON.h>
#include <string>

class Json {
  public:
    // specialized constructors
    Json() : m_object(cJSON_CreateObject()) {}
    explicit Json(bool _val) : m_object(cJSON_CreateBool(_val)) {}
    explicit Json(double _val) : m_object(cJSON_CreateNumber(_val)) {}
    explicit Json(const char *_val) : m_object(cJSON_CreateString(_val)) {}

    virtual ~Json() { clean(); }

    Json(const Json &other)
        : m_object(other.m_object), m_isParent(other.m_isParent) {}

    Json(Json &&other) {
        if (this != &other) {
            m_object = other.m_object;
            m_isParent = other.m_isParent;
            other.m_object = nullptr;
        }
    }

    Json &operator=(const Json &other) {
        m_object = other.m_object;
        return *this;
    }

    Json &operator=(Json &&other) = delete;
    //	{
    //		m_object = other.m_object;
    //		m_isParent = other.m_isParent;
    //		other.m_object = nullptr;
    //		return *this;
    //	}

    Json &parse(const char *value) {
        clean();
        m_object = cJSON_Parse(value);
        return *this;
    }

    Json operator[](const char *value) const {
        return {cJSON_GetObjectItem(const_cast<cJSON *>(m_object), value)};
    }

    Json item(const char *value) {
        return {cJSON_GetObjectItem(const_cast<cJSON *>(m_object), value)};
    }

    bool IsInvalid() const { return cJSON_IsInvalid(m_object); }
    bool IsFalse() const { return cJSON_IsFalse(m_object); }
    bool IsTrue() const { return cJSON_IsTrue(m_object); }
    bool IsBool() const { return cJSON_IsBool(m_object); }
    bool IsNull() const { return cJSON_IsNull(m_object); }
    bool IsNumber() const { return cJSON_IsNumber(m_object); }
    bool IsString() const { return cJSON_IsString(m_object); }
    bool IsArray() const { return cJSON_IsArray(m_object); }
    bool IsObject() const { return cJSON_IsObject(m_object); }
    bool IsRaw() const { return cJSON_IsRaw(m_object); }

    Json addItem(const char *name, Json &&_i) {
        cJSON_AddItemToObject(m_object, name, _i.m_object);
        _i.m_isParent = false;
        return *this;
    }

    Json addObject(const char *name) {
        return {cJSON_AddObjectToObject(m_object, name)};
    }

    size_t arraySize() const { return cJSON_GetArraySize(m_object); }
    Json arrayItem(size_t i) const { return {cJSON_GetArrayItem(m_object, i)}; }

    const char *string() const { return cJSON_GetStringValue(m_object); }

    double number() const {
        return (m_object != nullptr) ? m_object->valuedouble : 0;
    }

    bool valid() const { return m_object != nullptr; }

    std::string dump() {
        char *_p = cJSON_Print(m_object);
        std::string _s(_p != nullptr ? _p : "");
        free(_p);
        return _s;
    }

  private:
    Json(cJSON *_c) : m_object(_c), m_isParent(false) {}

    void clean() {
        if (m_isParent && m_object != nullptr) {
            cJSON_Delete(m_object);
        }
    }

    cJSON *m_object;
    bool m_isParent = true;
};

#endif /* MAIN_JSON_H_ */
