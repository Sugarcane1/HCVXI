#include <hwinfo/hwinfo.h>
#include <hwinfo/monitoring/cpu.h>
#include <hwinfo/monitoring/disk.h>
#include <hwinfo/monitoring/monitor.h>
#include <hwinfo/monitoring/ram.h>
#include <hwinfo/utils/stringutils.h>
#include <hwinfo/utils/unit.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <setupapi.h>
#include <initguid.h>
#include <wbemidl.h>
#pragma comment(lib, "Advapi32.lib")
#endif

#include "common.h"
#include "logger.h"
#include "config.h"

using namespace hwinfo::unit;
using namespace hwdetect;
using namespace std::chrono_literals;

static std::atomic<bool> g_running{true};

#ifdef _WIN32
static BOOL WINAPI console_ctrl_handler(DWORD event) {
  if (event == CTRL_C_EVENT) {
    g_running = false;
    return TRUE;
  }
  return FALSE;
}
#else
static void signal_handler(int) { g_running = false; }
#endif


static void enable_ansi() {
#ifdef _WIN32
  HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD mode = 0;
  if (GetConsoleMode(h, &mode)) {
    SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
  }
#endif
}

static void print_separator(const std::string& title) {
  std::cout << "\n";
  std::cout << "================================================================================\n";
  std::cout << "  " << title << "\n";
  std::cout << "================================================================================\n";
}

static void print_field(const std::string& label, const std::string& value) {
  std::cout << "  " << std::left << std::setw(22) << label << ": " << value << "\n";
}

static std::string bytes_to_gib(uint64_t bytes) {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(2) << unit_prefix_to(bytes, IECPrefix::GIBI) << " GiB";
  return oss.str();
}


#ifdef _WIN32




std::vector<MonitorInfo> get_monitor_info() {
  std::vector<MonitorInfo> monitors;

  get_monitor_info_from_wmi(monitors);

  DISPLAY_DEVICEA dd = {0};
  dd.cb = sizeof(dd);

  bool found_wmi_monitor = !monitors.empty() && !monitors[0].manufacturer.empty();

  for (DWORD i = 0; EnumDisplayDevicesA(NULL, i, &dd, 0); i++) {
    if ((dd.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) == 0) {
      continue;
    }

    MonitorInfo mi;
    mi.name = dd.DeviceName;

    DISPLAY_DEVICEA dd_monitor = {0};
    dd_monitor.cb = sizeof(dd_monitor);
    
    if (EnumDisplayDevicesA(dd.DeviceName, 0, &dd_monitor, EDD_GET_DEVICE_INTERFACE_NAME)) {
      mi.display_name = dd_monitor.DeviceString;
    }

    DEVMODEA dm = {0};
    dm.dmSize = sizeof(dm);
    
    if (EnumDisplaySettingsA(dd.DeviceName, ENUM_CURRENT_SETTINGS, &dm)) {
      mi.width = dm.dmPelsWidth;
      mi.height = dm.dmPelsHeight;
      mi.refresh_rate = dm.dmDisplayFrequency;
      mi.bits_per_pixel = dm.dmBitsPerPel;
    }

    if (found_wmi_monitor && !monitors.empty()) {
      monitors[0].width = mi.width;
      monitors[0].height = mi.height;
      monitors[0].refresh_rate = mi.refresh_rate;
      monitors[0].bits_per_pixel = mi.bits_per_pixel;
      if (monitors[0].display_name.empty()) {
        monitors[0].display_name = mi.display_name;
      }
    } else {
      monitors.push_back(mi);
    }
  }

  return monitors;
}
#endif

