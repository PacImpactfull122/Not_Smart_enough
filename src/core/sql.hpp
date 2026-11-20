#pragma once

namespace opencode::sql {

inline constexpr char kCreateTables[] = R"SQL(
CREATE TABLE IF NOT EXISTS session (
  id          TEXT PRIMARY KEY,
  agent       TEXT NOT NULL,
  created_at  TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS message (
  session_id  TEXT NOT NULL REFERENCES session(id),
  id          TEXT NOT NULL,
  seq         BIGINT NOT NULL,
  role        TEXT NOT NULL,
  text        TEXT NOT NULL,
  PRIMARY KEY (session_id, seq)
);
)SQL";

inline constexpr char kInsertSession[] =
    "INSERT INTO session (id, agent) VALUES ($1, $2) ON CONFLICT (id) DO NOTHING";

inline constexpr char kInsertMessage[] =
    "INSERT INTO message (session_id, seq, role, text) VALUES ($1, $2, $3, $4)";

inline constexpr char kSelectMessages[] =
    "SELECT id, seq, role, text FROM message WHERE session_id = $1 ORDER BY seq ASC";

inline constexpr char kNextSeq[] =
    "SELECT COALESCE(MAX(seq), 0) + 1 FROM message WHERE session_id = $1";

}  // namespace opencode::sql
