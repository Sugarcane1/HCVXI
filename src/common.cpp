#include "common.h"
#include "config.h"
#include "logger.h"

#include <hwinfo/utils/unit.h>
#include <hwinfo/utils/stringutils.h>

#ifdef _WIN32
#include <windows.h>
#include <comdef.h>
#include <wbemidl.h>
#include <setupapi.h>
#include <initguid.h>
#pragma comment(lib, "Advapi32.lib")
#endif

#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <cmath>
#include <mutex>
#include <algorithm>

using namespace hwinfo::unit;
using namespace hwdetect;

// ============================================================
// Global shared state
// ============================================================

std::vector<DiskDetailInfo> g_disk_details;
std::mutex g_disk_mutex;

// ============================================================
// Encoding helpers
// ============================================================

std::string trim_str(const std::string& s) {
  size_t start = s.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) return "";
  size_t end = s.find_last_not_of(" \t\r\n");
  return s.substr(start, end - start + 1);
}

bool is_valid_utf8(const std::string& str) {
  const uint8_t* p = (const uint8_t*)str.c_str();
  const uint8_t* end = p + str.size();
  while (p < end) {
    if (*p < 0x80) { p++; continue; }
    int n;
    if ((*p & 0xE0) == 0xC0) n = 2;
    else if ((*p & 0xF0) == 0xE0) n = 3;
    else if ((*p & 0xF8) == 0xF0) n = 4;
    else return false;
    if (p + n > end) return false;
    for (int i = 1; i < n; i++) {
      if ((p[i] & 0xC0) != 0x80) return false;
    }
    p += n;
  }
  return true;
}

#ifdef _WIN32
std::wstring utf8_to_wide(const std::string& str) {
  if (str.empty()) return L"";
  int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
  if (len <= 0) return L"";
  std::wstring wstr(len - 1, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], len);
  return wstr;
}

std::wstring ansi_to_wide(const std::string& str) {
  if (str.empty()) return L"";
  int len = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, nullptr, 0);
  if (len <= 0) return L"";
  std::wstring wstr(len - 1, L'\0');
  MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, &wstr[0], len);
  return wstr;
}
#endif

std::string gbk_to_utf8(const std::string& gbk_str) {
#ifdef _WIN32
  std::wstring wide = ansi_to_wide(gbk_str);
  if (wide.empty()) return gbk_str;
  int len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
  if (len <= 0) return gbk_str;
  std::string utf8(len - 1, '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, &utf8[0], len, nullptr, nullptr);
  return utf8;
#else
  return gbk_str;
#endif
}

std::string ensure_utf8(const std::string& str) {
  if (str.size() >= 3 && (uint8_t)str[0] == 0xEF && (uint8_t)str[1] == 0xBB && (uint8_t)str[2] == 0xBF) {
    return is_valid_utf8(str.substr(3)) ? str.substr(3) : gbk_to_utf8(str.substr(3));
  }
  return is_valid_utf8(str) ? str : gbk_to_utf8(str);
}

// ============================================================
// Brand translation
// ============================================================

std::string translate_brand(const std::string& brand) {
  static const struct { const char* en; const char* cn; } map[] = {
    {"Samsung", "\xe4\xb8\x89\xe6\x98\x9f"},
    {"SK Hynix", "SK\xe6\xb5\xb7\xe5\x8a\x9b\xe5\xa3\xab"},
    {"Hynix", "\xe6\xb5\xb7\xe5\x8a\x9b\xe5\xa3\xab"},
    {"Micron", "\xe9\x95\x81\xe5\x85\x89"},
    {"Crucial", "\xe8\x8b\xb1\xe7\x9d\x9b\xe8\xbe\xbe"},
    {"Kingston", "\xe9\x87\x91\xe5\xa3\xab\xe9\xa1\xbf"},
    {"Corsair", "\xe6\xb5\xb7\xe7\x9b\x97\xe8\x88\xb9"},
    {"G.Skill", "\xe8\x8a\x9d\xe5\xa5\x87"},
    {"ADATA", "\xe5\xa8\x81\xe5\x88\x9a"},
    {"Predator", "\xe6\x8e\xa0\xe5\xa4\xba\xe8\x80\x85"},
    {"Western Digital", "\xe8\xa5\xbf\xe9\x83\xa8\xe6\x95\xb0\xe6\x8d\xae"},
    {"Acer", "\xe5\xae\x8f\xe7\xa2\x81"},
    {"WDC", "\xe8\xa5\xbf\xe9\x83\xa8\xe6\x95\xb0\xe6\x8d\xae"},
    {"Seagate", "\xe5\xb8\x8c\xe6\x8d\xb7"},
    {"Toshiba", "\xe4\xb8\x9c\xe8\x8a\x9d"},
    {"KIOXIA", "\xe9\x93\xa0\xe4\xbe\xa0"},
    {"Intel", "\xe8\x8b\xb1\xe7\x89\xb9\xe5\xb0\x94"},
    {"ZHITAI", "\xe8\x87\xb4\xe6\x80\x81"},
    {"LENOVO", "\xe8\x81\x94\xe6\x83\xb3"},
    {"Lenovo", "\xe8\x81\x94\xe6\x83\xb3"},
    {"Dell", "\xe6\x88\xb4\xe5\xb0\x94"},
    {"DELL", "\xe6\x88\xb4\xe5\xb0\x94"},
    {"HP", "\xe6\x83\xa0\xe6\x99\xae"},
    {"AOC", "AOC"},
    {"ASUS", "\xe5\x8d\x8e\xe7\xa1\x95"},
    {"Asus", "\xe5\x8d\x8e\xe7\xa1\x95"},
    {"LG", "LG"},
    {"BenQ", "\xe6\x98\x8e\xe5\x9f\xba"},
    {"Philips", "\xe9\xa3\x9e\xe5\x88\xa9\xe6\xb5\xa6"},
    {"ViewSonic", "\xe4\xbc\x98\xe6\xb4\xbe"},
    {"Dell Inc.", "\xe6\x88\xb4\xe5\xb0\x94"},
    {"Hewlett-Packard", "\xe6\x83\xa0\xe6\x99\xae"},
    {"MSI", "\xe5\xbe\xae\xe6\x98\x9f"},
    {"Razer", "\xe9\x9b\xb7\xe8\x9b\x87"},
    {"Sceptre", "Sceptre"},
    {"SAMSUNG", "\xe4\xb8\x89\xe6\x98\x9f"},
    {"HUAWEI", "\xe5\x8d\x8e\xe4\xb8\xba"},
    {"Huawei", "\xe5\x8d\x8e\xe4\xb8\xba"},
    {"Xiaomi", "\xe5\xb0\x8f\xe7\xb1\xb3"},
    {"BOE", "\xe4\xba\xac\xe4\xb8\x9c\xe6\x96\xb9"},
    {"HKC", "\xe6\x83\xa0\xe7\xa7\x91"},
    {"SANXING", "\xe4\xb8\x89\xe6\x98\x9f"},
    {"SNC", "\xe4\xb8\x89\xe6\x98\x9f"},
    {"SEC", "\xe4\xb8\x89\xe6\x98\x9f"},
  };
  for (const auto& m : map) {
    if (brand.find(m.en) != std::string::npos) return m.cn;
  }
  return brand;
}

// ============================================================
// Formatting
// ============================================================

std::string hz_to_mhz(uint64_t hz) {
  if (hz == 0) return "N/A";
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(0) << unit_prefix_to(hz, SiPrefix::MEGA);
  return oss.str();
}

std::string bytes_to_gib(uint64_t bytes) {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(2) << unit_prefix_to(bytes, IECPrefix::GIBI) << " GiB";
  return oss.str();
}

// ============================================================
// Admin check (FIXED — logic was inverted)
// ============================================================

#ifdef _WIN32
bool is_admin() {
  BOOL isAdmin = FALSE;
  PSID adminGroup = NULL;

  SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
  if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0,
                                &adminGroup)) {
    if (!CheckTokenMembership(NULL, adminGroup, &isAdmin)) {
      isAdmin = FALSE;
    }
    FreeSid(adminGroup);
  }

  return isAdmin != FALSE;
}
#endif

