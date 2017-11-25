#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <unordered_map>

#include "configuration.h"

Configuration::Configuration(std::string filename) {
  std::ifstream file;
  file.open(filename);

  std::string line;

  std::string key;
  std::string value;

  while (std::getline(file, line)) {
    std::istringstream is_line(line);
    if (std::getline(is_line, key, '=') && std::getline(is_line, value)) {
      this->m_mappings[key] = value;
    }
  }
}

void Configuration::LoadFromFile(std::string filename) {
  std::ifstream file;
  file.open(filename);

  std::string line;

  std::string key;
  std::string value;

  while (std::getline(file, line)) {
    std::istringstream is_line(line);
    if (std::getline(is_line, key, '=') && std::getline(is_line, value)) {
      this->m_mappings[key] = value;
    }
  }
}

std::unordered_map<std::string, std::string> Configuration::GetMappings() {
  return this->m_mappings;
}

std::string Configuration::Get(std::string key) {
  if (this->m_mappings.find(key) != this->m_mappings.end()) {
    return this->m_mappings[key];
  } else {
    return std::string();
  }
}

bool Configuration::HasKey(std::string key) {
  return this->m_mappings.find(key) != this->m_mappings.end();
}

void Configuration::PrintMappings() {
  std::cout << "Configuration mappings: " << std::endl;
  std::unordered_map<std::string, std::string>::const_iterator it;
  for (it = this->m_mappings.begin(); it != this->m_mappings.end(); it++) {
    std::cout << it->first << "=" << it->second << std::endl;
  }
}
