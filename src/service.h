#ifndef MIGRATED_SERVICE_H_
#define MIGRATED_SERVICE_H_

#include <unordered_map>

#include "statedata.h"

class Service {
  int m_service_identifier;
  std::unordered_map<int, StateData *> m_clients;

public:
  Service(int service_identifier);
  void AddClient(int client_identifier, StateData *data);
  std::unordered_map<int, StateData *> GetClients();
};

#endif