// ============================================================
// CrystalDiskInfo executable search
// ============================================================

std::string find_crystal_disk_info() {
  // Minimum version requirement
  std::string min_ver = Config::instance().get_string("crystal_disk_info_min_version", "9.0.0");

  // Check config-defined paths first then fall back to compiled defaults
  auto cfg_paths = Config::instance().get("crystal_disk_info_search_paths", {});
  if (std::holds_alternative<std::vector<std::string>>(cfg_paths)) {
    for (const auto& path : std::get<std::vector<std::string>>(cfg_paths)) {
      if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
        HW_LOG_INFO("CrystalDiskInfo found at configured path: " + path);
        return path;
      }
    }
  }

  const char* search_paths[] = {
    "Tools\\DiskInfo64.exe",
    "Tools\\DiskInfo32.exe",
    ".\\CrystalDiskInfo\\DiskInfo64.exe",
    ".\\CrystalDiskInfo\\DiskInfo32.exe"
  };

  for (const auto& path : search_paths) {
    if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) {
      HW_LOG_INFO("CrystalDiskInfo found at: " + std::string(path));
      return path;
    }
  }

  char exe_path[MAX_PATH];
  GetModuleFileNameA(NULL, exe_path, MAX_PATH);

  std::string dir(exe_path);
  size_t last_slash = dir.find_last_of("\\/");
  if (last_slash != std::string::npos) {
    dir = dir.substr(0, last_slash + 1);

    const char* relative_paths[] = {
      "CrystalDiskInfo\\DiskInfo64.exe",
      "CrystalDiskInfo\\DiskInfo32.exe",
      "Tools\\DiskInfo64.exe"
    };

    for (const auto& rel : relative_paths) {
      std::string full_path = dir + rel;
      if (GetFileAttributesA(full_path.c_str()) != INVALID_FILE_ATTRIBUTES) {
        HW_LOG_INFO("CrystalDiskInfo found relative to exe: " + full_path);
        return full_path;
      }
    }

    auto search_wildcard = [](const std::string& base_dir) -> std::string {
      std::string search_pattern = base_dir + "CrystalDiskInfo*";
      WIN32_FIND_DATAA find_data;
      HANDLE hFind = FindFirstFileA(search_pattern.c_str(), &find_data);
      if (hFind != INVALID_HANDLE_VALUE) {
        do {
          if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            std::string sub_dir = base_dir + find_data.cFileName + "\\";
            std::string exe64 = sub_dir + "DiskInfo64.exe";
            std::string exeA64 = sub_dir + "DiskInfoA64.exe";
            std::string exe32 = sub_dir + "DiskInfo32.exe";
            if (GetFileAttributesA(exe64.c_str()) != INVALID_FILE_ATTRIBUTES) {
              FindClose(hFind);
              return exe64;
            }
            if (GetFileAttributesA(exeA64.c_str()) != INVALID_FILE_ATTRIBUTES) {
              FindClose(hFind);
              return exeA64;
            }
            if (GetFileAttributesA(exe32.c_str()) != INVALID_FILE_ATTRIBUTES) {
              FindClose(hFind);
              return exe32;
            }
          }
        } while (FindNextFileA(hFind, &find_data));
        FindClose(hFind);
      }
      return "";
    };

    std::string current_dir = dir;
    for (int i = 0; i < 5; i++) {
      std::string result = search_wildcard(current_dir);
      if (!result.empty()) return result;

      size_t pos = current_dir.find_last_of("\\/", current_dir.size() - 2);
      if (pos == std::string::npos) break;
      current_dir = current_dir.substr(0, pos + 1);

      if (current_dir.length() <= 3) break;
    }
  }

  HKEY hKey = nullptr;
  if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                     "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
                     0, KEY_READ | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS) {
    DWORD index = 0;
    char subKeyName[256];
    DWORD subKeyNameSize = sizeof(subKeyName);
    while (RegEnumKeyExA(hKey, index++, subKeyName, &subKeyNameSize,
                          nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
      HKEY hSubKey = nullptr;
      if (RegOpenKeyExA(hKey, subKeyName, 0, KEY_READ, &hSubKey) == ERROR_SUCCESS) {
        char displayName[512] = {0};
        DWORD displayNameSize = sizeof(displayName);
        if (RegQueryValueExA(hSubKey, "DisplayName", nullptr, nullptr,
                              (LPBYTE)displayName, &displayNameSize) == ERROR_SUCCESS) {
          if (strstr(displayName, "CrystalDiskInfo") != nullptr) {
            char installLocation[MAX_PATH] = {0};
            DWORD installSize = sizeof(installLocation);
            if (RegQueryValueExA(hSubKey, "InstallLocation", nullptr, nullptr,
                                  (LPBYTE)installLocation, &installSize) == ERROR_SUCCESS) {
              std::string base(installLocation);
              if (!base.empty() && base.back() != '\\') base += '\\';
              std::string exe64 = base + "DiskInfo64.exe";
              std::string exe32 = base + "DiskInfo32.exe";
              if (GetFileAttributesA(exe64.c_str()) != INVALID_FILE_ATTRIBUTES) {
                RegCloseKey(hSubKey);
                RegCloseKey(hKey);
                return exe64;
              }
              if (GetFileAttributesA(exe32.c_str()) != INVALID_FILE_ATTRIBUTES) {
                RegCloseKey(hSubKey);
                RegCloseKey(hKey);
                return exe32;
              }
            }
          }
        }
        RegCloseKey(hSubKey);
      }
      subKeyNameSize = sizeof(subKeyName);
    }
    RegCloseKey(hKey);
  }

  const char* common_paths[] = {
    "C:\\Program Files\\CrystalDiskInfo\\DiskInfo64.exe",
    "C:\\Program Files\\CrystalDiskInfo\\DiskInfo32.exe",
    "C:\\Program Files (x86)\\CrystalDiskInfo\\DiskInfo64.exe",
    "C:\\Program Files (x86)\\CrystalDiskInfo\\DiskInfo32.exe",
    "C:\\CrystalDiskInfo\\DiskInfo64.exe",
    "C:\\CrystalDiskInfo\\DiskInfo32.exe"
  };
  for (const auto* p : common_paths) {
    if (GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES) {
      HW_LOG_INFO("CrystalDiskInfo found at common path: " + std::string(p));
      return p;
    }
  }

  HW_LOG_WARNING("CrystalDiskInfo not found.");
  return "";
}

