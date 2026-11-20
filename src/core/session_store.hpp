#pragma once

#include <string>
#include <vector>

#include "core/schema.hpp"

namespace opencode {

class SessionStore {
 public:
  explicit SessionStore(std::string conninfo);
  ~SessionStore();

  SessionStore(const SessionStore&) = delete;
  SessionStore& operator=(const SessionStore&) = delete;

  void migrate();
  void create(const SessionId& id, const AgentId& agent);
  void append(const SessionId& id, const Message& message);
  std::vector<Message> list(const SessionId& id);

 private:
  struct Impl;
  Impl* impl_;
};

}  // namespace opencode
