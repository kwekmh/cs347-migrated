#ifndef MIGRATED_CONFIGURATION_H_
#define MIGRATED_CONFIGURATION_H_

#include <string>
#include <unordered_map>

class Configuration {
  std::unordered_map<std::string, std::string> m_mappings;
public:
  Configuration(std::string filename);
  void LoadFromFile(std::string filename);
  std::unordered_map<std::string, std::string> GetMappings();
  std::string Get(std::string key);
  bool HasKey(std::string key);
  void PrintMappings();
};

#endif
