/*
 * HttpResponse.h
 *
 *  Created on: 06.08.2020
 *      Author: marco
 */

#ifndef COMPONENTS_HTTP_HTTPRESPONSE_H_
#define COMPONENTS_HTTP_HTTPRESPONSE_H_

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace http {

class HttpRequest;

class HttpResponse {
    static constexpr const char *HTTP_Ver = "HTTP/1.1";
    static constexpr const char *LineEnd = "\r\n";

  public:
    enum ResponseCode {
        HTTP_200,
        HTTP_201,
        HTTP_204,
        HTTP_400,
        HTTP_401,
        HTTP_404,
        HTTP_500,
        HTTP_503,
        HTTP_511
    };

    enum ContentType {
        CT_TEXT_PLAIN,
        CT_APP_JSON,
        CT_APP_OCSTREAM,
        CT_APP_PDF,
        CT_APP_JAVASCRIPT,
		CT_TEXT_CSS,
        CT_TEXT_HTML,
		CT_IMAGE_ICO,
		CT_IMAGE_PNG
    };

    enum ContentEncoding { CT_ENC_IDENTITY, CT_ENC_GZIP };

    using ResponseMapType = std::map<ResponseCode, std::string_view>;
    using ContentTypeMapType = std::map<ContentType, std::string_view>;
    using FileContentMapType = std::map<std::string_view, ContentType>;

    static ResponseMapType s_respMap;
    static ContentTypeMapType s_ctMap;
    static FileContentMapType s_fcmap;

    HttpResponse(HttpRequest &_s);
    virtual ~HttpResponse();
    HttpResponse(const HttpResponse &other) = delete;
    HttpResponse(HttpResponse &&other) = delete;
    HttpResponse &operator=(const HttpResponse &other) = delete;
    HttpResponse &operator=(HttpResponse &&other) = delete;

    void setResponse(ResponseCode _c);
    void setHeader(const std::string &, const std::string &);
    void setHeaderDefaults();
    void setContentType(ContentType _c);
    void setContentEncoding(ContentEncoding _e);
    ContentType nameToContentType(const std::string &_name);
    void endHeader();

    void send(const std::string &);
    void send_chunk(const std::string &);

    void send(const char *_buf, size_t _s);
    void send_chunk(const char *_buf, size_t _s);

    void reset();

  private:
    void headerAddDate();
    void headerAddServer();
    void headerAddEntries();
    void headerAddStatusLine();
    std::string getTime();

    ResponseCode m_respCode;
    std::string m_header;
    std::vector<std::string> m_headerEntries;
    HttpRequest *m_request;
    bool m_headerFinished;
    bool m_firstChunkSent;
};

} /* namespace http */

#endif /* COMPONENTS_HTTP_HTTPRESPONSE_H_ */