void print_system_info() {
  hwinfo::OS os;
  print_separator("Operating System");
  print_field("Name", os.name());
  print_field("Version", os.version());
  print_field("Kernel", os.kernel());
  std::string bitness = os.is64bit() ? "64-bit" : (os.is32bit() ? "32-bit" : "Unknown");
  print_field("Architecture", bitness);
  print_field("Endianness", os.isLittleEndian() ? "Little Endian" : "Big Endian");

  hwinfo::MainBoard mb;
  print_separator("Mainboard");
  print_field("Vendor", mb.vendor());
  print_field("Name", mb.name());
  print_field("Version", mb.version());
  print_field("Serial Number", mb.serialNumber());

  const auto cpus = hwinfo::getAllCPUs();
  for (const auto& cpu : cpus) {
    print_separator("CPU - Socket " + std::to_string(cpu.id()));
    print_field("Vendor", cpu.vendor());
    print_field("Model", cpu.modelName());
    print_field("Physical Cores", std::to_string(cpu.numPhysicalCores()));
    print_field("Logical Cores", std::to_string(cpu.numLogicalCores()));

    std::vector<std::string> flags = cpu.flags();
    if (!flags.empty()) {
      std::cout << "  " << std::left << std::setw(22) << "Flags" << ": ";
      int count = 0;
      for (size_t i = 0; i < flags.size() && count < 60; ++i) {
        const auto& f = flags[i];
        if (f == "sse3" || f == "ssse3" || f == "sse4_1" || f == "sse4_2" ||
            f == "avx" || f == "avx2" || f == "avx512f" || f == "aes" ||
            f == "fma" || f == "mmx" || f == "sse" || f == "sse2" ||
            f == "f16c" || f == "bmi1" || f == "bmi2" || f == "rdrand") {
          std::cout << f << " ";
          ++count;
        }
      }
      std::cout << "\n";
    }

    const auto& first_core = cpu.cores().front();
    print_field("Cache L1 Data", [&]() {
      std::ostringstream oss;
      oss << std::fixed << std::setprecision(0) << unit_prefix_to(first_core.cache.l1_data, IECPrefix::KIBI) << " KiB";
      return oss.str();
    }());
    print_field("Cache L1 Inst", [&]() {
      std::ostringstream oss;
      oss << std::fixed << std::setprecision(0) << unit_prefix_to(first_core.cache.l1_instruction, IECPrefix::KIBI) << " KiB";
      return oss.str();
    }());
    print_field("Cache L2", [&]() {
      std::ostringstream oss;
      oss << std::fixed << std::setprecision(0) << unit_prefix_to(first_core.cache.l2, IECPrefix::KIBI) << " KiB";
      return oss.str();
    }());
    if (first_core.cache.l3 > 0) {
      print_field("Cache L3", [&]() {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << unit_prefix_to(first_core.cache.l3, IECPrefix::MEBI) << " MiB";
        return oss.str();
      }());
    }
  }

  const auto gpus = hwinfo::getAllGPUs();
  if (!gpus.empty()) {
    for (const auto& gpu : gpus) {
      print_separator("GPU " + std::to_string(gpu.id()));
      print_field("Vendor", gpu.vendor());
      print_field("Model", gpu.name());
      print_field("Driver Version", gpu.driverVersion());
      print_field("Dedicated VRAM", bytes_to_gib(gpu.dedicated_memory_Bytes()));
      print_field("Shared Memory", bytes_to_gib(gpu.shared_memory_Bytes()));
      print_field("Frequency", hz_to_mhz(gpu.frequency_hz()));
      print_field("Compute Units", std::to_string(gpu.num_cores()));
      print_field("Vendor ID", gpu.vendor_id());
      print_field("Device ID", gpu.device_id());
    }
  } else {
    print_separator("GPU");
    std::cout << "  No GPU detected.\n";
  }

  hwinfo::Memory mem;
  print_separator("Memory (RAM)");
  print_field("Total Size", bytes_to_gib(mem.size()));
  print_field("Free", bytes_to_gib(mem.free()));
  print_field("Available", bytes_to_gib(mem.available()));
  for (const auto& mod : mem.modules()) {
    std::cout << "  --- Module " << mod.id << " ---\n";
    print_field("  Vendor", mod.vendor);
    print_field("  Model", mod.model);
    print_field("  Name", mod.name);
    print_field("  Serial Number", mod.serial_number);
    print_field("  Size", bytes_to_gib(mod._size_bytes));
    print_field("  Frequency", hz_to_mhz(mod.frequency_hz));
  }

  // Disk info is provided by CrystalDiskInfo section below

  const auto batteries = hwinfo::getAllBatteries();
  if (!batteries.empty()) {
    for (const auto& bat : batteries) {
      print_separator("Battery " + std::to_string(bat.id()));
      print_field("Vendor", bat.vendor());
      print_field("Model", bat.model());
      print_field("Serial Number", bat.serialNumber());
      print_field("Technology", bat.technology());
      print_field("Capacity", [&]() {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << bat.capacity() << " %";
        return oss.str();
      }());
      print_field("Energy Full", std::to_string(bat.energyFull()) + " Wh");
      print_field("Energy Now", std::to_string(bat.energyNow()) + " Wh");
      print_field("State", [&]() {
        std::ostringstream oss;
        oss << bat.state();
        return oss.str();
      }());
    }
  }

#ifdef _WIN32
  BatteryReport bat_report;
  if (generate_battery_report(bat_report)) {
    print_separator("Battery Detailed Report (powercfg)");
    print_field("Manufacturer", bat_report.manufacturer);
    print_field("Model", bat_report.model);
    print_field("Serial Number", bat_report.serial);
    print_field("Chemistry", bat_report.chemistry);
    if (bat_report.design_capacity_mah > 0) {
      print_field("Design Capacity", std::to_string(bat_report.design_capacity_mah) + " mAh");
    }
    if (bat_report.full_charge_capacity_mah > 0) {
      print_field("Full Charge Capacity", std::to_string(bat_report.full_charge_capacity_mah) + " mAh");
    }
    if (bat_report.cycle_count > 0) {
      print_field("Cycle Count", std::to_string(bat_report.cycle_count));
    }
    if (bat_report.health_percentage > 0) {
      std::ostringstream health_ss;
      health_ss << std::fixed << std::setprecision(1) << bat_report.health_percentage << " %";
      print_field("Health", health_ss.str());
    }
  }

  std::vector<MonitorInfo> monitors = get_monitor_info();
  if (!monitors.empty()) {
    int monitor_count = 0;
    for (const auto& monitor : monitors) {
      print_separator("Display " + std::to_string(monitor_count++));
      if (!monitor.display_name.empty()) {
        print_field("Display Name", monitor.display_name);
      }
      if (!monitor.manufacturer.empty()) {
        print_field("Manufacturer", translate_brand(monitor.manufacturer));
      }
      if (!monitor.model.empty()) {
        print_field("Model", monitor.model);
      }
      if (monitor.width > 0 && monitor.height > 0) {
        print_field("Resolution", std::to_string(monitor.width) + " x " + std::to_string(monitor.height));
      }
      if (monitor.refresh_rate > 0) {
        print_field("Refresh Rate", std::to_string(monitor.refresh_rate) + " Hz");
      }
      if (monitor.bits_per_pixel > 0) {
        print_field("Bits per Pixel", std::to_string(monitor.bits_per_pixel) + " bpp");
      }
      if (monitor.physical_width_mm > 0 && monitor.physical_height_mm > 0) {
        double diagonal = sqrt(monitor.physical_width_mm * monitor.physical_width_mm + 
                               monitor.physical_height_mm * monitor.physical_height_mm);
        double diagonal_inch = diagonal / 25.4;
        std::ostringstream size_ss;
        size_ss << std::fixed << std::setprecision(1) << diagonal_inch << "\" (" 
                << monitor.physical_width_mm << " x " << monitor.physical_height_mm << " mm)";
        print_field("Physical Size", size_ss.str());
      } else if (monitor.width > 0 && monitor.height > 0) {
        double aspect_ratio = (double)monitor.width / monitor.height;
        double inch_size = 27.0;
        double mm_size = inch_size * 25.4;
        double height_ratio = 1.0 / sqrt(1.0 + aspect_ratio * aspect_ratio);
        uint32_t phys_height = (uint32_t)(mm_size * height_ratio);
        uint32_t phys_width = (uint32_t)(phys_height * aspect_ratio);
        std::ostringstream size_ss;
        size_ss << std::fixed << std::setprecision(1) << inch_size << "\" (" 
                << phys_width << " x " << phys_height << " mm)";
        print_field("Physical Size", size_ss.str());
      }
    }
  }
#endif

  const auto networks = hwinfo::getAllNetworks();
  if (!networks.empty()) {
    int net_count = 0;
    for (const auto& net : networks) {
      if (!net.ip4().empty() || !net.ip6().empty()) {
        print_separator("Network Adapter " + std::to_string(net_count++));
        print_field("Description", net.description());
        print_field("Interface Index", net.interfaceIndex());
        print_field("MAC Address", net.mac());
        if (!net.ip4().empty()) print_field("IPv4 Address", net.ip4());
        if (!net.ip6().empty()) print_field("IPv6 Address", net.ip6());
      }
    }
  }
}

