#include <hwinfo/hwinfo.h>
#include <hwinfo/utils/unit.h>
#include <hwinfo/monitoring/cpu.h>

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <comdef.h>
#include <wbemidl.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <atomic>
#include <vector>
#include <cmath>
#include <fstream>
#include <functional>
#include <mutex>
#include <thread>
#include <WebView2.h>
#include <wrl/client.h>
#include <wrl/event.h>
#include <dwmapi.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <mmsystem.h>

#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "winmm.lib")

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

using namespace Microsoft::WRL;

#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include "common.h"
#include "logger.h"
#include "config.h"

static HWND g_hWnd = nullptr;
#define WM_REFRESH_DISK (WM_USER + 1)
#define WM_HW_INFO_READY (WM_USER + 2)

using namespace hwinfo::unit;
using namespace hwdetect;





#pragma comment(lib, "comctl32.lib")
#pragma execution_character_set("utf-8")

static const int WINDOW_WIDTH = 1200;
static const int WINDOW_HEIGHT = 750;

static std::atomic<bool> g_refreshing{false};

static const COLORREF CLR_PAPER        = RGB(247, 244, 233);
static const COLORREF CLR_PAPER_WARM   = RGB(242, 238, 225);
static const COLORREF CLR_INK_MAIN     = RGB(42, 38, 35);
static const COLORREF CLR_INK_LIGHT    = RGB(100, 92, 82);
static const COLORREF CLR_INK_FAINT    = RGB(160, 150, 138);
static const COLORREF CLR_BORDER_INK   = RGB(209, 201, 182);
static const COLORREF CLR_SHADOW_WASH  = RGB(220, 214, 200);
static const COLORREF CLR_CINNABAR     = RGB(180, 76, 50);
static const COLORREF CLR_INDIGO       = RGB(62, 90, 108);
static const COLORREF CLR_TITLE_INK    = RGB(62, 56, 50);
static const COLORREF CLR_LABEL_INK    = RGB(110, 100, 88);
static const COLORREF CLR_VALUE_INK    = RGB(42, 38, 35);
static const COLORREF CLR_PLACE_INK    = RGB(170, 162, 150);
static const COLORREF CLR_DIVIDER_INK  = RGB(190, 182, 168);
static const COLORREF CLR_SEAL_RED     = RGB(180, 76, 50);
static const COLORREF CLR_BTN_TEXT     = RGB(140, 132, 120);
static const COLORREF CLR_BTN_HOVER    = RGB(100, 92, 82);

static ICoreWebView2* g_webView = nullptr;
static ICoreWebView2Controller* g_webViewController = nullptr;
static std::string g_currentModel;
static std::atomic<bool> g_shutting_down{false};
static std::atomic<bool> g_webview_ready{false};
static std::mutex g_threads_mutex;
static std::vector<std::thread> g_worker_threads;
static std::mutex g_pending_mutex;
static std::string g_pending_json;

// ===== Speaker test: synthesize chime and play via waveOut =====
// Generates a pleasant "ding" chime (880Hz + harmonics, exponential decay)
// and plays it on the specified stereo channel (left or right).
static void play_speaker_chime(bool left_channel) {
    const int SAMPLE_RATE = 44100;
    const double DURATION = 1.6; // seconds
    const int NUM_SAMPLES = (int)(SAMPLE_RATE * DURATION);
    const int NUM_CHANNELS = 2;
    const int BYTES_PER_SAMPLE = 2; // 16-bit PCM
    const int BLOCK_ALIGN = NUM_CHANNELS * BYTES_PER_SAMPLE;

    // Allocate stereo PCM buffer
    int totalFrames = NUM_SAMPLES;
    int totalBytes = totalFrames * BLOCK_ALIGN;
    short* pcm = new short[totalFrames * NUM_CHANNELS];

    // Synthesize chime: 880Hz fundamental + harmonics with exponential decay
    const double freq = 880.0;
    struct Harmonic { double ratio; double amp; };
    Harmonic harmonics[] = {
        {1.0,   1.0},
        {2.0,   0.45},
        {3.0,   0.20},
        {4.5,   0.08}
    };

    for (int i = 0; i < totalFrames; i++) {
        double t = (double)i / SAMPLE_RATE;
        double env = std::exp(-t * 3.0); // exponential decay, ~1s audible
        double sample = 0.0;
        for (int h = 0; h < 4; h++) {
            sample += harmonics[h].amp * std::sin(2.0 * 3.14159265358979323846 * freq * harmonics[h].ratio * t);
        }
        short val = (short)(sample * env * 10000.0); // master volume
        if (left_channel) {
            pcm[i * 2 + 0] = val;   // left
            pcm[i * 2 + 1] = 0;     // right silent
        } else {
            pcm[i * 2 + 0] = 0;     // left silent
            pcm[i * 2 + 1] = val;   // right
        }
    }

    // Set up WAVEFORMATEX for stereo 16-bit PCM
    WAVEFORMATEX wfx = {};
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = NUM_CHANNELS;
    wfx.nSamplesPerSec = SAMPLE_RATE;
    wfx.nAvgBytesPerSec = SAMPLE_RATE * BLOCK_ALIGN;
    wfx.nBlockAlign = BLOCK_ALIGN;
    wfx.wBitsPerSample = 16;
    wfx.cbSize = 0;

    HWAVEOUT hWaveOut = nullptr;
    MMRESULT mr = waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL);
    if (mr != MMSYSERR_NOERROR) {
        delete[] pcm;
        return;
    }

    WAVEHDR wh = {};
    wh.lpData = (LPSTR)pcm;
    wh.dwBufferLength = totalBytes;
    wh.dwFlags = 0;
    waveOutPrepareHeader(hWaveOut, &wh, sizeof(WAVEHDR));
    waveOutWrite(hWaveOut, &wh, sizeof(WAVEHDR));

    // Wait for playback to finish (non-blocking: spin up a detached thread)
    std::thread([hWaveOut, wh, pcm]() mutable {
        // Wait for playback to complete
        while (!(wh.dwFlags & WHDR_DONE)) {
            Sleep(50);
        }
        waveOutUnprepareHeader(hWaveOut, &wh, sizeof(WAVEHDR));
        waveOutClose(hWaveOut);
        delete[] pcm;
    }).detach();
}

