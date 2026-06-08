// Copyright (c) Leon Freist <freist@informatik.uni-freiburg.de>
// This software is part of HWBenchmark

#include <hwinfo/platform.h>

#ifdef HWINFO_WINDOWS
#include <Windows.h>
#include <hwinfo/ram.h>
#include <hwinfo/utils/stringutils.h>
#include <hwinfo/utils/wmi_wrapper.h>

#include <string>
#include <vector>

namespace hwinfo {

// _____________________________________________________________________________________________________________________
Memory::Memory() {
  utils::WMI::_WMI wmi;
  const std::wstring query_string(
      L"SELECT Capacity, ConfiguredClockSpeed, Manufacturer, SerialNumber, PartNumber, SMBIOSMemoryType, FormFactor FROM Win32_PhysicalMemory");
  bool success = wmi.execute_query(query_string);
  if (!success) {
    return;
  }
  ULONG u_return = 0;
  IWbemClassObject* obj = nullptr;
  std::vector<Memory> rams;
  int id = 0;
  while (wmi.enumerator) {
    wmi.enumerator->Next(WBEM_INFINITE, 1, &obj, &u_return);
    if (!u_return) {
      break;
    }
    VARIANT vt_prop;
    HRESULT hr;
    Memory::Module module;
    module.id = id++;
    hr = obj->Get(L"Manufacturer", 0, &vt_prop, nullptr, nullptr);
    if (SUCCEEDED(hr) && (V_VT(&vt_prop) == VT_BSTR)) {
      module.vendor = utils::wstring_to_std_string(vt_prop.bstrVal);
    }
    hr = obj->Get(L"partNumber", 0, &vt_prop, nullptr, nullptr);
    if (SUCCEEDED(hr) && (V_VT(&vt_prop) == VT_BSTR)) {
      module.model = utils::wstring_to_std_string(vt_prop.bstrVal);
      // TODO: One expects an actual name of the RAM but wmi does not provide such a property...
      //       The "Name"-property of WMI returns "PhysicalMemory".
      module.name = std::string(module.vendor + " " + module.model);
    }
    hr = obj->Get(L"Capacity", 0, &vt_prop, nullptr, nullptr);
    if (SUCCEEDED(hr) && (V_VT(&vt_prop) == VT_BSTR)) {
      module._size_bytes = std::stoll(utils::wstring_to_std_string(vt_prop.bstrVal));
    }
    hr = obj->Get(L"ConfiguredClockSpeed", 0, &vt_prop, nullptr, nullptr);
    if (SUCCEEDED(hr) && (V_VT(&vt_prop) == VT_I4)) {
      module.frequency_hz = static_cast<int64_t>(vt_prop.intVal) * 1000 * 1000;
    }
    hr = obj->Get(L"SerialNumber", 0, &vt_prop, nullptr, nullptr);
    if (SUCCEEDED(hr) && (V_VT(&vt_prop) == VT_BSTR)) {
      module.serial_number = utils::wstring_to_std_string(vt_prop.bstrVal);
    }
    hr = obj->Get(L"SMBIOSMemoryType", 0, &vt_prop, nullptr, nullptr);
    if (SUCCEEDED(hr) && (V_VT(&vt_prop) == VT_I4)) {
      switch (vt_prop.intVal) {
        case 0x12: module.memory_type = "DDR"; break;
        case 0x13: module.memory_type = "DDR2"; break;
        case 0x14: module.memory_type = "DDR2 FB-DIMM"; break;
        case 0x18: module.memory_type = "DDR3"; break;
        case 0x1A: module.memory_type = "DDR4"; break;
        case 0x1B: module.memory_type = "LPDDR"; break;
        case 0x1C: module.memory_type = "LPDDR2"; break;
        case 0x1D: module.memory_type = "LPDDR3"; break;
        case 0x1E: module.memory_type = "LPDDR4"; break;
        case 0x21: module.memory_type = "DDR5"; break;
        case 0x22: module.memory_type = "LPDDR5"; break;
        default: break;
      }
    }
    hr = obj->Get(L"FormFactor", 0, &vt_prop, nullptr, nullptr);
    if (SUCCEEDED(hr) && (V_VT(&vt_prop) == VT_I4)) {
      int ff = vt_prop.intVal;
      module.form_factor = ff;
      if (ff == 8 || ff == 12) {
        if (module.memory_type == "LPDDR5") module.memory_type = "DDR5";
        else if (module.memory_type == "LPDDR4") module.memory_type = "DDR4";
        else if (module.memory_type == "LPDDR3") module.memory_type = "DDR3";
        else if (module.memory_type == "LPDDR2") module.memory_type = "DDR2";
        else if (module.memory_type == "LPDDR") module.memory_type = "DDR";
      }
    }
    VariantClear(&vt_prop);
    obj->Release();
    _modules.push_back(std::move(module));
  }
}

// _____________________________________________________________________________________________________________________
uint64_t Memory::size() const {
  uint64_t sum = 0;
  for (const auto& module : _modules) {
    sum += module._size_bytes;
  }
  return sum;
}

// _____________________________________________________________________________________________________________________
uint64_t Memory::free() const {
  auto res = utils::WMI::query<std::string>(L"Win32_OperatingSystem", L"FreePhysicalMemory");
  if (res.empty()) {
    return 0;
  }
  return std::stoll(res.front()) * 1024;
}

// _____________________________________________________________________________________________________________________
uint64_t Memory::available() const {
  // TODO: Get actual available memory size...
  return free();
}

}  // namespace hwinfo

#endif  // HWINFO_WINDOWS