static std::string bar(double ratio, int width = 20) {
  const int filled = std::max(0, std::min(width, static_cast<int>(ratio * width + 0.5)));
  return '[' + std::string(filled, '#') + std::string(width - filled, '.') + ']';
}

struct Snapshot {
  hwinfo::monitoring::cpu::Data cpu;
  hwinfo::monitoring::ram::Data ram;
  std::vector<hwinfo::monitoring::disk::Data> disks;
};

void run_monitor() {
#ifdef _WIN32
  SetConsoleCtrlHandler(console_ctrl_handler, TRUE);
#else
  std::signal(SIGINT, signal_handler);
#endif

  const auto cpus = hwinfo::getAllCPUs();
  const auto disks = hwinfo::getAllDisks();
  hwinfo::Memory memory;

  std::vector<std::string> mount_points;
  for (const auto& disk : disks) {
    for (const auto& mp : disk.mount_points()) {
      mount_points.push_back(mp);
    }
  }

  std::cout << "\n========== Live Hardware Monitor (Ctrl+C to quit) ==========\n\n";
  for (const auto& cpu : cpus) {
    std::cout << "CPU : " << cpu.vendor() << " " << cpu.modelName()
              << "  (" << cpu.numPhysicalCores() << "C / " << cpu.numLogicalCores() << "T)\n";
  }
  std::cout << "RAM : " << std::fixed << std::setprecision(1)
            << unit_prefix_to(memory.size(), IECPrefix::GIBI) << " GiB total\n";
  for (const auto& disk : disks) {
    std::cout << "Disk: [" << disk.id() << "] " << disk.model() << "  "
              << unit_prefix_to(disk.size(), IECPrefix::GIBI) << " GiB\n";
  }
  std::cout << std::endl;

  std::mutex render_mtx;
  int prev_lines = 0;

  auto render = [&](const Snapshot& s) {
    std::ostringstream out;
    int lines = 0;

    out << std::fixed;

    out << "CPU  Usage : " << bar(s.cpu.utilization, 30) << " "
        << std::setw(6) << std::setprecision(1) << s.cpu.utilization * 100.0 << " %\n";
    ++lines;

    for (size_t i = 0; i < s.cpu.thread_utilization.size(); ++i) {
      const double u = s.cpu.thread_utilization[i];
      out << "  Thread " << std::setw(2) << std::setfill('0') << i << std::setfill(' ')
          << ": " << bar(u, 20) << " " << std::setw(6) << std::setprecision(1) << u * 100.0 << " %";
      if (i < s.cpu.thread_frequency_hz.size() && s.cpu.thread_frequency_hz[i] > 0) {
        out << " @ " << std::setw(4)
            << static_cast<int>(unit_prefix_to(s.cpu.thread_frequency_hz[i], SiPrefix::MEGA) + 0.5)
            << " MHz";
      }
      out << "\n";
      ++lines;
    }

    out << "RAM  Free : " << std::setw(7) << std::setprecision(2)
        << unit_prefix_to(s.ram.free_bytes, IECPrefix::GIBI) << " GiB";
    out << "   Available: " << std::setw(7)
        << unit_prefix_to(s.ram.available_bytes, IECPrefix::GIBI) << " GiB\n";
    ++lines;

    for (const auto& d : s.disks) {
      out << "Disk [" << d.mount_point << "] Free: " << std::setw(7)
          << unit_prefix_to(d.free_bytes, IECPrefix::GIBI) << " GiB\n";
      ++lines;
    }

    std::lock_guard<std::mutex> lock(render_mtx);
    if (prev_lines > 0) {
      std::cout << "\033[" << prev_lines << 'A';
    }
    std::cout << out.str();
    std::cout.flush();
    prev_lines = lines;
  };

  auto fetch_all = [&mount_points]() -> Snapshot {
    Snapshot s;
    s.cpu = hwinfo::monitoring::cpu::fetch(200ms);
    s.ram = hwinfo::monitoring::ram::fetch();
    for (const auto& mp : mount_points) {
      s.disks.push_back(hwinfo::monitoring::disk::fetch(mp));
    }
    return s;
  };

  hwinfo::monitoring::Monitor<Snapshot> monitor(fetch_all, render, 1s);
  monitor.start();

  while (g_running) {
    std::this_thread::sleep_for(100ms);
  }

  monitor.stop();
  std::cout << "\nMonitor stopped.\n";
}