// ============================================================
// CrystalDiskInfo output parser
// ============================================================

void parse_diskinfo_output(const std::string& content, std::vector<DiskDetailInfo>& disks) {
  std::istringstream stream(content);
  std::string line;
  bool in_disk_block = false;
  DiskDetailInfo current_info;

  auto finalize_disk = [&]() {
    if (current_info.loaded && !current_info.model.empty()) {
      disks.push_back(std::move(current_info));
      current_info = DiskDetailInfo{};
    }
  };

  while (std::getline(stream, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }

    size_t disk_header = line.find(" (");
    if (disk_header != std::string::npos && line.find(") ") != std::string::npos && line.find(":") == std::string::npos) {
      finalize_disk();
      in_disk_block = true;
      current_info.loaded = true;
      continue;
    }

    if (in_disk_block) {
      size_t colon_pos = line.find(" : ");
      if (colon_pos == std::string::npos) {
        colon_pos = line.find(":");
      }

      if (colon_pos != std::string::npos || line.find(" :") != std::string::npos) {
        size_t actual_colon = line.find(" :");
        if (actual_colon == std::string::npos) {
          actual_colon = line.find(":");
        }

        if (actual_colon != std::string::npos) {
          std::string key = line.substr(0, actual_colon);
          std::string value = line.substr(actual_colon);

          while (!value.empty() && (value[0] == ' ' || value[0] == ':')) {
            value.erase(0, 1);
          }
          while (!value.empty() && value[0] == ' ') {
            value.erase(0, 1);
          }

          while (!key.empty() && key.back() == ' ') {
            key.pop_back();
          }

          if (key.find("Model") != std::string::npos) {
            current_info.model = value;
          } else if (key.find("Firmware") != std::string::npos) {
            current_info.firmware = value;
          } else if (key.find("Serial Number") != std::string::npos) {
            current_info.serial = value;
          } else if (key.find("Disk Size") != std::string::npos) {
            current_info.disk_size = value;
          } else if (key.find("Interface") != std::string::npos) {
            current_info.interface_type = value;
          } else if (key.find("Standard") != std::string::npos) {
            current_info.standard = value;
          } else if (key.find("Transfer Mode") != std::string::npos) {
            current_info.transfer_mode = value;
          } else if (key.find("Power On Hours") != std::string::npos) {
            std::string num;
            for (char c : value) {
              if (isdigit(static_cast<unsigned char>(c)) || c == '.') num += c;
              else if (!num.empty()) break;
            }
            current_info.power_on_hours = num;
          } else if (key.find("Power On Count") != std::string::npos) {
            std::string num;
            for (char c : value) {
              if (isdigit(static_cast<unsigned char>(c)) || c == '.') num += c;
              else if (!num.empty()) break;
            }
            current_info.power_on_count = num;
          } else if (key.find("Host Reads") != std::string::npos || key.find("Total Host Reads") != std::string::npos) {
            current_info.total_host_reads = value;
          } else if (key.find("Host Writes") != std::string::npos || key.find("Total Host Writes") != std::string::npos) {
            current_info.total_host_writes = value;
          } else if (key.find("Temperature") != std::string::npos) {
            current_info.temperature = value;
          } else if (key.find("Health Status") != std::string::npos) {
            size_t p1 = value.find('(');
            size_t p2 = value.find(')');
            if (p1 != std::string::npos && p2 != std::string::npos && p2 > p1) {
              current_info.health_status = value.substr(p1 + 1, p2 - p1 - 1);
            } else {
              current_info.health_status = value;
            }
          } else if (key.find("Features") != std::string::npos) {
            current_info.features = value;
          } else if (key.find("Drive Letter") != std::string::npos) {
            current_info.drive_letter = value;
          } else if (key.find("Rotation Rate") != std::string::npos) {
            current_info.rotation_rate = value;
          }
        }
      }

      if (line.find("-- S.M.A.R.T.") != std::string::npos ||
          line.find("-- Controller Map") != std::string::npos ||
          line.find("-- Disk List") != std::string::npos) {
        finalize_disk();
        in_disk_block = false;
      }
    }
  }

  finalize_disk();
}

