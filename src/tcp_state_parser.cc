#include <string>
#include <sstream>

#include "tcp_state_parser.h"

TcpStateParser::TcpStateParser(std::string tcp_state_data) {
  std::string seq_number_str;
  std::string mss_str;

  std::istringstream is_tcp_state(tcp_state_data);

  if (std::getline(is_tcp_state, seq_number_str, ' ') && std::getline(is_tcp_state, mss_str, ' ')) {
    this->m_seq_number = std::stoi(seq_number_str);
    this->m_mss = std::stoi(mss_str);
  }
}

int TcpStateParser::GetSeqNumber() {
  return this->m_seq_number;
}

int TcpStateParser::GetMss() {
  return this->m_mss;
}
