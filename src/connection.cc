#include "connection.h"

Connection::Connection(int service_identifier, int connection_identifier, char *state) {
  this->m_service_identifier = service_identifier;
  this->m_connection_identifier = connection_identifier;
  this->m_state = state;
}

int Connection::GetSupportGroup() {
  return this->m_support_group;
}

int Connection::GetServiceIdentifier() {
  return this->m_service_identifier;
}

void Connection::SetServiceIdentifier(int service_identifier) {
  this->m_service_identifier = service_identifier;
}

int Connection::GetConnectionIdentifier() {
  return this->m_connection_identifier;
}

void Connection::SetConnectionIdentifier(int connection_identifier) {
  this->m_connection_identifier = connection_identifier;
}

char * Connection::GetState() {
  return this->m_state;
}

void Connection::SetState(char *state) {
  char *old_state = this->m_state;
  this->m_state = state;
  delete [] old_state;
}
