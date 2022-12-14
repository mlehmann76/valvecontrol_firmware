/*
 * HttpRequest.h
 *
 *  Created on: 05.08.2020
 *      Author: marco
 */

#ifndef COMPONENTS_HTTP_HTTPREQUEST_H_
#define COMPONENTS_HTTP_HTTPREQUEST_H_

#include <string>
#include <unordered_map>
#include <vector>

class Socket;

namespace http {

class HttpRequest {
  public:
    enum Method { NONE, GET, HEAD, POST, PUT, DELETE, CONNECT, OPTIONS, TRACE };

    enum ParseResult { PARSE_OK, PARSE_NODATA, PARSE_ERROR };

    using Header = std::unordered_map<std::string, std::string>;
    using MethodMapType = std::unordered_map<std::string, enum Method>;
    using MethodStringType = std::unordered_map<enum Method, std::string>;
    using ReqPairType = std::pair<std::string, std::string>;

  private:
    static const MethodMapType MethodMap;
    static const MethodStringType s_MethodString;
    static constexpr const char *LineEnd = "\r\n";

  public:
    HttpRequest(Socket *_s);
    virtual ~HttpRequest();
    HttpRequest(const HttpRequest &other) = delete;
    HttpRequest(HttpRequest &&other) = delete;
    HttpRequest &operator=(const HttpRequest &other) = delete;
    HttpRequest &operator=(HttpRequest &&other) = delete;

    bool hasMethod(Method m) const;
    const std::string &method() const { return m_method; }
    const std::string &path() const { return m_path; }
    const std::string &version() const { return m_version; }
    const std::string &body() const { return m_body; }
    const Header &header() const { return m_header; }
    Socket *socket() const { return m_socket; }

    static ReqPairType splitHeaderLine(const std::string &line);
    static std::vector<std::string> split(const std::string &s,
                                          const std::string &seperator);

    ParseResult parse();

  private:
    void analyze(const std::string &);

    Socket *m_socket;
    std::string m_method;
    std::string m_path;
    std::string m_version;
    std::string m_body;
    Header m_header;
};

} /* namespace http */
#endif /* COMPONENTS_HTTP_HTTPREQUEST_H_ */
