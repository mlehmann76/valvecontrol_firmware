/*
 * FileHandler.cpp
 *
 *  Created on: 29.10.2020
 *      Author: marco
 */

#include "FileHandler.h"
#include <fstream>
#include <iostream>
#include <memory.h>

using namespace std::string_literals;

namespace http {

FileHandler::FileHandler(const std::string &_method, const std::string &_path,
                         const std::string &_base)
    : RequestHandlerBase(_method, _path), m_base(_base) {}

FileHandler::~FileHandler() {}

bool FileHandler::doGet(const HttpRequest &_req, HttpResponse &_res) {
    bool ret = false;
    bool compressed = false;
    const std::string _p = _req.path();
    const std::string _filename =
        _p[_p.length() - 1] == '/' ? m_base + "/index.html"s : m_base + _p;
    std::ifstream ifs(_filename, std::ifstream::in);

    //fmt::print("file request: {:s}\n",_filename);

    if (!ifs) {
        // not found, try gzipped filename
        const std::string _cfilename = _filename + ".gz";
        ifs = std::ifstream(_cfilename, std::ifstream::in);
        compressed = true;
    }

    if (ifs) {
        // get length of file:
        ifs.seekg(0, ifs.end);
        int length = ifs.tellg();
        ifs.seekg(0, ifs.beg);
        //fmt::print("file length: {:d}\n",length);
        _res.setResponse(HttpResponse::HTTP_200);
        _res.setContentType(_res.nameToContentType(_filename));
        _res.setContentEncoding(compressed ? HttpResponse::CT_ENC_GZIP
                                           : HttpResponse::CT_ENC_IDENTITY);

        _sendFile(_res, ifs, length);
        _res.reset();
        ret = true;
    } else {
        _res.setResponse(HttpResponse::HTTP_404);
        _res.endHeader();
        _res.send(nullptr, 0);
        _res.reset();
    }
    return ret;
}

void FileHandler::_sendFile(HttpResponse &_res, std::ifstream &ifs,
                            size_t length) {
    const size_t blockl = length < s_maxBlockSize ? length : s_maxBlockSize;
    std::unique_ptr<char[]> buffer = std::make_unique<char[]>(s_maxBlockSize);
    //fmt::print("file length: {:d}\n",length);
    if (length < s_maxBlockSize) {
        // simple file send
        ifs.read(buffer.get(), blockl);
        _res.send(buffer.get(), blockl);
    } else {
        // sending in chunks
        while (ifs) {
            // read data as a block:
        	size_t readl = length > blockl ? blockl : length;
        	//fmt::print("read length: {:d}, remain: {:d}\n",readl, length-readl);
            ifs.read(buffer.get(), readl);
            _res.send_chunk(buffer.get(), readl);
            if (length > readl) {
            	length -= readl;
            } else {
            	break;
            }
        }
        _res.send_chunk(buffer.get(), 0); /// finalize chunk
        ifs.close();
    }
}
bool FileHandler::doPut(const HttpRequest &_req, HttpResponse &_res) {
    const std::string _p = _req.path();
    const std::string _filename =
        _p[_p.length() - 1] == '/' ? m_base + "/index.html"s : m_base + _p;
    std::ofstream ifs(_filename, std::ifstream::binary);

    // TODO write data from request

    return false;
}

bool FileHandler::handle(const HttpRequest &_req, HttpResponse &_res) {
	_res.reset();
    if (_req.hasMethod(HttpRequest::GET)) {
        return doGet(_req, _res);
    } else if (_req.hasMethod(HttpRequest::PUT)) {
        return doPut(_req, _res);
    }
    return false;
}

} /* namespace http */
