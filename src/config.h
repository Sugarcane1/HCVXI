#pragma once
#include <string>
#include <map>
#include <variant>
#include <vector>
#include <fstream>

namespace hwdetect {

using ConfigValue = std::variant<std::string, int64_t, double, bool, std::vector<std::string>>;

class Config {
public:
  static Config& instance() {
    static Config cfg;
    return cfg;
  }

  bool load(const std::string& path);
  bool save(const std::string& path) const;

  void set(const std::string& key, ConfigValue value);
  ConfigValue get(const std::string& key, const ConfigValue& default_val = "") const;

  std::string get_string(const std::string& key, const std::string& default_val = "") const;
  int64_t  get_int(const std::string& key, int64_t default_val = 0) const;
  double   get_double(const std::string& key, double default_val = 0.0) const;
  bool     get_bool(const std::string& key, bool default_val = false) const;

  bool is_loaded() const { return m_loaded; }

private:
  Config() = default;
  std::map<std::string, ConfigValue> m_data;
  bool m_loaded = false;
};

} // namespace hwdetect
