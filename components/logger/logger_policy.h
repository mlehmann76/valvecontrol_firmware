/*
 * log_policy.h
 *
 *  Created on: 12.10.2020
 *      Author: marco
 */

#ifndef COMPONENTS_LOGGER_LOGGER_POLICY_H_
#define COMPONENTS_LOGGER_LOGGER_POLICY_H_

#include <fstream>
#include <memory>

namespace logger {

/**
 *
 */
struct DefaultLogPolicy {
  void open(const std::string &) {}
  void close() {}
  void write(const std::string &) {}
};
/**
 *
 */
struct FileLogPolicy {
  FileLogPolicy() : out_stream(new std::ofstream) {}
  void open(const std::string &name);
  void close();
  void write(const std::string &msg);
  ~FileLogPolicy();
  std::unique_ptr<std::ofstream> out_stream;
};

inline void FileLogPolicy::open(const std::string &name) {
  out_stream->open(name, std::ios::out);
}

inline void FileLogPolicy::close() { out_stream->close(); }

inline void FileLogPolicy::write(const std::string &msg) {
  out_stream->write(msg.c_str(), msg.length());
}

inline FileLogPolicy::~FileLogPolicy() { close(); }

} // namespace logger

#endif /* COMPONENTS_LOGGER_LOGGER_POLICY_H_ */