// ============================================================
// WMI disk fallback
// ============================================================

void get_disk_details_wmi() {
  HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
  if (FAILED(hr)) {
    HW_LOG_WARNING("get_disk_details_wmi: CoInitializeEx failed");
    return;
  }

  IWbemLocator* pLocator = NULL;
  hr = CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER,
                        IID_IWbemLocator, (LPVOID*)&pLocator);
  if (FAILED(hr)) {
    HW_LOG_WARNING("get_disk_details_wmi: CoCreateInstance(WbemLocator) failed");
    CoUninitialize(); return;
  }

  IWbemServices* pServices = NULL;
  hr = pLocator->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), NULL, NULL, NULL, 0, NULL, NULL, &pServices);
  if (FAILED(hr)) {
    HW_LOG_WARNING("get_disk_details_wmi: ConnectServer(ROOT\\CIMV2) failed");
    pLocator->Release(); CoUninitialize(); return;
  }

  CoSetProxyBlanket(pServices, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
                    RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);

  std::vector<DiskDetailInfo> disk_details;
  IEnumWbemClassObject* pEnumerator = NULL;
  hr = pServices->ExecQuery(_bstr_t(L"WQL"), _bstr_t(L"SELECT * FROM Win32_DiskDrive"),
                            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumerator);

  if (SUCCEEDED(hr) && pEnumerator) {
    IWbemClassObject* pObj = NULL;
    ULONG uReturn = 0;
    while (pEnumerator->Next(WBEM_INFINITE, 1, &pObj, &uReturn) != WBEM_S_FALSE) {
      DiskDetailInfo info;
      VARIANT vtProp; VariantInit(&vtProp);

      if (SUCCEEDED(pObj->Get(L"Model", 0, &vtProp, NULL, NULL)) && vtProp.vt == VT_BSTR)
        info.model = _bstr_t(vtProp.bstrVal); VariantClear(&vtProp);

      if (SUCCEEDED(pObj->Get(L"SerialNumber", 0, &vtProp, NULL, NULL)) && vtProp.vt == VT_BSTR)
        info.serial = _bstr_t(vtProp.bstrVal); VariantClear(&vtProp);

      if (SUCCEEDED(pObj->Get(L"FirmwareRevision", 0, &vtProp, NULL, NULL)) && vtProp.vt == VT_BSTR)
        info.firmware = _bstr_t(vtProp.bstrVal); VariantClear(&vtProp);

      if (SUCCEEDED(pObj->Get(L"InterfaceType", 0, &vtProp, NULL, NULL)) && vtProp.vt == VT_BSTR)
        info.interface_type = _bstr_t(vtProp.bstrVal); VariantClear(&vtProp);

      if (SUCCEEDED(pObj->Get(L"MediaType", 0, &vtProp, NULL, NULL)) && vtProp.vt == VT_BSTR) {
        info.media_type = _bstr_t(vtProp.bstrVal); VariantClear(&vtProp);
      }

      uint64_t size_val = 0;
      if (SUCCEEDED(pObj->Get(L"Size", 0, &vtProp, NULL, NULL))) {
        if (vtProp.vt == VT_UI8) size_val = vtProp.ullVal;
        else if (vtProp.vt == VT_I8) size_val = (uint64_t)vtProp.llVal;
        else if (vtProp.vt == VT_UI4) size_val = vtProp.ulVal;
        else if (vtProp.vt == VT_I4) size_val = (uint64_t)vtProp.lVal;
        else if (vtProp.vt == VT_BSTR) {
          std::string s = _bstr_t(vtProp.bstrVal);
          try { size_val = std::stoull(s); } catch (...) {}
        }
      }
      VariantClear(&vtProp);

      if (size_val > 0) {
        double gb = size_val / (1024.0 * 1024.0 * 1024.0);
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << gb << " GB";
        info.disk_size = oss.str();
      }

      if (SUCCEEDED(pObj->Get(L"Status", 0, &vtProp, NULL, NULL)) && vtProp.vt == VT_BSTR)
        info.health_status = _bstr_t(vtProp.bstrVal); VariantClear(&vtProp);

      VariantInit(&vtProp);
      if (SUCCEEDED(pObj->Get(L"PnPDeviceID", 0, &vtProp, NULL, NULL)) && vtProp.vt == VT_BSTR) {
        std::string pnp_id = _bstr_t(vtProp.bstrVal);
        if (pnp_id.find("NVME") != std::string::npos || pnp_id.find("NVM_") != std::string::npos) {
          info.interface_type = "NVMe";
        }
      }
      VariantClear(&vtProp);

      pObj->Release();
      info.loaded = true;
      disk_details.push_back(info);
    }
    pEnumerator->Release();
  }

  {
    std::lock_guard<std::mutex> lock(g_disk_mutex);
    g_disk_details = std::move(disk_details);
  }

  pServices->Release();
  pLocator->Release();
  CoUninitialize();
}

