#include "config.h"
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace hwdetect {

// Minimal JSON parser — avoids adding a dependency like nlohmann
namespace {

void skip_ws(const std::string& s, size_t& i) {
  while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')) i++;
}

std::string read_string(const std::string& s, size_t& i) {
  if (i >= s.size() || s[i] != '"') return "";
  i++; // skip opening "
  std::string result;
  while (i < s.size() && s[i] != '"') {
    if (s[i] == '\\' && i + 1 < s.size()) {
      i++;
      switch (s[i]) {
        case '"':  result += '"';  break;
        case '\\': result += '\\'; break;
        case '/':  result += '/';  break;
        case 'n':  result += '\n'; break;
        case 'r':  result += '\r'; break;
        case 't':  result += '\t'; break;
        default:   result += s[i]; break;
      }
    } else {
      result += s[i];
    }
    i++;
  }
  if (i < s.size()) i++; // skip closing "
  return result;
}

std::string read_raw_value(const std::string& s, size_t& i) {
  size_t start = i;
  while (i < s.size() && s[i] != ',' && s[i] != '}' && s[i] != ']' && s[i] != '\n' && s[i] != '\r') {
    i++;
  }
  std::string val = s.substr(start, i - start);
  // trim
  size_t e = val.size();
  while (e > 0 && (val[e-1] == ' ' || val[e-1] == '\t')) e--;
  size_t b = 0;
  while (b < val.size() && (val[b] == ' ' || val[b] == '\t')) b++;
  return val.substr(b, e - b);
}

} // anonymous namespace

bool Config::load(const std::string& path) {
  std::ifstream file(path);
  if (!file.is_open()) return false;

  std::stringstream ss;
  ss << file.rdbuf();
  std::string content = ss.str();
  file.close();

  // Parse flat JSON object (no nested objects for simplicity)
  size_t i = 0;
  skip_ws(content, i);
  if (i >= content.size() || content[i] != '{') return false;
  i++; // skip {

  while (i < content.size()) {
    skip_ws(content, i);
    if (i >= content.size() || content[i] == '}') break;

    std::string key = read_string(content, i);
    if (key.empty()) break;

    skip_ws(content, i);
    if (i >= content.size() || content[i] != ':') break;
    i++; // skip :
    skip_ws(content, i);

    if (content[i] == '"') {
      m_data[key] = read_string(content, i);
    } else if (content[i] == 't' || content[i] == 'f') {
      std::string raw = read_raw_value(content, i);
      m_data[key] = (raw == "true");
    } else if (content[i] == '[') {
      i++; // skip [
      std::vector<std::string> arr;
      while (i < content.size()) {
        skip_ws(content, i);
        if (content[i] == ']') { i++; break; }
        if (content[i] == '"') {
          arr.push_back(read_string(content, i));
        } else {
          arr.push_back(read_raw_value(content, i));
        }
        skip_ws(content, i);
        if (content[i] == ',') i++;
      }
      m_data[key] = arr;
    } else {
      std::string raw = read_raw_value(content, i);
      try {
        if (raw.find('.') != std::string::npos) {
          m_data[key] = std::stod(raw);
        } else {
          m_data[key] = (int64_t)std::stoll(raw);
        }
      } catch (...) {
        m_data[key] = raw;
      }
    }

    skip_ws(content, i);
    if (i < content.size() && content[i] == ',') i++;
  }

  m_loaded = true;
  return true;
}

bool Config::save(const std::string& path) const {
  std::ofstream file(path);
  if (!file.is_open()) return false;

  file << "{\n";
  size_t count = 0;
  for (const auto& [key, val] : m_data) {
    if (count++ > 0) file << ",\n";
    file << "  \"" << key << "\": ";
    std::visit([&](auto&& arg) {
      using T = std::decay_t<decltype(arg)>;
      if constexpr (std::is_same_v<T, std::string>) {
        file << "\"" << arg << "\"";
      } else if constexpr (std::is_same_v<T, bool>) {
        file << (arg ? "true" : "false");
      } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
        file << "[";
        for (size_t i = 0; i < arg.size(); ++i) {
          if (i > 0) file << ", ";
          file << "\"" << arg[i] << "\"";
        }
        file << "]";
      } else if constexpr (std::is_same_v<T, double>) {
        file << arg;
      } else {
        file << (int64_t)arg;
      }
    }, val);
  }
  file << "\n}\n";
  file.close();
  return true;
}

void Config::set(const std::string& key, ConfigValue value) {
  m_data[key] = std::move(value);
}

ConfigValue Config::get(const std::string& key, const ConfigValue& default_val) const {
  auto it = m_data.find(key);
  return (it != m_data.end()) ? it->second : default_val;
}

std::string Config::get_string(const std::string& key, const std::string& default_val) const {
  auto it = m_data.find(key);
  if (it != m_data.end() && std::holds_alternative<std::string>(it->second))
    return std::get<std::string>(it->second);
  return default_val;
}

int64_t Config::get_int(const std::string& key, int64_t default_val) const {
  auto it = m_data.find(key);
  if (it != m_data.end() && std::holds_alternative<int64_t>(it->second))
    return std::get<int64_t>(it->second);
  return default_val;
}

double Config::get_double(const std::string& key, double default_val) const {
  auto it = m_data.find(key);
  if (it != m_data.end() && std::holds_alternative<double>(it->second))
    return std::get<double>(it->second);
  return default_val;
}

bool Config::get_bool(const std::string& key, bool default_val) const {
  auto it = m_data.find(key);
  if (it != m_data.end() && std::holds_alternative<bool>(it->second))
    return std::get<bool>(it->second);
  return default_val;
}

} // namespace hwdetect
