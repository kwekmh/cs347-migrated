#include "service.h"

Service::Service(int service_identifier) {
  this->m_service_identifier = service_identifier;
}

int Service::GetServiceIdentifier() {
  return this->m_service_identifier;
}

void Service::SetServiceIdentifier(int service_identifier) {
  this->m_service_identifier = service_identifier;
}

int Service::GetLocalSocket() {
  return this->m_local_socket;
}

void Service::SetLocalSocket(int local_socket) {
  this->m_local_socket = local_socket;
}

void Service::AddClient(int client_identifier, StateData *data) {
  this->m_clients[client_identifier] = data;
}

std::unordered_map<int, StateData *> Service::GetClients() {
  return this->m_clients;
}
