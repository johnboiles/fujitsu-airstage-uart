#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace fujitsu::airstage {

inline constexpr std::size_t kPacketHeaderBytes = 5;  // 4-byte command + 1-byte length
inline constexpr std::size_t kPacketTrailerBytes = 2; // 16-bit checksum

struct Packet {
  uint32_t command_id = 0;
  std::vector<uint8_t> payload;
  uint16_t checksum = 0;

  [[nodiscard]] std::size_t payload_length() const { return payload.size(); }
  [[nodiscard]] std::size_t frame_length() const { return kPacketHeaderBytes + payload.size() + kPacketTrailerBytes; }

  [[nodiscard]] std::vector<uint8_t> Serialize() const;
};

[[nodiscard]] uint16_t ComputeChecksum(std::span<const uint8_t> bytes);

// Returns a parsed packet if the frame is well-formed and checksum matches.
// The frame must contain the full header (command id + payload length), payload, and checksum.
[[nodiscard]] std::optional<Packet> ParsePacket(std::span<const uint8_t> frame, std::string* error = nullptr);

// Validates a raw frame without building a Packet structure. Returns true if the frame
// has coherent sizing and checksum. On failure, `error` (if provided) receives a message.
[[nodiscard]] bool ValidateFrame(std::span<const uint8_t> frame, std::string* error = nullptr);

}  // namespace fujitsu::airstage