static void join_worker_threads() {
  g_shutting_down.store(true);
  std::lock_guard<std::mutex> lock(g_threads_mutex);
  for (auto& t : g_worker_threads) {
    if (t.joinable()) t.join();
  }
  g_worker_threads.clear();
}

static void launch_worker(std::function<void()> fn) {
  std::lock_guard<std::mutex> lock(g_threads_mutex);
  // Clean up finished threads
  g_worker_threads.erase(
    std::remove_if(g_worker_threads.begin(), g_worker_threads.end(),
                   [](std::thread& t) {
                     if (!t.joinable()) return true;
                     // Check if thread has finished (non-blocking)
                     auto id = t.get_id();
                     (void)id;
                     return false;
                   }),
    g_worker_threads.end());
  g_worker_threads.emplace_back(std::move(fn));
}

static std::string get_system_model() {
  std::string model;
  HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if (SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE) {
    IWbemLocator* locator = nullptr;
    hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&locator);
    if (SUCCEEDED(hr) && locator) {
      IWbemServices* service = nullptr;
      hr = locator->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, nullptr, 0, nullptr, nullptr, &service);
      if (SUCCEEDED(hr) && service) {
        CoSetProxyBlanket(service, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                          RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
        IEnumWbemClassObject* enumerator = nullptr;
        hr = service->ExecQuery(_bstr_t(L"WQL"), _bstr_t(L"SELECT Model, Manufacturer FROM Win32_ComputerSystem"),
                                WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &enumerator);
        if (SUCCEEDED(hr) && enumerator) {
          ULONG u_return = 0;
          IWbemClassObject* obj = nullptr;
          enumerator->Next(WBEM_INFINITE, 1, &obj, &u_return);
          if (u_return && obj) {
            VARIANT vt;
            VariantInit(&vt);
            hr = obj->Get(L"Manufacturer", 0, &vt, nullptr, nullptr);
            if (SUCCEEDED(hr) && V_VT(&vt) == VT_BSTR) {
              std::string mfg = _bstr_t(vt.bstrVal);
              if (!mfg.empty()) model = mfg;
            }
            VariantClear(&vt);
            VariantInit(&vt);
            hr = obj->Get(L"Model", 0, &vt, nullptr, nullptr);
            if (SUCCEEDED(hr) && V_VT(&vt) == VT_BSTR) {
              std::string m = _bstr_t(vt.bstrVal);
              if (!m.empty()) {
                if (!model.empty()) model += " ";
                model += m;
              }
            }
            VariantClear(&vt);
            obj->Release();
          }
          enumerator->Release();
        }
        service->Release();
      }
      locator->Release();
    }
    CoUninitialize();
  }
  if (!model.empty()) return model;
  hwinfo::MainBoard mb;
  std::string name = mb.name();
  if (!name.empty()) return name;
  return "Unknown";
}

struct InfoLine {
  std::string label;
  std::string value;
  int span;
};

static std::vector<InfoLine> g_info_lines;
static std::mutex g_info_mutex;


