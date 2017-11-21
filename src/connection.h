#ifndef MIGRATED_CONNECTION_H_
#define MIGRATED_CONNECTION_H_

#include <string>
#include <memory>

class Connection {
  int m_service_identifier;
  int m_connection_identifier;
  char *m_state;
  int m_state_size;
  int m_support_group;

public:
  Connection(int service_identifier, int m_connection_identifier, char *state, int state_size);
  int GetSupportGroup();

  int GetServiceIdentifier();
  void SetServiceIdentifier(int service_identifier);

  int GetConnectionIdentifier();
  void SetConnectionIdentifier(int connection_identifier);

  char * GetState();
  void SetState(char *state);

  int GetStateSize();
  void SetStateSize(int state_size);
};

#endif
