#include "core/session_store.hpp"

#include <libpq-fe.h>

#include <stdexcept>
#include <string>
#include <vector>

#include "core/schema.hpp"
#include "core/sql.hpp"

namespace opencode {

struct SessionStore::Impl {
  PGconn* conn;
};

namespace {

void check(PGconn* conn, PGresult* result, ExecStatusType expected) {
  if (result == nullptr) {
    throw std::runtime_error(PQerrorMessage(conn));
  }
  if (PQresultStatus(result) != expected) {
    std::string message = PQresultErrorMessage(result);
    PQclear(result);
    throw std::runtime_error(message);
  }
}

void exec(PGconn* conn, const char* query, int nparams, const char* const* values) {
  PGresult* result = PQexecParams(conn, query, nparams, nullptr, values, nullptr, nullptr, 0);
  check(conn, result, PGRES_COMMAND_OK);
  PQclear(result);
}

Seq next_seq(PGconn* conn, const SessionId& id) {
  const char* values[] = {id.c_str()};
  PGresult* result = PQexecParams(conn, sql::kNextSeq, 1, nullptr, values, nullptr, nullptr, 0);
  check(conn, result, PGRES_TUPLES_OK);
  Seq seq = static_cast<Seq>(std::stoull(PQgetvalue(result, 0, 0)));
  PQclear(result);
  return seq;
}

}  // namespace

SessionStore::SessionStore(std::string conninfo) : impl_(new Impl{PQconnectdb(conninfo.c_str())}) {
  if (PQstatus(impl_->conn) != CONNECTION_OK) {
    std::string message = PQerrorMessage(impl_->conn);
    PQfinish(impl_->conn);
    delete impl_;
    throw std::runtime_error(message);
  }
}

SessionStore::~SessionStore() {
  PQfinish(impl_->conn);
  delete impl_;
}

void SessionStore::migrate() {
  PGresult* result = PQexec(impl_->conn, sql::kCreateTables);
  check(impl_->conn, result, PGRES_COMMAND_OK);
  PQclear(result);
}

void SessionStore::create(const SessionId& id, const AgentId& agent) {
  const char* values[] = {id.c_str(), agent.c_str()};
  exec(impl_->conn, sql::kInsertSession, 2, values);
}

void SessionStore::append(const SessionId& id, const Message& message) {
  Seq seq = message.seq == 0 ? next_seq(impl_->conn, id) : message.seq;
  std::string seq_text = std::to_string(seq);
  const char* values[] = {id.c_str(), seq_text.c_str(), message.role.c_str(), message.text.c_str()};
  exec(impl_->conn, sql::kInsertMessage, 4, values);
}

std::vector<Message> SessionStore::list(const SessionId& id) {
  const char* values[] = {id.c_str()};
  PGresult* result =
      PQexecParams(impl_->conn, sql::kSelectMessages, 1, nullptr, values, nullptr, nullptr, 0);
  check(impl_->conn, result, PGRES_TUPLES_OK);

  std::vector<Message> messages;
  int rows = PQntuples(result);
  messages.reserve(static_cast<std::size_t>(rows));
  for (int i = 0; i < rows; ++i) {
    messages.push_back(Message{PQgetvalue(result, i, 0),
                               static_cast<Seq>(std::stoull(PQgetvalue(result, i, 1))),
                               PQgetvalue(result, i, 2),
                               PQgetvalue(result, i, 3)});
  }
  PQclear(result);
  return messages;
}

}  // namespace opencode
