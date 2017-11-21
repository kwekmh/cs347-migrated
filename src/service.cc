#include "service.h"

Service::Service(int service_identifier) {
  this->m_service_identifier = service_identifier;
}

void Service::AddClient(int client_identifier, StateData *data) {
  this->m_clients[client_identifier] = data;
}

std::unordered_map<int, StateData *> Service::GetClients() {
  return this->m_clients;
}