void print_usage(const char* prog) {
  std::cout << "Usage: " << prog << " [OPTION]\n"
            << "Hardware detection tool powered by hwinfo library.\n\n"
            << "Options:\n"
            << "  --monitor    Run live hardware monitor mode (CPU/RAM/Disk usage)\n"
            << "  --help       Display this help message\n"
            << "\n"
            << "Without any option, displays all static hardware information.\n";
}

#ifdef _WIN32
void request_elevation(int argc, char* argv[]) {
  if (is_admin()) {
    return;
  }

  char exePath[MAX_PATH];
  GetModuleFileNameA(NULL, exePath, MAX_PATH);
  char tempPath[MAX_PATH];
  GetTempPathA(MAX_PATH, tempPath);
  
  std::string resultFile = std::string(tempPath) + "hwinfo_elevated.txt";

  std::string params = "--elevated-result \"" + resultFile + "\"";
  
  SHELLEXECUTEINFOA sei = {0};
  sei.cbSize = sizeof(SHELLEXECUTEINFOA);
  sei.lpVerb = "runas";
  sei.lpFile = exePath;
  sei.lpParameters = params.c_str();
  sei.nShow = SW_HIDE;

  std::cout << "\n=================================================================\n";
  std::cout << "  [INFO] Requesting administrator privileges for\n";
  std::cout << "         CrystalDiskInfo integration...\n";
  std::cout << "=================================================================\n\n";

  if (!ShellExecuteExA(&sei)) {
    DWORD error = GetLastError();
    if (error == ERROR_CANCELLED) {
      std::cout << "  [*] UAC cancelled. Running with WMI-based disk info.\n\n";
    } else {
      std::cout << "  [*] Elevation failed (Error: " << error << "). Using WMI fallback.\n\n";
    }
    return;
  }

  std::cout << "  [*] Waiting for elevated process...\n";
  
  DWORD wait_result = WaitForSingleObject(sei.hProcess, 60000);
  
  if (wait_result != WAIT_OBJECT_0) {
    std::cout << "  [*] Elevated process timed out. Terminating it.\n";
    TerminateProcess(sei.hProcess, 1);
  }
  
  CloseHandle(sei.hProcess);

  Sleep(1000);

  std::ifstream file(resultFile);
  if (file.is_open()) {
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    
    std::string content = buffer.str();
    if (!content.empty()) {
      parse_diskinfo_output(content, g_disk_details);
      
      DeleteFileA(resultFile.c_str());
      
      if (wait_result != WAIT_OBJECT_0) {
        std::cout << "  [*] CrystalDiskInfo data loaded (after timeout)!\n\n";
      } else {
        std::cout << "  [*] CrystalDiskInfo data loaded successfully!\n\n";
      }
      return;
    }
  }

  DeleteFileA(resultFile.c_str());
  
  if (wait_result != WAIT_OBJECT_0) {
    std::cout << "  [*] Using WMI fallback.\n\n";
  } else {
    std::cout << "  [*] Could not get CrystalDiskInfo data. Using WMI fallback.\n\n";
  }
}
#endif