void get_disk_details_async() {
  HW_LOG_INFO("get_disk_details_async: starting disk detection");

  std::string diskinfo_exe = find_crystal_disk_info();
  bool success = false;

  if (!diskinfo_exe.empty()) {
    int timeout_ms = (int)Config::instance().get_int("crystal_disk_info_timeout_ms", 30000);
    std::string working_dir = diskinfo_exe.substr(0, diskinfo_exe.find_last_of("\\/"));
    std::string output_file = working_dir + "\\DiskInfo.txt";

    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE hNull = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_WRITE, &sa,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    STARTUPINFOA si = { sizeof(si) };
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hNull;
    si.hStdError = hNull;

    PROCESS_INFORMATION pi = {};
    std::string cmd = "\"" + diskinfo_exe + "\" /CopyExit";

    if (CreateProcessA(NULL, (LPSTR)cmd.c_str(), NULL, NULL, TRUE,
                        0, NULL, working_dir.c_str(), &si, &pi)) {
      CloseHandle(hNull);
      DWORD wait = WaitForSingleObject(pi.hProcess, (DWORD)timeout_ms);
      if (wait != WAIT_OBJECT_0) {
        HW_LOG_WARNING("get_disk_details_async: CrystalDiskInfo timed out after " +
                       std::to_string(timeout_ms) + "ms");
        TerminateProcess(pi.hProcess, 1);
      }
      CloseHandle(pi.hProcess);
      CloseHandle(pi.hThread);

      if (wait == WAIT_OBJECT_0) {
        Sleep(500);
        std::ifstream file(output_file);
        if (file.is_open()) {
          std::stringstream buffer;
          buffer << file.rdbuf();
          file.close();
          std::string content = ensure_utf8(buffer.str());
          DeleteFileA(output_file.c_str());

          std::vector<DiskDetailInfo> disks;
          parse_diskinfo_output(content, disks);
          if (!disks.empty()) {
            std::lock_guard<std::mutex> lock(g_disk_mutex);
            g_disk_details = std::move(disks);
            success = true;
            HW_LOG_INFO("get_disk_details_async: CrystalDiskInfo parsed " +
                        std::to_string(g_disk_details.size()) + " disk(s)");
          }
        }
      }
    } else {
      CloseHandle(hNull);
      HW_LOG_WARNING("get_disk_details_async: failed to launch CrystalDiskInfo");
    }
  }

  if (!success) {
    HW_LOG_INFO("get_disk_details_async: falling back to WMI");
    get_disk_details_wmi();
  }
}

// ============================================================
// WMI monitor detection
// ============================================================

bool get_monitor_info_from_wmi(std::vector<MonitorInfo>& monitors) {
  HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
  if (FAILED(hr)) {
    HW_LOG_WARNING("get_monitor_info_from_wmi: CoInitializeEx failed");
    return false;
  }

  IWbemLocator* pLocator = NULL;
  hr = CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER,
                        IID_IWbemLocator, (LPVOID*)&pLocator);
  if (FAILED(hr)) {
    HW_LOG_WARNING("get_monitor_info_from_wmi: CoCreateInstance failed");
    CoUninitialize();
    return false;
  }

  IWbemServices* pServices = NULL;
  hr = pLocator->ConnectServer(_bstr_t(L"ROOT\\WMI"), NULL, NULL, NULL, 0, NULL, NULL, &pServices);
  if (FAILED(hr)) {
    HW_LOG_WARNING("get_monitor_info_from_wmi: ConnectServer(ROOT\\WMI) failed");
    pLocator->Release();
    CoUninitialize();
    return false;
  }

  hr = CoSetProxyBlanket(pServices, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
                         RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
  if (FAILED(hr)) {
    pServices->Release();
    pLocator->Release();
    CoUninitialize();
    return false;
  }

  IEnumWbemClassObject* pEnumerator = NULL;
  hr = pServices->ExecQuery(_bstr_t(L"WQL"),
                            _bstr_t(L"SELECT * FROM WmiMonitorID"),
                            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                            NULL, &pEnumerator);
  if (FAILED(hr)) {
    HW_LOG_WARNING("get_monitor_info_from_wmi: ExecQuery(WmiMonitorID) failed");
    pServices->Release();
    pLocator->Release();
    CoUninitialize();
    return false;
  }

  IWbemClassObject* pObj = NULL;
  ULONG uReturn = 0;
  while (pEnumerator->Next(WBEM_INFINITE, 1, &pObj, &uReturn) != WBEM_S_FALSE) {
    MonitorInfo mi;
    VARIANT vtProp;

    VariantInit(&vtProp);
    hr = pObj->Get(L"InstanceName", 0, &vtProp, NULL, NULL);
    if (SUCCEEDED(hr) && vtProp.vt == VT_BSTR) {
      mi.name = _bstr_t(vtProp.bstrVal);
      std::string instance = mi.name;
      size_t b1 = instance.find('\\');
      if (b1 != std::string::npos) {
        size_t b2 = instance.find('\\', b1 + 1);
        if (b2 != std::string::npos) {
          std::string dev = instance.substr(b1 + 1, b2 - b1 - 1);
          std::string fp = dev.substr(0, dev.find('&'));
          if (fp.length() >= 3) mi.manufacturer = fp.substr(0, 3);
        }
      }
    }
    VariantClear(&vtProp);

    VariantInit(&vtProp);
    hr = pObj->Get(L"ManufacturerName", 0, &vtProp, NULL, NULL);
    if (SUCCEEDED(hr) && vtProp.vt & VT_ARRAY && vtProp.parray) {
      SAFEARRAY* sa = vtProp.parray;
      long lb = 0, ub = 0;
      SafeArrayGetLBound(sa, 1, &lb); SafeArrayGetUBound(sa, 1, &ub);
      std::string mfg;
      for (long i = lb; i <= ub && i < lb + 10; i++) {
        short v = 0; SafeArrayGetElement(sa, &i, &v);
        if (v >= 32 && v < 127) mfg += (char)v;
        else if (v == 0 && !mfg.empty()) break;
      }
      if (!mfg.empty()) mi.manufacturer = mfg;
    }
    VariantClear(&vtProp);

    VariantInit(&vtProp);
    hr = pObj->Get(L"UserFriendlyName", 0, &vtProp, NULL, NULL);
    if (SUCCEEDED(hr) && vtProp.vt & VT_ARRAY && vtProp.parray) {
      SAFEARRAY* sa = vtProp.parray;
      long lb = 0, ub = 0;
      SafeArrayGetLBound(sa, 1, &lb); SafeArrayGetUBound(sa, 1, &ub);
      std::string nm;
      for (long i = lb; i <= ub && i < lb + 100; i++) {
        short v = 0; SafeArrayGetElement(sa, &i, &v);
        if (v >= 32 && v < 127) nm += (char)v;
        else if (v == 0 && !nm.empty()) break;
      }
      if (!nm.empty()) mi.model = nm;
    }
    VariantClear(&vtProp);

    VariantInit(&vtProp);
    hr = pObj->Get(L"SerialNumberID", 0, &vtProp, NULL, NULL);
    if (SUCCEEDED(hr)) {
      if (vtProp.vt == (VT_UI1 | VT_ARRAY)) {
        SAFEARRAY* sa = vtProp.parray;
        if (sa) {
          long lb = 0, ub = 0;
          SafeArrayGetLBound(sa, 1, &lb); SafeArrayGetUBound(sa, 1, &ub);
          std::string sn;
          for (long i = lb; i <= ub && i < 100; i++) {
            unsigned char v = 0; SafeArrayGetElement(sa, &i, &v);
            if (v >= 32 && v < 127) sn += (char)v;
            else if (v == 0 && !sn.empty()) break;
          }
          if (!sn.empty()) mi.serial = sn;
        }
      } else if (vtProp.vt == VT_BSTR) {
        std::string sn = _bstr_t(vtProp.bstrVal);
        if (!sn.empty()) mi.serial = sn;
      }
    }
    VariantClear(&vtProp);

    VariantInit(&vtProp);
    hr = pObj->Get(L"YearOfManufacture", 0, &vtProp, NULL, NULL);
    if (SUCCEEDED(hr) && vtProp.vt == VT_I4) {
      int yr = vtProp.lVal;
      if (yr > 0) mi.serial = "Year: " + std::to_string(yr) + " SN: " + mi.serial;
    }
    VariantClear(&vtProp);

    VariantInit(&vtProp);
    hr = pObj->Get(L"DisplaySizeInches", 0, &vtProp, NULL, NULL);
    if (SUCCEEDED(hr) && vtProp.vt == VT_UI1) {
      uint8_t sz = vtProp.bVal;
      if (sz > 0 && sz < 100) {
        double diag_mm = sz * 25.4;
        double asp = 16.0 / 9.0;
        double hr2 = 1.0 / sqrt(1.0 + asp * asp);
        mi.physical_height_mm = (uint32_t)(diag_mm * hr2);
        mi.physical_width_mm = (uint32_t)(mi.physical_height_mm * asp);
      }
    }
    VariantClear(&vtProp);

    pObj->Release();
    monitors.push_back(mi);
  }

  pEnumerator->Release();
  pServices->Release();
  pLocator->Release();
  CoUninitialize();
  return monitors.size() > 0;
}

