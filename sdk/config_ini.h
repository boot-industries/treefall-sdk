#ifndef CONFIG_INI_H
#define CONFIG_INI_H

#ifdef _WIN32
#include <windows.h>

namespace config_ini {
inline int getInt(const char *fn, const char *section, const char *key, int defval) {
  return (int)GetPrivateProfileInt(section, key, defval, fn);
}
inline bool getBinary(const char *fn, const char *section, const char *key, void *data, unsigned int len) {
  return GetPrivateProfileStruct(section, key, data, len, fn) != 0;
}
inline void setString(const char *fn, const char *section, const char *key, const char *value) {
  WritePrivateProfileString(section, key, value, fn);
}
inline void setBinary(const char *fn, const char *section, const char *key, const void *data, unsigned int len) {
  WritePrivateProfileStruct(section, key, const_cast<void *>(data), len, fn);
}
}

#else

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cctype>
#include <cstdio>

namespace config_ini {

static inline std::string trim(const std::string &s) {
  size_t start = 0, end = s.size();
  while (start < end && std::isspace((unsigned char)s[start])) start++;
  while (end > start && std::isspace((unsigned char)s[end-1])) end--;
  return s.substr(start, end-start);
}

static inline std::string readValue(const std::filesystem::path &path, const std::string &section, const std::string &key) {
  std::ifstream in(path);
  if (!in.is_open()) return {};
  std::string line;
  bool in_section = false;
  while (std::getline(in, line)) {
    std::string t = trim(line);
    if (t.empty() || t[0] == ';' || t[0] == '#') continue;
    if (t.front() == '[' && t.back() == ']') {
      in_section = (t.substr(1, t.size()-2) == section);
      continue;
    }
    if (in_section) {
      size_t eq = t.find('=');
      if (eq != std::string::npos) {
        std::string k = trim(t.substr(0, eq));
        if (k == key) return trim(t.substr(eq+1));
      }
    }
  }
  return {};
}

static inline void writeValue(const std::filesystem::path &path, const std::string &section, const std::string &key, const std::string &value) {
  std::ifstream in(path);
  std::vector<std::string> lines;
  std::string line;
  bool section_found = false;
  bool key_written = false;
  while (in.good() && std::getline(in, line)) {
    lines.push_back(line);
  }
  in.close();
  for (size_t i = 0; i < lines.size(); ++i) {
    std::string t = trim(lines[i]);
    if (t.size() > 0 && t.front() == '[' && t.back() == ']') {
      std::string sec = t.substr(1, t.size()-2);
      if (sec == section) {
        section_found = true;
        size_t j = i + 1;
        for (; j < lines.size(); ++j) {
          std::string lt = trim(lines[j]);
          if (lt.size() > 0 && lt.front() == '[' && lt.back() == ']') break;
          size_t eq = lt.find('=');
          if (eq != std::string::npos) {
            std::string k = trim(lt.substr(0, eq));
            if (k == key) {
              lines[j] = std::string(key) + "=" + value;
              key_written = true;
              break;
            }
          }
        }
        if (!key_written) {
          lines.insert(lines.begin()+j, std::string(key) + "=" + value);
          key_written = true;
        }
        break;
      }
    }
  }
  if (!section_found) {
    lines.push_back("[" + section + "]");
    lines.push_back(std::string(key) + "=" + value);
    key_written = true;
  }
  else if (!key_written) {
    lines.push_back(std::string(key) + "=" + value);
  }
  std::ofstream out(path, std::ios::trunc);
  for (const auto &l : lines) out << l << "\n";
}

inline int getInt(const char *fn, const char *section, const char *key, int defval) {
  std::string v = readValue(std::filesystem::path(fn), section, key);
  if (v.empty()) return defval;
  return atoi(v.c_str());
}

inline bool getBinary(const char *fn, const char *section, const char *key, void *data, unsigned int len) {
  std::string v = readValue(std::filesystem::path(fn), section, key);
  if (v.empty() || v.length() < len*2) return false;
  unsigned char *out = (unsigned char *)data;
  for (unsigned int i = 0; i < len; ++i) {
    unsigned int byte;
    if (sscanf(v.c_str() + i*2, "%02x", &byte) != 1) return false;
    out[i] = (unsigned char)byte;
  }
  return true;
}

inline void setString(const char *fn, const char *section, const char *key, const char *value) {
  writeValue(std::filesystem::path(fn), section, key, value ? value : "");
}

inline void setBinary(const char *fn, const char *section, const char *key, const void *data, unsigned int len) {
  const unsigned char *in = (const unsigned char *)data;
  std::string v;
  char buf[3];
  for (unsigned int i = 0; i < len; ++i) {
    sprintf(buf, "%02X", in[i]);
    v += buf;
  }
  setString(fn, section, key, v.c_str());
}

} // namespace config_ini

#endif

#endif