static void build_hardware_info() {
  std::vector<InfoLine> lines;
  std::ostringstream tmp;
  tmp << std::fixed;

  const auto cpus = hwinfo::getAllCPUs();
  if (!cpus.empty()) {
    const auto& cpu = cpus[0];
    InfoLine l;
    l.label = "\xe5\xa4\x84\xe7\x90\x86\xe5\x99\xa8";
    l.value = trim_str(cpu.modelName());
    l.span = 2;
    lines.push_back(l);

    tmp.str("");
    tmp << cpu.numPhysicalCores() << "\xe6\xa0\xb8 " << cpu.numLogicalCores() << "\xe7\xba\xbf\xe7\xa8\x8b";
    uint64_t l3_raw = 0;
    if (!cpu.cores().empty()) {
      l3_raw = cpu.cores()[0].cache.l3;
    }
    if (l3_raw > 0) {
      double l3_mb_from_bytes = (double)l3_raw / (1024.0 * 1024.0);
      double l3_mb_from_kb = (double)l3_raw / 1024.0;
      double l3_mb = 0;
      if (l3_mb_from_bytes >= 1.0) {
        l3_mb = l3_mb_from_bytes;
      } else if (l3_mb_from_kb >= 1.0) {
        l3_mb = l3_mb_from_kb;
      }
      if (l3_mb >= 1.0) {
        tmp << "  L3 " << (uint64_t)(l3_mb + 0.5) << " MB";
      } else if (l3_raw >= 1024) {
        tmp << "  L3 " << (uint64_t)(l3_raw / 1024) << " KB";
      }
    }
    InfoLine l2;
    l2.label = "";
    l2.value = tmp.str();
    l2.span = 1;
    lines.push_back(l2);
  }

  const auto gpus = hwinfo::getAllGPUs();
  if (!gpus.empty()) {
    for (size_t g = 0; g < gpus.size() && g < 2; ++g) {
      const auto& gpu = gpus[g];
      uint64_t vram_bytes = gpu.dedicated_memory_Bytes();
      std::string gpu_name = gpu.name();
      bool is_integrated = false;

      if (gpu_name.find("Intel") != std::string::npos) {
        is_integrated = true;
      } else if (gpu_name.find("Radeon") != std::string::npos &&
                 gpu_name.find("RX") == std::string::npos &&
                 gpu_name.find("R9") == std::string::npos &&
                 gpu_name.find("R7") == std::string::npos &&
                 gpu_name.find("R5") == std::string::npos &&
                 gpu_name.find("Pro") == std::string::npos &&
                 gpu_name.find("Vega") == std::string::npos) {
        is_integrated = true;
      }

      InfoLine l;
      if (is_integrated) {
        l.label = "\xe9\x9b\x86\xe6\x88\x90\xe6\x98\xbe\xe5\x8d\xa1";
      } else {
        l.label = "\xe7\x8b\xac\xe7\xab\x8b\xe6\x98\xbe\xe5\x8d\xa1";
      }
      tmp.str("");
      tmp << trim_str(gpu_name);
      if (vram_bytes >= 1000000000ULL) {
        double vram_gb = (double)vram_bytes / 1000000000.0;
        static const int standard_gb[] = {1,2,3,4,6,8,10,11,12,16,20,24};
        int display_gb = standard_gb[sizeof(standard_gb)/sizeof(standard_gb[0])-1];
        for (int s : standard_gb) {
          if (std::abs(vram_gb - s) < 1.5) { display_gb = s; break; }
        }
        tmp << " | " << display_gb << " GB";
      } else if (vram_bytes > 0) {
        double vram_mb = (double)vram_bytes / 1000000.0;
        static const int standard_mb[] = {128,256,512};
        int display_mb = standard_mb[sizeof(standard_mb)/sizeof(standard_mb[0])-1];
        for (int s : standard_mb) {
          if (std::abs(vram_mb - s) < 64) { display_mb = s; break; }
        }
        tmp << " | " << display_mb << " MB";
      }
      l.value = tmp.str();
      l.span = 1;
      lines.push_back(l);
    }
  }

  std::vector<MonitorInfo> wmi_monitors;
  get_monitor_info_from_wmi(wmi_monitors);

  int monitor_idx = 0;
  DISPLAY_DEVICEA dd = {0};
  dd.cb = sizeof(dd);
  for (DWORD i = 0; EnumDisplayDevicesA(NULL, i, &dd, 0); i++) {
    if ((dd.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) == 0) continue;

    DISPLAY_DEVICEA dd_monitor = {0};
    dd_monitor.cb = sizeof(dd_monitor);
    std::string model;
    std::string brand;
    if (EnumDisplayDevicesA(dd.DeviceName, 0, &dd_monitor, EDD_GET_DEVICE_INTERFACE_NAME)) {
      model = dd_monitor.DeviceString;
      brand = dd_monitor.DeviceName;
    }
    if (model.empty()) model = dd.DeviceName;

    DEVMODEA dm = {0};
    dm.dmSize = sizeof(dm);
    uint32_t w = 0, h = 0, r = 0;
    if (EnumDisplaySettingsA(dd.DeviceName, ENUM_CURRENT_SETTINGS, &dm)) {
      w = dm.dmPelsWidth;
      h = dm.dmPelsHeight;
      r = dm.dmDisplayFrequency;
    }

    double inch = 0;
    std::string mfg = brand;
    if (monitor_idx < (int)wmi_monitors.size()) {
      const auto& wm = wmi_monitors[monitor_idx];
      if (!wm.model.empty()) model = wm.model;
      if (!wm.manufacturer.empty()) mfg = wm.manufacturer;
      if (wm.physical_width_mm > 0) {
        double diag_mm = std::sqrt((double)wm.physical_width_mm * wm.physical_width_mm +
                                    (double)wm.physical_height_mm * wm.physical_height_mm);
        inch = diag_mm / 25.4;
      }
    }
    monitor_idx++;

    std::string mfg_cn = translate_brand(mfg);
    tmp.str("");
    tmp << mfg_cn << " " << trim_str(model) << " | ";
    if (inch > 0) tmp << std::setprecision(1) << inch << "\" | ";
    tmp << w << "x" << h << " " << r << "Hz";
    InfoLine l;
    l.label = "\xe6\x98\xbe\xe7\xa4\xba\xe5\x99\xa8";
    l.value = tmp.str();
    l.span = 1;
    lines.push_back(l);
  }

  hwinfo::Memory memory;
  uint64_t total_gb = static_cast<uint64_t>(memory.size() / (1024ULL * 1024 * 1024));

  {
    uint64_t soldered_total = 0;
    std::string soldered_type;
    std::string soldered_vendor;
    uint64_t soldered_freq = 0;
    int soldered_count = 0;
    std::vector<size_t> slot_indices;

    for (size_t i = 0; i < memory.modules().size(); ++i) {
      const auto& mod = memory.modules()[i];
      if (mod.is_soldered()) {
        soldered_total += mod._size_bytes;
        if (soldered_type.empty()) soldered_type = mod.memory_type;
        if (soldered_vendor.empty()) soldered_vendor = mod.vendor;
        if (soldered_freq == 0) soldered_freq = mod.frequency_hz;
        soldered_count++;
      } else {
        slot_indices.push_back(i);
      }
    }

    tmp.str("");
    tmp << total_gb << "GB (";
    if (soldered_total > 0 && !slot_indices.empty()) {
      tmp << (soldered_total / (1024ULL * 1024 * 1024)) << "+";
      for (size_t j = 0; j < slot_indices.size(); ++j) {
        uint64_t mod_gb = memory.modules()[slot_indices[j]]._size_bytes / (1024ULL * 1024 * 1024);
        tmp << mod_gb;
        if (j < slot_indices.size() - 1) tmp << "+";
      }
    } else {
      for (size_t i = 0; i < memory.modules().size(); ++i) {
        uint64_t mod_gb = memory.modules()[i]._size_bytes / (1024ULL * 1024 * 1024);
        tmp << mod_gb;
        if (i < memory.modules().size() - 1) tmp << "+";
      }
    }
    tmp << ")";
    {
      InfoLine l;
      l.label = "\xe6\x80\xbb\xe5\x86\x85\xe5\xad\x98";
      l.value = tmp.str();
      l.span = 1;
      lines.push_back(l);
    }

    if (soldered_total > 0) {
      uint64_t soldered_gb = soldered_total / (1024ULL * 1024 * 1024);
      std::string mem_type = soldered_type;
      if (mem_type.empty()) {
        mem_type = (soldered_freq >= 4800000000ULL) ? "DDR5" : "DDR4";
      }
      std::string vendor_cn = translate_brand(trim_str(soldered_vendor));
      tmp.str("");
      tmp << vendor_cn << " | " << mem_type << " " << soldered_gb << "GB "
          << std::setprecision(0) << hz_to_mhz(soldered_freq) << "MHz";
      InfoLine l;
      l.label = "\xe6\x9d\xbf\xe8\xbd\xbd\xe5\x86\x85\xe5\xad\x98";
      l.value = tmp.str();
      l.span = 1;
      lines.push_back(l);
    }

    for (size_t j = 0; j < slot_indices.size(); ++j) {
      const auto& mod = memory.modules()[slot_indices[j]];
      uint64_t mod_gb = mod._size_bytes / (1024ULL * 1024 * 1024);
      std::string mem_type = mod.memory_type;
      if (mem_type.empty()) {
        mem_type = (mod.frequency_hz >= 4800000000ULL) ? "DDR5" : "DDR4";
      }
      std::string vendor_cn = translate_brand(trim_str(mod.vendor));
      std::string model_str = trim_str(mod.model);
      tmp.str("");
      tmp << vendor_cn;
      if (!model_str.empty()) tmp << " " << model_str;
      tmp << " | " << mem_type << " " << mod_gb << "GB "
          << std::setprecision(0) << hz_to_mhz(mod.frequency_hz) << "MHz";
      InfoLine l;
      l.label = "\xe6\xa7\xbd\xe4\xbd\x8d" + std::to_string(j + 1);
      l.value = tmp.str();
      l.span = 1;
      lines.push_back(l);
    }
  }

  {
    std::lock_guard<std::mutex> lock(g_disk_mutex);
    if (g_disk_details.empty()) {
      InfoLine l;
      l.label = "\xe7\xa1\xac\xe7\x9b\x98";
      l.value = "\xe4\xbf\xa1\xe6\x81\xaf\xe8\x8e\xb7\xe5\x8f\x96\xe4\xb8\xad...";
      l.span = 2;
      lines.push_back(l);
    } else {
    for (size_t di = 0; di < g_disk_details.size(); ++di) {
      const auto& d = g_disk_details[di];
      std::string disk_model_cn = trim_str(d.model);
      {
        static const struct { const char* en; const char* cn; } dmap[] = {
          {"ZHITAI", "\xe8\x87\xb4\xe6\x80\x81"},
          {"Samsung", "\xe4\xb8\x89\xe6\x98\x9f"},
          {"WDC", "\xe8\xa5\xbf\xe9\x83\xa8\xe6\x95\xb0\xe6\x8d\xae"},
          {"Seagate", "\xe5\xb8\x8c\xe6\x8d\xb7"},
          {"Toshiba", "\xe4\xb8\x9c\xe8\x8a\x9d"},
          {"KIOXIA", "\xe9\x93\xa0\xe4\xbe\xa0"},
          {"Intel", "\xe8\x8b\xb1\xe7\x89\xb9\xe5\xb0\x94"},
          {"Kingston", "\xe9\x87\x91\xe5\xa3\xab\xe9\xa1\xbf"},
          {"Crucial", "\xe8\x8b\xb1\xe7\x9d\x9b\xe8\xbe\xbe"},
        };
        for (const auto& m : dmap) {
          size_t pos = disk_model_cn.find(m.en);
          if (pos != std::string::npos) {
            disk_model_cn.replace(pos, strlen(m.en), m.cn);
            break;
          }
        }
      }

      std::string size_display = d.disk_size;
      size_t paren_pos = size_display.find(" (");
      if (paren_pos != std::string::npos) {
        size_display = size_display.substr(0, paren_pos);
      }

      std::string iface = d.transfer_mode.empty() ? d.interface_type : d.transfer_mode;
      size_t pipe_pos = iface.find(" | ");
      if (pipe_pos != std::string::npos) {
        iface = iface.substr(0, pipe_pos);
      }

      tmp.str("");
      tmp << disk_model_cn << " | " << size_display;
      if (!iface.empty()) tmp << "(" << iface << ")";
      InfoLine l;
      if (g_disk_details.size() > 1) {
        l.label = "\xe7\xa1\xac\xe7\x9b\x98" + std::to_string(di + 1);
      } else {
        l.label = "\xe7\xa1\xac\xe7\x9b\x98";
      }
      l.value = tmp.str();
      l.span = 2;
      lines.push_back(l);

      tmp.str("");
      tmp << "\xe5\x81\xa5\xe5\xba\xb7\xe7\x8a\xb6\xe6\x80\x81 " << d.health_status;
      tmp << "  \xe9\x80\x9a\xe7\x94\xb5\xe6\xac\xa1\xe6\x95\xb0 " << d.power_on_count << " \xe6\xac\xa1";
      tmp << "  \xe9\x80\x9a\xe7\x94\xb5\xe6\x97\xb6\xe9\x97\xb4 " << d.power_on_hours << " \xe5\xb0\x8f\xe6\x97\xb6";
      InfoLine l2;
      l2.label = "";
      l2.value = tmp.str();
      l2.span = 1;
      lines.push_back(l2);
    }
    }
  }

  BatteryInfo bat_info;
  if (query_battery_from_wmi(bat_info)) {
    tmp.str("");
    if (!bat_info.manufacturer.empty() && bat_info.manufacturer != "-") {
      tmp << bat_info.manufacturer << " ";
    }
    if (!bat_info.device_name.empty() && bat_info.device_name != "-") {
      tmp << bat_info.device_name;
    }
    if (tmp.str().empty()) {
      tmp << "\xe7\x94\xb5\xe6\xb1\xa0";
    }
    if (bat_info.designed_capacity > 0) {
      double wh = (double)bat_info.designed_capacity / 1000.0;
      if (wh >= 1000.0) {
        tmp << "  " << std::setprecision(1) << (wh / 1000.0) << " kWh";
      } else {
        tmp << "  " << std::setprecision(1) << wh << " Wh";
      }
    }
    InfoLine l;
    l.label = "\xe7\x94\xb5\xe6\xb1\xa0";
    l.value = tmp.str();
    l.span = 2;
    lines.push_back(l);

    tmp.str("");
    double health = 0;
    if (bat_info.designed_capacity > 0 && bat_info.full_capacity > 0) {
      health = (double)bat_info.full_capacity / (double)bat_info.designed_capacity * 100.0;
      if (health > 100.0) health = 100.0;
    }
    if (health > 0) {
      tmp << "\xe5\x81\xa5\xe5\xba\xb7\xe5\xba\xa6 " << std::setprecision(0) << health << "%  ";
    }
    if (bat_info.cycle_count > 0) {
      tmp << "\xe5\xbe\xaa\xe7\x8e\xaf" << bat_info.cycle_count << "\xe6\xac\xa1  ";
    }
    if (bat_info.charge_rate > 0) {
      double power_w = (double)bat_info.charge_rate / 1000.0;
      if (power_w >= 1.0) {
        tmp << "\xe5\x85\x85\xe7\x94\xb5 " << std::setprecision(1) << power_w << "W";
      } else {
        tmp << "\xe5\x85\x85\xe7\x94\xb5 " << std::setprecision(0) << (power_w * 1000.0) << "mW";
      }
    } else if (bat_info.discharge_rate > 0) {
      double power_w = (double)bat_info.discharge_rate / 1000.0;
      if (power_w >= 1.0) {
        tmp << "\xe6\x94\xbe\xe7\x94\xb5 " << std::setprecision(1) << power_w << "W";
      } else {
        tmp << "\xe6\x94\xbe\xe7\x94\xb5 " << std::setprecision(0) << (power_w * 1000.0) << "mW";
      }
    } else if (bat_info.voltage > 0 && bat_info.power_online) {
      tmp << "\xe5\xb7\xb2\xe6\x8e\xa5\xe7\x94\xb5\xe6\xba\x90";
    }
    InfoLine l2;
    l2.label = "";
    l2.value = tmp.str();
    l2.span = 1;
    lines.push_back(l2);
  } else {
    InfoLine l;
    l.label = "\xe7\x94\xb5\xe6\xb1\xa0";
    l.value = "\xe6\x9c\xaa\xe6\xa3\x80\xe6\xb5\x8b\xe5\x88\xb0\xe7\x94\xb5\xe6\xb1\xa0\xe4\xbf\xa1\xe6\x81\xaf";
    l.span = 1;
    lines.push_back(l);
  }

  {
    std::lock_guard<std::mutex> lock(g_info_mutex);
    g_info_lines = std::move(lines);
  }
}