// ============================================================
// Battery detection (powercfg + WMI)
// ============================================================

BatteryReport parse_battery_report(const std::string& file_path) {
  BatteryReport report;
  std::ifstream file(file_path);
  if (!file.is_open()) return report;

  std::string line;
  bool in_battery_section = false;

  while (std::getline(file, line)) {
    if (line.find("Installed batteries") != std::string::npos) {
      in_battery_section = true;
      continue;
    }
    if (line.find("Recent usage") != std::string::npos) {
      in_battery_section = false;
    }
    if (in_battery_section) {
      size_t colon_pos = line.find(':');
      if (colon_pos != std::string::npos) {
        std::string key = line.substr(0, colon_pos);
        std::string value = line.substr(colon_pos + 1);
        hwinfo::utils::strip(value);

        if (key.find("Manufacturer") != std::string::npos)
          report.manufacturer = value;
        else if (key.find("Model Number") != std::string::npos)
          report.model = value;
        else if (key.find("Serial Number") != std::string::npos)
          report.serial = value;
        else if (key.find("Chemistry") != std::string::npos)
          report.chemistry = value;
        else if (key.find("Design Capacity") != std::string::npos) {
          size_t mwh_pos = value.find(" mWh");
          if (mwh_pos != std::string::npos) {
            try {
              uint32_t mwh = std::stoul(value.substr(0, mwh_pos));
              report.design_voltage_mv = Config::instance().get_int("battery_default_voltage_mv", 11400);
              report.design_capacity_mah = (uint32_t)(mwh * 1000.0 / report.design_voltage_mv);
            } catch (...) {}
          }
        } else if (key.find("Design Voltage") != std::string::npos) {
          size_t mv_pos = value.find(" mV");
          if (mv_pos == std::string::npos) mv_pos = value.find("mV");
          if (mv_pos != std::string::npos) {
            try { report.design_voltage_mv = std::stoul(value.substr(0, mv_pos)); } catch (...) {}
          }
        } else if (key.find("Full Charge Capacity") != std::string::npos) {
          size_t mwh_pos = value.find(" mWh");
          if (mwh_pos != std::string::npos) {
            try {
              uint32_t mwh = std::stoul(value.substr(0, mwh_pos));
              uint32_t voltage = report.design_voltage_mv > 0 ? report.design_voltage_mv
                                : (uint32_t)Config::instance().get_int("battery_default_voltage_mv", 11400);
              report.full_charge_capacity_mah = (uint32_t)(mwh * 1000.0 / voltage);
            } catch (...) {}
          }
        } else if (key.find("Cycle Count") != std::string::npos) {
          try { report.cycle_count = std::stoul(value); } catch (...) {}
        }
      }
    }
  }

  if (report.design_capacity_mah > 0 && report.full_charge_capacity_mah > 0)
    report.health_percentage = (double)report.full_charge_capacity_mah / report.design_capacity_mah * 100.0;

  return report;
}