int main(int argc, char* argv[]) {
  Config::instance().load("config.json");
  enable_ansi();

#ifdef _WIN32
  if (argc > 2 && std::strcmp(argv[1], "--elevated-result") == 0) {
    std::string diskinfo_exe = find_crystal_disk_info();
    std::string resultFile = argv[2];
    
    if (!diskinfo_exe.empty()) {
      std::string working_dir = diskinfo_exe.substr(0, diskinfo_exe.find_last_of("\\/"));
      std::string output_file = working_dir + "\\DiskInfo.txt";

      SECURITY_ATTRIBUTES sa = {0};
      sa.nLength = sizeof(sa);
      sa.bInheritHandle = TRUE;

      HANDLE hNull = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_WRITE, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

      STARTUPINFOA si = {0};
      si.cb = sizeof(si);
      si.dwFlags = STARTF_USESTDHANDLES;
      si.hStdOutput = hNull;
      si.hStdError = hNull;

      PROCESS_INFORMATION pi = {0};
      std::string command = "\"" + diskinfo_exe + "\" /CopyExit";

      BOOL success = CreateProcessA(NULL, (LPSTR)command.c_str(), NULL, NULL, TRUE,
                                     0, NULL, working_dir.c_str(), &si, &pi);

      CloseHandle(hNull);

      if (success) {
        DWORD wait_result = WaitForSingleObject(pi.hProcess, 20000);
        if (wait_result != WAIT_OBJECT_0) {
          TerminateProcess(pi.hProcess, 1);
        }
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        Sleep(1000);

        if (GetFileAttributesA(output_file.c_str()) != INVALID_FILE_ATTRIBUTES) {
          std::ifstream src(output_file, std::ios::binary);
          if (src.is_open()) {
            std::ofstream dst(resultFile, std::ios::binary);
            dst << src.rdbuf();
            src.close();
            dst.close();
            DeleteFileA(output_file.c_str());
          }
        }
      }
    } else {
      get_disk_details_wmi();
      std::ofstream outFile(resultFile);
      if (outFile.is_open()) {
        outFile << "=== WMI Fallback ===\n";
        for (const auto& disk : g_disk_details) {
          if (disk.loaded) {
            outFile << "Model : " << disk.model << "\n";
            outFile << "Serial Number : " << disk.serial << "\n";
            outFile << "Firmware : " << disk.firmware << "\n";
            outFile << "Disk Size : " << disk.disk_size << "\n";
            outFile << "Interface : " << disk.interface_type << "\n";
            outFile << "Health Status : " << disk.health_status << "\n\n";
          }
        }
        outFile.close();
      }
    }
    return 0;
  }

  request_elevation(argc, argv);
#endif

  if (argc > 1) {
    if (std::strcmp(argv[1], "--help") == 0 || std::strcmp(argv[1], "-h") == 0) {
      print_usage(argv[0]);
      return 0;
    }
    if (std::strcmp(argv[1], "--monitor") == 0 || std::strcmp(argv[1], "-m") == 0) {
      run_monitor();
      return 0;
    }
    print_usage(argv[0]);
    return 1;
  }

#ifdef _WIN32
  bool has_disk_data = false;
  {
    std::lock_guard<std::mutex> lock(g_disk_mutex);
    has_disk_data = !g_disk_details.empty() && g_disk_details[0].loaded;
  }

  if (!has_disk_data) {
    std::cout << "  [Loading disk details via WMI...]\n";
    std::thread disk_thread(get_disk_details_async);
    disk_thread.join();
  } else {
    std::cout << "  [Disk details already loaded via CrystalDiskInfo.]\n";
  }
#endif

  std::cout << "\n"
            << "  _    _  __        __ _       \n"
            << " | |  | | \\ \\      / //_|      \n"
            << " | |__| |  \\ \\ /\\ / / |_ _ __  \n"
            << " |  __  |   \\ V  V /|  _| '_ \\ \n"
            << " | |  | |    \\_/\\_/ | | | | | |\n"
            << " |_|  |_|           |_| |_| |_|\n"
            << "          Hardware Detection\n"
            << "========================================\n"
            << std::endl;

  print_system_info();

#ifdef _WIN32
  {
    std::lock_guard<std::mutex> lock(g_disk_mutex);
    if (!g_disk_details.empty()) {
      for (size_t i = 0; i < g_disk_details.size(); i++) {
        const auto& detail = g_disk_details[i];
        if (detail.loaded) {
          print_separator("Disk " + std::to_string(i) + " - CrystalDiskInfo");
          print_field("Model", detail.model);
          if (!detail.serial.empty()) {
            print_field("Serial", detail.serial);
          }
          if (!detail.firmware.empty()) {
            print_field("Firmware", detail.firmware);
          }
          if (!detail.disk_size.empty()) {
            print_field("Disk Size", detail.disk_size);
          }
          if (!detail.interface_type.empty()) {
            print_field("Interface", detail.interface_type);
          }
          if (!detail.transfer_mode.empty()) {
            print_field("Transfer Mode", detail.transfer_mode);
          }
          if (!detail.standard.empty()) {
            print_field("Standard", detail.standard);
          }
          if (!detail.health_status.empty()) {
            print_field("Health Status", detail.health_status);
          }
          if (!detail.temperature.empty()) {
            print_field("Temperature", detail.temperature);
          }
          if (!detail.power_on_hours.empty()) {
            print_field("Power On Hours", detail.power_on_hours + " hours");
          }
          if (!detail.power_on_count.empty()) {
            print_field("Power On Count", detail.power_on_count);
          }
          if (!detail.total_host_reads.empty()) {
            print_field("Total Host Reads", detail.total_host_reads);
          }
          if (!detail.total_host_writes.empty()) {
            print_field("Total Host Writes", detail.total_host_writes);
          }
          if (!detail.rotation_rate.empty()) {
            print_field("Rotation Rate", detail.rotation_rate);
          }
          if (!detail.features.empty()) {
            print_field("Features", detail.features);
          }
          if (!detail.drive_letter.empty()) {
            print_field("Drive Letter", detail.drive_letter);
          }
        }
      }
    }
  }
#endif

  std::cout << "\n================================================================================\n";
  std::cout << "  Detection Complete.\n";
  std::cout << "================================================================================\n";
  std::cout << "  Tip: Run with --monitor for live hardware monitoring.\n\n";

  return 0;
}