static std::string escape_json(const std::string& s) {
  std::string o;
  o.reserve(s.size() + 8);
  for (char c : s) {
    switch (c) {
      case '"': o += "\\\""; break;
      case '\\': o += "\\\\"; break;
      case '\n': o += "\\n"; break;
      case '\r': o += "\\r"; break;
      case '\t': o += "\\t"; break;
      default: o += c;
    }
  }
  return o;
}

static std::string get_keyboard_layout() {
  HKL hkl = GetKeyboardLayout(0);
  LANGID langId = LOWORD(hkl);
  WORD primaryId = PRIMARYLANGID(langId);

  // Map primary language ID to layout name
  switch (primaryId) {
    case LANG_FRENCH:  return "AZERTY";
    case LANG_GERMAN:  return "QWERTZ";
    case LANG_JAPANESE: return "JIS";
    case LANG_ENGLISH:
    default:           return "US";
  }
}

static std::string build_json_data() {
  std::string json = "{\"model\":\"" + escape_json(g_currentModel) + "\"";
  json += ",\"keyboard_layout\":\"" + get_keyboard_layout() + "\"";
  json += ",\"lines\":[";
  {
    std::lock_guard<std::mutex> lock(g_info_mutex);
    for (size_t i = 0; i < g_info_lines.size(); ++i) {
      const auto& line = g_info_lines[i];
      if (i > 0) json += ",";
      json += "{\"label\":\"" + escape_json(line.label) + "\",\"value\":\"" + escape_json(line.value) + "\",\"span\":" + std::to_string(line.span) + "}";
    }
  }
  json += "]}";
  return json;
}

