#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <mutex>

// ============================================================
// Shared data structures
// ============================================================

struct DiskDetailInfo {
  std::string model;
  std::string serial;
  std::string firmware;
  std::string interface_type;
  std::string disk_size;
  std::string media_type;
  std::string health_status;
  std::string temperature;
  std::string power_on_hours;
  std::string power_on_count;
  std::string total_host_reads;
  std::string total_host_writes;
  std::string rotation_rate;
  std::string transfer_mode;
  std::string standard;
  std::string features;
  std::string drive_letter;
  bool loaded = false;
};

struct MonitorInfo {
  std::string name;
  std::string manufacturer;
  std::string model;
  std::string serial;
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t refresh_rate = 0;
  uint32_t bits_per_pixel = 0;
  uint32_t physical_width_mm = 0;
  uint32_t physical_height_mm = 0;
  std::string display_name;
};

struct BatteryReport {
  std::string manufacturer;
  std::string model;
  std::string serial;
  std::string chemistry;
  uint32_t design_capacity_mah = 0;
  uint32_t full_charge_capacity_mah = 0;
  uint32_t current_capacity_mah = 0;
  double health_percentage = 0.0;
  uint32_t cycle_count = 0;
  std::string last_full_charge_date;
  uint32_t design_voltage_mv = 0;
  std::vector<std::pair<std::string, uint32_t>> usage_history;
};

struct BatteryInfo {
  std::string manufacturer;
  std::string device_name;
  std::string serial_number;
  std::string chemistry;
  uint32_t designed_capacity = 0;
  uint32_t full_capacity = 0;
  uint32_t remaining_capacity = 0;
  uint32_t cycle_count = 0;
  uint32_t charge_rate = 0;
  uint32_t discharge_rate = 0;
  uint32_t voltage = 0;
  bool charging = false;
  bool power_online = false;
};

// ============================================================
// Global shared state
// ============================================================

extern std::vector<DiskDetailInfo> g_disk_details;
extern std::mutex g_disk_mutex;

// ============================================================
// Encoding helpers
// ============================================================

std::string trim_str(const std::string& s);
std::string ensure_utf8(const std::string& str);
std::string gbk_to_utf8(const std::string& gbk_str);
bool is_valid_utf8(const std::string& str);

#ifdef _WIN32
std::wstring utf8_to_wide(const std::string& str);
std::wstring ansi_to_wide(const std::string& str);
#endif

// ============================================================
// Brand translation (English -> Chinese)
// ============================================================

std::string translate_brand(const std::string& brand);

// ============================================================
// Formatting
// ============================================================

std::string hz_to_mhz(uint64_t hz);
std::string bytes_to_gib(uint64_t bytes);

// ============================================================
// Disk detection
// ============================================================

std::string find_crystal_disk_info();
void parse_diskinfo_output(const std::string& content, std::vector<DiskDetailInfo>& disks);
void get_disk_details_wmi();
void get_disk_details_async();

// ============================================================
// Monitor detection (WMI)
// ============================================================

bool get_monitor_info_from_wmi(std::vector<MonitorInfo>& monitors);

// ============================================================
// Battery detection
// ============================================================

#ifdef _WIN32
BatteryReport parse_battery_report(const std::string& file_path);
bool generate_battery_report(BatteryReport& report);
bool query_battery_from_wmi(BatteryInfo& info);
#endif

// ============================================================
// WiFi & Bluetooth detection (WMI)
// ============================================================

#ifdef _WIN32
struct WifiAdapterInfo {
  std::string name;
  std::string description;
  std::string connection_id;
};

struct BluetoothDeviceInfo {
  std::string name;
  std::string description;
};

std::vector<WifiAdapterInfo> scan_wifi_adapters();
std::vector<BluetoothDeviceInfo> scan_bluetooth_devices();
#endif

// ============================================================
// Admin check
// ============================================================

#ifdef _WIN32
bool is_admin();
#endif
