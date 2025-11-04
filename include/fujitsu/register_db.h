#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace fujitsu::airstage {

struct RegisterInfo {
  const char* name = nullptr;
  const char* description = nullptr;
};

// Returns metadata for a known register address, if available.
[[nodiscard]] std::optional<RegisterInfo> LookupRegister(uint16_t address);

}  // namespace fujitsu::airstage