bool generate_battery_report(BatteryReport& report) {
  char temp_path[MAX_PATH];
  if (!GetTempPathA(MAX_PATH, temp_path)) return false;

  std::string report_path = std::string(temp_path) + "battery_report.html";
  std::string command = "powercfg /batteryreport /output \"" + report_path + "\"";

  SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
  HANDLE hNull = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_WRITE, &sa,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (hNull == INVALID_HANDLE_VALUE) return false;

  STARTUPINFOA si = { sizeof(si) };
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdOutput = hNull;
  si.hStdError = hNull;

  PROCESS_INFORMATION pi = {};
  if (!CreateProcessA(NULL, (LPSTR)command.c_str(), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
    CloseHandle(hNull);
    return false;
  }

  WaitForSingleObject(pi.hProcess, INFINITE);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
  CloseHandle(hNull);

  report = parse_battery_report(report_path);
  DeleteFileA(report_path.c_str());
  return !report.manufacturer.empty();
}

bool query_battery_from_wmi(BatteryInfo& info) {
  HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return false;

  IWbemLocator* locator = nullptr;
  hr = CoCreateInstance(__uuidof(WbemLocator), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&locator));
  if (FAILED(hr)) return false;

  IWbemServices* service = nullptr;
  hr = locator->ConnectServer(_bstr_t(L"ROOT\\WMI"), nullptr, nullptr, nullptr, 0, nullptr, nullptr, &service);
  if (FAILED(hr)) { locator->Release(); return false; }

  CoSetProxyBlanket(service, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                    RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);

  {
    IEnumWbemClassObject* enumerator = nullptr;
    hr = service->ExecQuery(_bstr_t(L"WQL"),
                            _bstr_t(L"SELECT * FROM BatteryStaticData"),
                            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                            nullptr, &enumerator);
    if (SUCCEEDED(hr) && enumerator) {
      ULONG u_return = 0;
      IWbemClassObject* obj = nullptr;
      enumerator->Next(WBEM_INFINITE, 1, &obj, &u_return);
      if (u_return && obj) {
        VARIANT vt;
        if (SUCCEEDED(obj->Get(L"ManufactureName", 0, &vt, nullptr, nullptr)) && V_VT(&vt) == VT_BSTR)
          info.manufacturer = ensure_utf8((const char*)_bstr_t(vt.bstrVal));
        VariantClear(&vt);
        if (SUCCEEDED(obj->Get(L"DeviceName", 0, &vt, nullptr, nullptr)) && V_VT(&vt) == VT_BSTR)
          info.device_name = ensure_utf8((const char*)_bstr_t(vt.bstrVal));
        VariantClear(&vt);
        if (SUCCEEDED(obj->Get(L"SerialNumber", 0, &vt, nullptr, nullptr)) && V_VT(&vt) == VT_BSTR)
          info.serial_number = ensure_utf8((const char*)_bstr_t(vt.bstrVal));
        VariantClear(&vt);
        if (SUCCEEDED(obj->Get(L"DesignedCapacity", 0, &vt, nullptr, nullptr)) && V_VT(&vt) == VT_I4)
          info.designed_capacity = vt.uintVal;
        VariantClear(&vt);
        obj->Release();
      }
      enumerator->Release();
    }
  }

  {
    IEnumWbemClassObject* enumerator = nullptr;
    hr = service->ExecQuery(_bstr_t(L"WQL"),
                            _bstr_t(L"SELECT * FROM BatteryStatus"),
                            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                            nullptr, &enumerator);
    if (SUCCEEDED(hr) && enumerator) {
      ULONG u_return = 0;
      IWbemClassObject* obj = nullptr;
      enumerator->Next(WBEM_INFINITE, 1, &obj, &u_return);
      if (u_return && obj) {
        VARIANT vt;
        if (SUCCEEDED(obj->Get(L"RemainingCapacity", 0, &vt, nullptr, nullptr)) && V_VT(&vt) == VT_I4)
          info.remaining_capacity = vt.uintVal;
        VariantClear(&vt);
        if (SUCCEEDED(obj->Get(L"Charging", 0, &vt, nullptr, nullptr)))
          info.charging = (vt.boolVal != VARIANT_FALSE);
        VariantClear(&vt);
        if (SUCCEEDED(obj->Get(L"PowerOnline", 0, &vt, nullptr, nullptr)))
          info.power_online = (vt.boolVal != VARIANT_FALSE);
        VariantClear(&vt);
        if (SUCCEEDED(obj->Get(L"ChargeRate", 0, &vt, nullptr, nullptr)) && V_VT(&vt) == VT_I4)
          info.charge_rate = vt.uintVal;
        VariantClear(&vt);
        if (SUCCEEDED(obj->Get(L"DischargeRate", 0, &vt, nullptr, nullptr)) && V_VT(&vt) == VT_I4)
          info.discharge_rate = vt.uintVal;
        VariantClear(&vt);
        if (SUCCEEDED(obj->Get(L"Voltage", 0, &vt, nullptr, nullptr)) && V_VT(&vt) == VT_I4)
          info.voltage = vt.uintVal;
        VariantClear(&vt);
        obj->Release();
      }
      enumerator->Release();
    }
  }

  {
    IEnumWbemClassObject* enumerator = nullptr;
    hr = service->ExecQuery(_bstr_t(L"WQL"),
                            _bstr_t(L"SELECT * FROM BatteryFullChargedCapacity"),
                            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                            nullptr, &enumerator);
    if (SUCCEEDED(hr) && enumerator) {
      ULONG u_return = 0;
      IWbemClassObject* obj = nullptr;
      enumerator->Next(WBEM_INFINITE, 1, &obj, &u_return);
      if (u_return && obj) {
        VARIANT vt;
        if (SUCCEEDED(obj->Get(L"FullChargedCapacity", 0, &vt, nullptr, nullptr)) && V_VT(&vt) == VT_I4)
          info.full_capacity = vt.uintVal;
        VariantClear(&vt);
        obj->Release();
      }
      enumerator->Release();
    }
  }

  {
    IEnumWbemClassObject* enumerator = nullptr;
    hr = service->ExecQuery(_bstr_t(L"WQL"),
                            _bstr_t(L"SELECT * FROM BatteryCycleCount"),
                            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                            nullptr, &enumerator);
    if (SUCCEEDED(hr) && enumerator) {
      ULONG u_return = 0;
      IWbemClassObject* obj = nullptr;
      enumerator->Next(WBEM_INFINITE, 1, &obj, &u_return);
      if (u_return && obj) {
        VARIANT vt;
        if (SUCCEEDED(obj->Get(L"CycleCount", 0, &vt, nullptr, nullptr)) && V_VT(&vt) == VT_I4)
          info.cycle_count = vt.uintVal;
        VariantClear(&vt);
        obj->Release();
      }
      enumerator->Release();
    }
  }

  service->Release();
  locator->Release();
  return info.designed_capacity > 0 || info.remaining_capacity > 0;
}

