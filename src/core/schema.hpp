#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace opencode {

using SessionId = std::string;
using MessageId = std::string;
using AgentId = std::string;
using Seq = std::uint64_t;

struct TokenBudget {
  std::size_t total = 0;
  std::size_t reserved = 0;
};

struct Message {
  MessageId id;
  Seq seq = 0;
  std::string role;
  std::string text;
};

struct ToolSpec {
  std::string name;
  bool writable = false;
  bool executes_code = false;
};

struct ToolSnapshot {
  AgentId agent;
  std::vector<ToolSpec> tools;
};

struct Session {
  SessionId id;
  AgentId agent;
  std::vector<Message> messages;
};

}  // namespace opencode
