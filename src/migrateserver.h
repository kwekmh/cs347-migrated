#ifndef MIGRATED_MIGRATESERVER_H_
#define MIGRATED_MIGRATESERVER_H_

#include <vector>
#include <unordered_map>
#include <memory>
#include <netinet/in.h>

#include "connection.h"

class MigrateServer {
  std::string m_identifier;
  std::vector<int> m_services;
  std::unordered_map<int, std::vector<Connection *> *> m_connections;
  int m_counter;

public:
  MigrateServer(std::string identifier);
  ~MigrateServer();
  std::string GetIdentifier();
  void SetIdentifier(std::string identifier);
  void AddService(int service);
  bool HasService(int service);
  std::vector<int> GetServices();
  void AddConnection(int service, Connection *conn);
  void AddOrUpdateConnection(int service_identifier, int connection_identifier, char *state, int state_size);
  std::vector<Connection *> * GetConnections(int service);
  int GetCounter();
  void SetCounter(int counter);
  void IncrementCounter();
};

#endif
