#ifndef MIGRATED_TCP_STATE_PARSER_H_
#define MIGRATED_TCP_STATE_PARSER_H_

#include <string>

class TcpStateParser {
  int m_seq_number;
  int m_mss;
public:
  TcpStateParser(std::string tcp_state_data);
  int GetSeqNumber();
  int GetMss();
};

#endif