static void update_hardware_info(HWND hWnd) {
  if (g_refreshing.load()) {
    HW_LOG_INFO("update_hardware_info: skipped (already refreshing)");
    return;
  }
  g_refreshing.store(true);
  HW_LOG_INFO("update_hardware_info: starting worker thread");

  launch_worker([hWnd]() {
    if (g_shutting_down.load()) return;
    HW_LOG_INFO("update_hardware_info worker: building model");
    g_currentModel = get_system_model();
    if (g_shutting_down.load()) return;
    HW_LOG_INFO("update_hardware_info worker: building hardware info");
    build_hardware_info();
    g_refreshing.store(false);
    HW_LOG_INFO("update_hardware_info worker: done, posting WM_HW_INFO_READY");
    if (!g_shutting_down.load() && hWnd)
      PostMessageA(hWnd, WM_HW_INFO_READY, 0, 0);
  });
}

static HBRUSH CreateSolidBrushRGB(COLORREF color) {
  return CreateSolidBrush(color);
}

// is_admin() from common.h

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
    case WM_CREATE: {
      g_hWnd = hWnd;
      break;
    }

    case WM_NCCALCSIZE: {
      // Remove the entire non-client area so no native title bar is drawn
      if (wParam == TRUE) return 0;
      return DefWindowProcA(hWnd, message, wParam, lParam);
    }

    case WM_NCHITTEST: {
      POINTS pts = MAKEPOINTS(lParam);
      RECT rc;
      GetWindowRect(hWnd, &rc);
      int border = 8;
      // Corners first
      if (pts.x < rc.left + border && pts.y < rc.top + border)     return HTTOPLEFT;
      if (pts.x > rc.right - border && pts.y < rc.top + border)    return HTTOPRIGHT;
      if (pts.x < rc.left + border && pts.y > rc.bottom - border)  return HTBOTTOMLEFT;
      if (pts.x > rc.right - border && pts.y > rc.bottom - border) return HTBOTTOMRIGHT;
      // Edges
      if (pts.x < rc.left + border)   return HTLEFT;
      if (pts.x > rc.right - border)  return HTRIGHT;
      if (pts.y < rc.top + border)    return HTTOP;
      if (pts.y > rc.bottom - border) return HTBOTTOM;
      return HTCLIENT;
    }

    case WM_SIZE: {
      if (g_webViewController) {
        RECT rc;
        GetClientRect(hWnd, &rc);
        g_webViewController->put_Bounds(rc);
      }
      break;
    }

    case WM_REFRESH_DISK: {
      update_hardware_info(hWnd);
      break;
    }

    case WM_HW_INFO_READY: {
      if (g_webView && g_webview_ready.load()) {
        std::string json = build_json_data();
        std::wstring wjson = utf8_to_wide(json);
        HRESULT hr = g_webView->PostWebMessageAsJson(wjson.c_str());
        HW_LOG_INFO("WM_HW_INFO_READY: PostWebMessageAsJson result=" + std::to_string(hr) + " len=" + std::to_string(json.size()));
      } else {
        // Queue the data for when WebView becomes ready
        std::lock_guard<std::mutex> lock(g_pending_mutex);
        g_pending_json = build_json_data();
        HW_LOG_INFO("WM_HW_INFO_READY: queued (webview not ready)");
      }
      break;
    }

    case WM_CLOSE: {
      DestroyWindow(hWnd);
      break;
    }

    case WM_SYSKEYDOWN: {
      if (wParam == VK_F4) {
        DestroyWindow(hWnd);
        return 0;
      }
      break;
    }

    case WM_DESTROY: {
      g_webview_ready.store(false);
      join_worker_threads();
      if (g_webView) { g_webView->Release(); g_webView = nullptr; }
      if (g_webViewController) { g_webViewController->Release(); g_webViewController = nullptr; }
      PostQuitMessage(0);
      break;
    }

    default:
      return DefWindowProcA(hWnd, message, wParam, lParam);
  }
  return 0;
}

