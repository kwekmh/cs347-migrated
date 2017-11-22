#ifndef MIGRATED_SERVICE_H_
#define MIGRATED_SERVICE_H_

#include <unordered_map>

#include "statedata.h"

class Service {
  int m_service_identifier;
  int m_local_socket;
  std::unordered_map<int, StateData *> m_clients;

public:
  Service(int service_identifier);
  int GetServiceIdentifier();
  void SetServiceIdentifier(int service_identifier);
  int GetLocalSocket();
  void SetLocalSocket(int local_socket);
  void AddClient(int client_identifier, StateData *data);
  std::unordered_map<int, StateData *> GetClients();
};

#endif
