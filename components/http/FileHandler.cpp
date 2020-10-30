/*
 * FileHandler.cpp
 *
 *  Created on: 29.10.2020
 *      Author: marco
 */

#include "FileHandler.h"
#include <fmt/printf.h>
#include <fstream>
#include <iostream>

using namespace std::string_literals;

namespace http {

FileHandler::FileHandler(const std::string &_method, const std::string &_path,
                         const std::string &_base)
    : RequestHandlerBase(_method, _path), m_base(_base) {}

FileHandler::~FileHandler() {}

bool FileHandler::doGet(const HttpRequest &_req, HttpResponse &_res) {
    bool ret = false;
    const std::string _p = _req.path();
    const std::string _filename =
        _p[_p.length() - 1] == '/' ? m_base + "/index.html"s : m_base + _p;
    std::ifstream ifs(_filename, std::ifstream::binary);
    // fmt::print("reading {}, good:{}, fail:{},
    // bad:{}\n",_filename,ifs.good(),ifs.fail(),ifs.bad());
    if (ifs) {
        // get length of file:
        ifs.seekg(0, ifs.end);
        int length = ifs.tellg();
        ifs.seekg(0, ifs.beg);
        // fmt::print("file length: {:d}\n",length);
        size_t blockl = length < s_maxBlockSize ? length : s_maxBlockSize;
        _res.setResponse(HttpResponse::HTTP_200);
        _res.setContentType(_res.nameToContentType(_filename));
        char *buffer = new char[blockl];
        if (length < s_maxBlockSize) {
            // simple file send
            ifs.read(buffer, blockl);
            _res.send(buffer, blockl);
            _res.reset();
        } else {
            // sending in chunks
            while (ifs) {
                // read data as a block:
                ifs.read(buffer, blockl);
                _res.send_chunk(buffer, ifs.gcount());
            }
            _res.send_chunk(buffer, 0); /// finalize chunk
            ifs.close();
        }
        delete[] buffer;
        ret = true;
    } else {
        _res.setResponse(HttpResponse::HTTP_404);
        _res.endHeader();
        _res.send(nullptr, 0);
        _res.reset();
    }
    return ret;
}

bool FileHandler::handle(const HttpRequest &_req, HttpResponse &_res) {
    bool ret = false;
    if (_req.method() == "GET") {
        ret = doGet(_req, _res);
    }
    return ret;
}

} /* namespace http */