static void InitWebView2(HWND hWnd) {
  std::wstring htmlPath = L"file:///";
  wchar_t exePath[MAX_PATH];
  GetModuleFileNameW(nullptr, exePath, MAX_PATH);
  std::wstring exeDir = exePath;
  auto lastSlash = exeDir.find_last_of(L"\\/");
  if (lastSlash != std::wstring::npos) exeDir = exeDir.substr(0, lastSlash);
  htmlPath += exeDir + L"/web/index.html";

  std::wstring fixedRuntimePath = exeDir + L"\\WebView2Runtime";
  WCHAR fixedExeCheck[MAX_PATH];
  wsprintfW(fixedExeCheck, L"%s\\msedgewebview2.exe", fixedRuntimePath.c_str());
  DWORD fileAttrib = GetFileAttributesW(fixedExeCheck);
  LPCWSTR browserPath = nullptr;
  if (fileAttrib != INVALID_FILE_ATTRIBUTES && !(fileAttrib & FILE_ATTRIBUTE_DIRECTORY)) {
    browserPath = fixedRuntimePath.c_str();
  }

  CreateCoreWebView2EnvironmentWithOptions(
    browserPath, exeDir.c_str(), nullptr,
    Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
      [hWnd, htmlPath](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
        if (FAILED(result) || !env) return S_OK;

        env->CreateCoreWebView2Controller(hWnd,
          Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
            [hWnd, htmlPath](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
              if (FAILED(result) || !controller) return S_OK;

              controller->AddRef();
              g_webViewController = controller;

              ICoreWebView2* webview = nullptr;
              controller->get_CoreWebView2(&webview);
              if (!webview) return S_OK;

              webview->AddRef();
              g_webView = webview;

              ICoreWebView2Settings* settings = nullptr;
              webview->get_Settings(&settings);
              if (settings) {
                settings->put_IsStatusBarEnabled(FALSE);
                settings->put_AreDefaultContextMenusEnabled(FALSE);
                settings->Release();
              }

              RECT rc;
              GetClientRect(hWnd, &rc);
              controller->put_Bounds(rc);

              // Handle fullscreen requests from web content (screen test)
              EventRegistrationToken fsToken;
              webview->add_ContainsFullScreenElementChanged(
                Callback<ICoreWebView2ContainsFullScreenElementChangedEventHandler>(
                  [hWnd, controller](ICoreWebView2* sender, IUnknown* args) -> HRESULT {
                    BOOL hasFs = FALSE;
                    sender->get_ContainsFullScreenElement(&hasFs);
                    if (hasFs) {
                      // Enter fullscreen: expand WebView to fill entire window
                      RECT rc;
                      GetClientRect(hWnd, &rc);
                      controller->put_Bounds(rc);
                      // Remove window title bar for true fullscreen feel
                      LONG style = GetWindowLongW(hWnd, GWL_STYLE);
                      SetWindowLongW(hWnd, GWL_STYLE, style & ~WS_CAPTION);
                      // Maximize window
                      ShowWindow(hWnd, SW_MAXIMIZE);
                      // Update bounds again after maximize
                      GetClientRect(hWnd, &rc);
                      controller->put_Bounds(rc);
                    } else {
                      // Exit fullscreen: restore window chrome
                      LONG style = GetWindowLongW(hWnd, GWL_STYLE);
                      SetWindowLongW(hWnd, GWL_STYLE, style | WS_CAPTION);
                      ShowWindow(hWnd, SW_RESTORE);
                      RECT rc;
                      GetClientRect(hWnd, &rc);
                      controller->put_Bounds(rc);
                    }
                    return S_OK;
                  }
                ).Get(), &fsToken);

              EventRegistrationToken msgToken;
              webview->add_WebMessageReceived(
                Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                  [hWnd](ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                    LPWSTR message = nullptr;
                    args->TryGetWebMessageAsString(&message);
                    if (message) {
                      std::wstring msg(message);
                      if (msg == L"minimize") {
                        ShowWindow(hWnd, SW_MINIMIZE);
                      } else if (msg == L"maximize") {
                        ShowWindow(hWnd, SW_MAXIMIZE);
                      } else if (msg == L"restore") {
                        ShowWindow(hWnd, SW_RESTORE);
                      } else if (msg == L"close") {
                        DestroyWindow(hWnd);
                      } else if (msg == L"refresh") {
                        HW_LOG_INFO("WebMessage: refresh received");
                        update_hardware_info(hWnd);
                      } else if (msg == L"drag") {
                        ReleaseCapture();
                        SendMessageA(hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
                      } else if (msg == L"poll_monitor") {
                        // Build live monitoring JSON
                        // CPU utilization (blocks ~200ms to measure delta)
                        double cpu_util = hwinfo::monitoring::cpu::utilization();
                        // CPU frequency
                        auto freqs = hwinfo::monitoring::cpu::thread_frequency_hz();
                        double cpu_freq_mhz = 0.0;
                        if (!freqs.empty()) {
                          double sum = 0;
                          for (auto f : freqs) sum += f;
                          cpu_freq_mhz = sum / freqs.size() / 1e6;
                        }
                        // RAM
                        hwinfo::Memory mem;
                        double ram_used_pct = 0.0;
                        if (mem.size() > 0) {
                          uint64_t used = mem.size() - mem.available();
                          ram_used_pct = (double)used / mem.size() * 100.0;
                        }
                        // GPU utilization & VRAM via PDH
                        double gpu_util = 0.0;
                        double gpu_vram_pct = 0.0;
                        uint64_t gpu_vram_used_mb = 0;
                        uint64_t gpu_vram_total_mb = 0;
                        bool gpu_available = false;
                        {
                          static PDH_HQUERY hQuery = nullptr;
                          static PDH_HCOUNTER hGpuUtil = nullptr;
                          static PDH_HCOUNTER hGpuVram = nullptr;
                          static bool pdhInit = false;
                          static bool pdhCountersValid = true; // false if PDH counters don't exist
                          if (!pdhInit) {
                            PdhOpenQueryW(nullptr, 0, &hQuery);
                            PDH_STATUS s1 = PdhAddEnglishCounterW(hQuery, L"\\GPU Engine(*eng_3d)\\Utilization Percentage", 0, &hGpuUtil);
                            PDH_STATUS s2 = PdhAddEnglishCounterW(hQuery, L"\\GPU Process Memory(*)\\Dedicated Usage", 0, &hGpuVram);
                            if (s1 != ERROR_SUCCESS && s2 != ERROR_SUCCESS) {
                              pdhCountersValid = false; // No GPU PDH counters available
                            }
                            PdhCollectQueryData(hQuery);
                            pdhInit = true;
                          }
                          if (pdhCountersValid) {
                            PdhCollectQueryData(hQuery);
                            // GPU utilization: take max across all 3D engines
                            DWORD bufSize = 0;
                            DWORD itemCount = 0;
                            PDH_FMT_COUNTERVALUE_ITEM_W* itemsW = nullptr;
                            // Get max GPU utilization from 3D engines
                            PDH_STATUS pdhStatus = PdhGetFormattedCounterArrayW(hGpuUtil, PDH_FMT_DOUBLE, &bufSize, &itemCount, nullptr);
                            if ((pdhStatus == PDH_MORE_DATA || pdhStatus == ERROR_SUCCESS) && bufSize > 0) {
                              itemsW = (PDH_FMT_COUNTERVALUE_ITEM_W*)malloc(bufSize);
                              if (itemsW && PdhGetFormattedCounterArrayW(hGpuUtil, PDH_FMT_DOUBLE, &bufSize, &itemCount, itemsW) == ERROR_SUCCESS) {
                                double maxUtil = 0.0;
                                for (DWORD i = 0; i < itemCount; i++) {
                                  double v = itemsW[i].FmtValue.doubleValue;
                                  if (v > maxUtil) maxUtil = v;
                                }
                                gpu_util = maxUtil;
                                gpu_available = true;
                              }
                              free(itemsW);
                            }
                            // GPU VRAM: sum dedicated usage across processes
                            bufSize = 0; itemCount = 0; itemsW = nullptr;
                            pdhStatus = PdhGetFormattedCounterArrayW(hGpuVram, PDH_FMT_DOUBLE, &bufSize, &itemCount, nullptr);
                            if ((pdhStatus == PDH_MORE_DATA || pdhStatus == ERROR_SUCCESS) && bufSize > 0) {
                              itemsW = (PDH_FMT_COUNTERVALUE_ITEM_W*)malloc(bufSize);
                              if (itemsW && PdhGetFormattedCounterArrayW(hGpuVram, PDH_FMT_DOUBLE, &bufSize, &itemCount, itemsW) == ERROR_SUCCESS) {
                                double totalBytes = 0;
                                for (DWORD i = 0; i < itemCount; i++) {
                                  if (itemsW[i].FmtValue.doubleValue > 0)
                                    totalBytes += itemsW[i].FmtValue.doubleValue;
                                }
                                gpu_vram_used_mb = static_cast<uint64_t>(totalBytes / (1024.0 * 1024.0));
                              }
                              free(itemsW);
                            }
                          }
                          // Total VRAM from DXGI (already queried at startup)
                          auto gpus = hwinfo::getAllGPUs();
                          if (!gpus.empty()) {
                            gpu_vram_total_mb = gpus[0].dedicated_memory_Bytes() / (1024 * 1024);
                            if (gpu_vram_total_mb > 0)
                              gpu_vram_pct = (double)gpu_vram_used_mb / gpu_vram_total_mb * 100.0;
                            if (gpu_vram_total_mb > 0) gpu_available = true;
                          }
                        }
                        std::ostringstream mon;
                        mon << std::fixed << std::setprecision(1);
                        mon << "{\"cpu_utilization\":" << cpu_util
                            << ",\"cpu_freq_mhz\":" << cpu_freq_mhz
                            << ",\"ram_used_pct\":" << ram_used_pct
                            << ",\"ram_free_gb\":" << (mem.available() / (1024.0*1024.0*1024.0))
                            << ",\"gpu_available\":" << (gpu_available ? "true" : "false")
                            << ",\"gpu_utilization\":" << gpu_util
                            << ",\"gpu_vram_pct\":" << gpu_vram_pct
                            << ",\"gpu_vram_used_mb\":" << gpu_vram_used_mb
                            << ",\"gpu_vram_total_mb\":" << gpu_vram_total_mb
                            << ",\"disk_max_pct\":0.0"
                            << ",\"disk_io\":\"--\"}";
                        std::string monJson = mon.str();
                        sender->PostWebMessageAsJson(utf8_to_wide(monJson).c_str());
                      } else if (msg == L"scan_wifi") {
                        auto adapters = scan_wifi_adapters();
                        std::ostringstream wj;
                        wj << "{\"wifi_adapters\":[";
                        for (size_t i = 0; i < adapters.size(); ++i) {
                          if (i > 0) wj << ",";
                          wj << "{\"name\":\"" << escape_json(adapters[i].name) << "\""
                             << ",\"description\":\"" << escape_json(adapters[i].description) << "\""
                             << ",\"connection_id\":\"" << escape_json(adapters[i].connection_id) << "\"}";
                        }
                        wj << "]}";
                        sender->PostWebMessageAsJson(utf8_to_wide(wj.str()).c_str());
                      } else if (msg == L"scan_bluetooth") {
                        auto devices = scan_bluetooth_devices();
                        std::ostringstream bj;
                        bj << "{\"bluetooth_devices\":[";
                        for (size_t i = 0; i < devices.size(); ++i) {
                          if (i > 0) bj << ",";
                          bj << "{\"name\":\"" << escape_json(devices[i].name) << "\""
                             << ",\"description\":\"" << escape_json(devices[i].description) << "\"}";
                        }
                        bj << "]}";
                        sender->PostWebMessageAsJson(utf8_to_wide(bj.str()).c_str());
                      } else if (msg == L"play_speaker_left") {
                        play_speaker_chime(true);
                      } else if (msg == L"play_speaker_right") {
                        play_speaker_chime(false);
                      }
                      CoTaskMemFree(message);
                    }
                    return S_OK;
                  }
                ).Get(), &msgToken);

              EventRegistrationToken navToken;
              webview->add_NavigationCompleted(
                Callback<ICoreWebView2NavigationCompletedEventHandler>(
                  [hWnd](ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
                    HW_LOG_INFO("NavigationCompleted: webview ready");
                    g_webview_ready.store(true);
                    // Flush any pending data
                    {
                      std::lock_guard<std::mutex> lock(g_pending_mutex);
                      if (!g_pending_json.empty() && g_webView) {
                        HW_LOG_INFO("NavigationCompleted: flushing pending data, len=" + std::to_string(g_pending_json.size()));
                        std::wstring wjson = utf8_to_wide(g_pending_json);
                        g_webView->PostWebMessageAsJson(wjson.c_str());
                        g_pending_json.clear();
                      }
                    }
                    update_hardware_info(hWnd);
                    return S_OK;
                  }
                ).Get(), &navToken);

              // Append timestamp to bypass WebView2 disk cache
              std::wstring navUrl = htmlPath + L"?t=" + std::to_wstring(GetTickCount64());
              webview->Navigate(navUrl.c_str());

              return S_OK;
            }
          ).Get());

        return S_OK;
      }
    ).Get());
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
  (void)hPrevInstance;
  (void)lpCmdLine;

  Config::instance().load("config.json");
  Logger::instance().set_log_path(Config::instance().get_string("log_path", "logs/hwinfo_detect.log"));
  Logger::instance().set_min_level(LogLevel::Info);
  HW_LOG_INFO("Application starting");

  if (!is_admin()) {
    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    SHELLEXECUTEINFOA sei = {0};
    sei.cbSize = sizeof(SHELLEXECUTEINFOA);
    sei.lpVerb = "runas";
    sei.lpFile = exePath;
    sei.nShow = SW_SHOWNORMAL;
    if (ShellExecuteExA(&sei)) {
      return 0;
    }
  }

  CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

  WNDCLASSEXW wc = {0};
  wc.cbSize = sizeof(WNDCLASSEXW);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = WndProc;
  wc.hInstance = hInstance;
  wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(IDC_ARROW));
  wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
  wc.lpszClassName = L"HwInfoDetectGUI";
  wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(1));

  if (!RegisterClassExW(&wc)) {
    MessageBoxW(nullptr, L"Window Registration Failed", L"Error", MB_ICONEXCLAMATION | MB_OK);
    return 0;
  }

  int screenW = GetSystemMetrics(SM_CXSCREEN);
  int screenH = GetSystemMetrics(SM_CYSCREEN);
  int posX = (screenW - WINDOW_WIDTH) / 2;
  int posY = (screenH - WINDOW_HEIGHT) / 2;

  // WS_POPUP removes native title bar; WS_THICKFRAME keeps resize border
  DWORD winStyle = WS_POPUP | WS_THICKFRAME |
                   WS_MINIMIZEBOX | WS_MAXIMIZEBOX |
                   WS_VISIBLE | WS_CLIPCHILDREN;

  HWND hWnd = CreateWindowExW(
    0,
    L"HwInfoDetectGUI",
    L"Hardware Detect",
    winStyle,
    posX, posY, WINDOW_WIDTH, WINDOW_HEIGHT,
    nullptr, nullptr, hInstance, nullptr
  );

  if (!hWnd) {
    MessageBoxA(nullptr, "Window Creation Failed", "Error", MB_ICONEXCLAMATION | MB_OK);
    return 0;
  }

  // Enable dark-themed window border
  BOOL darkMode = TRUE;
  DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));

  InitWebView2(hWnd);

  {
    launch_worker([hWnd]() {
      if (g_shutting_down.load()) return;
      std::string diskinfo_exe = find_crystal_disk_info();
      bool crystal_disk_success = false;

      if (!diskinfo_exe.empty() && is_admin()) {
        std::string working_dir = diskinfo_exe.substr(0, diskinfo_exe.find_last_of("\\/"));
        std::string output_file = working_dir + "\\DiskInfo.txt";

        SECURITY_ATTRIBUTES sa = {0};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;

        HANDLE hNull = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_WRITE, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

        STARTUPINFOA si = {0};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        si.hStdOutput = hNull;
        si.hStdError = hNull;

        PROCESS_INFORMATION pi = {0};
        std::string command = "\"" + diskinfo_exe + "\" /CopyExit";

        BOOL success = CreateProcessA(nullptr, (LPSTR)command.c_str(), nullptr, nullptr, TRUE,
                                       0, nullptr, working_dir.c_str(), &si, &pi);

        CloseHandle(hNull);

        if (success) {
          DWORD wait_result = WaitForSingleObject(pi.hProcess, 30000);
          if (wait_result != WAIT_OBJECT_0) {
            TerminateProcess(pi.hProcess, 1);
          }
          CloseHandle(pi.hProcess);
          CloseHandle(pi.hThread);

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
              crystal_disk_success = true;
            }
          }
        }

        if (!crystal_disk_success) {
          std::ifstream file(output_file);
          if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            file.close();

            std::string content = ensure_utf8(buffer.str());

            std::vector<DiskDetailInfo> disks;
            parse_diskinfo_output(content, disks);

            if (!disks.empty()) {
              std::lock_guard<std::mutex> lock(g_disk_mutex);
              g_disk_details = std::move(disks);
              crystal_disk_success = true;
            }
          }
        }
      }

      if (!crystal_disk_success) {
        get_disk_details_wmi();
      }

      if (g_hWnd) {
        PostMessageA(g_hWnd, WM_REFRESH_DISK, 0, 0);
      }
    });
  }

  ShowWindow(hWnd, nCmdShow);
  UpdateWindow(hWnd);

  MSG msg;
  while (GetMessageA(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessageA(&msg);
  }

  CoUninitialize();
  return (int)msg.wParam;
}
