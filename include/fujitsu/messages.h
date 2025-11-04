#pragma once

#include "fujitsu/packet.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace fujitsu::airstage {

enum class CommandId : uint32_t {
  kHandshake0 = 0x00000000,
  kHandshake1 = 0x00000001,
  kSetpoint = 0x00000002,
  kReadRegisters = 0x00000003,
  kControlRegister = 0x00000004,
  kBulkWrite = 0x00000005,
};

struct RegisterValue {
  uint16_t address = 0;
  uint16_t value = 0;
};

struct ReadRequest {
  std::vector<uint16_t> addresses;
};

struct ReadResponse {
  uint8_t status = 0;
  std::vector<RegisterValue> values;
};

struct WriteRequest {
  std::vector<RegisterValue> values;
};

struct WriteResponse {
  uint8_t status = 0;
};

// Attempt to interpret the provided packet as a read request originating from the indoor unit.
[[nodiscard]] std::optional<ReadRequest> DecodeReadRequest(const Packet& packet);

// Attempt to interpret the provided packet as a read response originating from the WiFi module.
[[nodiscard]] std::optional<ReadResponse> DecodeReadResponse(const Packet& packet);

// Attempt to interpret the packet as a write request (single or multi-register) from the indoor unit.
[[nodiscard]] std::optional<WriteRequest> DecodeWriteRequest(const Packet& packet);

// Attempt to interpret the packet as a write acknowledgement from the WiFi module.
[[nodiscard]] std::optional<WriteResponse> DecodeWriteResponse(const Packet& packet);

// Helper to stringify known command identifiers; unknown ids return an empty string.
[[nodiscard]] std::string CommandToString(uint32_t command_id);

}  // namespace fujitsu::airstage