// ============================================================
// WiFi adapter detection via WMI
// ============================================================

std::vector<WifiAdapterInfo> scan_wifi_adapters() {
  std::vector<WifiAdapterInfo> adapters;
  HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return adapters;

  IWbemLocator* locator = nullptr;
  hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
                        IID_IWbemLocator, (LPVOID*)&locator);
  if (SUCCEEDED(hr) && locator) {
    IWbemServices* service = nullptr;
    hr = locator->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, nullptr,
                                0, nullptr, nullptr, &service);
    if (SUCCEEDED(hr) && service) {
      CoSetProxyBlanket(service, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                        RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
                        nullptr, EOAC_NONE);

      IEnumWbemClassObject* enumerator = nullptr;
      // AdapterTypeID = 9 → Wireless LAN (802.11)
      hr = service->ExecQuery(
          _bstr_t(L"WQL"),
          _bstr_t(L"SELECT Name, Description, NetConnectionID FROM Win32_NetworkAdapter "
                  L"WHERE PhysicalAdapter=TRUE AND AdapterTypeID=9"),
          WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
          nullptr, &enumerator);

      if (SUCCEEDED(hr) && enumerator) {
        IWbemClassObject* obj = nullptr;
        ULONG u_return = 0;
        while (enumerator->Next(WBEM_INFINITE, 1, &obj, &u_return) == S_OK && u_return) {
          WifiAdapterInfo info;
          VARIANT vt;
          VariantInit(&vt);
          if (SUCCEEDED(obj->Get(L"Name", 0, &vt, nullptr, nullptr)) && V_VT(&vt) == VT_BSTR)
            info.name = _bstr_t(vt.bstrVal);
          VariantClear(&vt);
          VariantInit(&vt);
          if (SUCCEEDED(obj->Get(L"Description", 0, &vt, nullptr, nullptr)) && V_VT(&vt) == VT_BSTR)
            info.description = _bstr_t(vt.bstrVal);
          VariantClear(&vt);
          VariantInit(&vt);
          if (SUCCEEDED(obj->Get(L"NetConnectionID", 0, &vt, nullptr, nullptr)) && V_VT(&vt) == VT_BSTR)
            info.connection_id = _bstr_t(vt.bstrVal);
          VariantClear(&vt);
          if (!info.name.empty()) adapters.push_back(info);
          obj->Release();
        }
        enumerator->Release();
      }
      service->Release();
    }
    locator->Release();
  }
  CoUninitialize();
  return adapters;
}

// ============================================================
// Bluetooth device detection via WMI
// ============================================================

std::vector<BluetoothDeviceInfo> scan_bluetooth_devices() {
  std::vector<BluetoothDeviceInfo> devices;
  HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return devices;

  IWbemLocator* locator = nullptr;
  hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
                        IID_IWbemLocator, (LPVOID*)&locator);
  if (SUCCEEDED(hr) && locator) {
    IWbemServices* service = nullptr;
    hr = locator->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, nullptr,
                                0, nullptr, nullptr, &service);
    if (SUCCEEDED(hr) && service) {
      CoSetProxyBlanket(service, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                        RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
                        nullptr, EOAC_NONE);

      IEnumWbemClassObject* enumerator = nullptr;
      hr = service->ExecQuery(
          _bstr_t(L"WQL"),
          _bstr_t(L"SELECT Name, Description FROM Win32_PnPEntity "
                  L"WHERE Name LIKE '%Bluetooth%' AND Status='OK'"),
          WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
          nullptr, &enumerator);

      if (SUCCEEDED(hr) && enumerator) {
        IWbemClassObject* obj = nullptr;
        ULONG u_return = 0;
        while (enumerator->Next(WBEM_INFINITE, 1, &obj, &u_return) == S_OK && u_return) {
          BluetoothDeviceInfo info;
          VARIANT vt;
          VariantInit(&vt);
          if (SUCCEEDED(obj->Get(L"Name", 0, &vt, nullptr, nullptr)) && V_VT(&vt) == VT_BSTR)
            info.name = _bstr_t(vt.bstrVal);
          VariantClear(&vt);
          VariantInit(&vt);
          if (SUCCEEDED(obj->Get(L"Description", 0, &vt, nullptr, nullptr)) && V_VT(&vt) == VT_BSTR)
            info.description = _bstr_t(vt.bstrVal);
          VariantClear(&vt);
          if (!info.name.empty()) devices.push_back(info);
          obj->Release();
        }
        enumerator->Release();
      }
      service->Release();
    }
    locator->Release();
  }
  CoUninitialize();
  return devices;
}
