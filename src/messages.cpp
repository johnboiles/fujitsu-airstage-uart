#include "fujitsu/messages.h"

#include <sstream>

namespace fujitsu::airstage {

namespace {

bool IsWriteCommand(uint32_t command_id) {
  switch (command_id) {
    case static_cast<uint32_t>(CommandId::kSetpoint):
    case static_cast<uint32_t>(CommandId::kControlRegister):
    case static_cast<uint32_t>(CommandId::kBulkWrite):
      return true;
    default:
      return false;
  }
}

}  // namespace

std::optional<ReadRequest> DecodeReadRequest(const Packet& packet) {
  if (packet.command_id != static_cast<uint32_t>(CommandId::kReadRegisters)) {
    return std::nullopt;
  }
  if (packet.payload.empty() || (packet.payload.size() % 2) != 0) {
    return std::nullopt;
  }

  ReadRequest request;
  request.addresses.reserve(packet.payload.size() / 2);
  for (std::size_t i = 0; i < packet.payload.size(); i += 2) {
    uint16_t address = static_cast<uint16_t>(packet.payload[i] << 8 | packet.payload[i + 1]);
    request.addresses.push_back(address);
  }
  return request;
}

std::optional<ReadResponse> DecodeReadResponse(const Packet& packet) {
  if (packet.command_id != static_cast<uint32_t>(CommandId::kReadRegisters)) {
    return std::nullopt;
  }
  if (packet.payload.empty()) {
    return std::nullopt;
  }
  if ((packet.payload.size() - 1) % 4 != 0) {
    return std::nullopt;
  }

  ReadResponse response;
  response.status = packet.payload[0];
  response.values.reserve((packet.payload.size() - 1) / 4);
  for (std::size_t i = 1; i < packet.payload.size(); i += 4) {
    uint16_t address = static_cast<uint16_t>(packet.payload[i] << 8 | packet.payload[i + 1]);
    uint16_t value = static_cast<uint16_t>(packet.payload[i + 2] << 8 | packet.payload[i + 3]);
    response.values.push_back(RegisterValue{address, value});
  }
  return response;
}

std::optional<WriteRequest> DecodeWriteRequest(const Packet& packet) {
  if (!IsWriteCommand(packet.command_id)) {
    return std::nullopt;
  }

  if (packet.payload.empty() || (packet.payload.size() % 4) != 0) {
    return std::nullopt;
  }

  WriteRequest request;
  request.values.reserve(packet.payload.size() / 4);
  for (std::size_t i = 0; i < packet.payload.size(); i += 4) {
    uint16_t address = static_cast<uint16_t>(packet.payload[i] << 8 | packet.payload[i + 1]);
    uint16_t value = static_cast<uint16_t>(packet.payload[i + 2] << 8 | packet.payload[i + 3]);
    request.values.push_back(RegisterValue{address, value});
  }
  return request;
}

std::optional<WriteResponse> DecodeWriteResponse(const Packet& packet) {
  if (!IsWriteCommand(packet.command_id)) {
    return std::nullopt;
  }
  if (packet.payload.size() != 1) {
    return std::nullopt;
  }

  WriteResponse response;
  response.status = packet.payload[0];
  return response;
}

std::string CommandToString(uint32_t command_id) {
  switch (command_id) {
    case static_cast<uint32_t>(CommandId::kHandshake0):
      return "Handshake0";
    case static_cast<uint32_t>(CommandId::kHandshake1):
      return "Handshake1";
    case static_cast<uint32_t>(CommandId::kSetpoint):
      return "WriteRegister";
    case static_cast<uint32_t>(CommandId::kReadRegisters):
      return "ReadRegisters";
    case static_cast<uint32_t>(CommandId::kControlRegister):
      return "WriteControlRegister";
    case static_cast<uint32_t>(CommandId::kBulkWrite):
      return "BulkWrite";
    default: {
      std::ostringstream oss;
      oss << "Unknown(0x" << std::hex << std::uppercase << command_id << ")";
      return oss.str();
    }
  }
}

}  // namespace fujitsu::airstage

