#include "fujitsu/packet.h"

#include <algorithm>
#include <stdexcept>

namespace fujitsu::airstage {

namespace {

[[nodiscard]] uint16_t ReadBigEndianUint16(std::span<const uint8_t> bytes) {
  return static_cast<uint16_t>((bytes[0] << 8) | bytes[1]);
}

}  // namespace

std::vector<uint8_t> Packet::Serialize() const {
  std::vector<uint8_t> frame(frame_length());
  frame[0] = static_cast<uint8_t>(command_id & 0xFF);
  frame[1] = static_cast<uint8_t>((command_id >> 8) & 0xFF);
  frame[2] = static_cast<uint8_t>((command_id >> 16) & 0xFF);
  frame[3] = static_cast<uint8_t>((command_id >> 24) & 0xFF);
  frame[4] = static_cast<uint8_t>(payload.size());
  std::copy(payload.begin(), payload.end(), frame.begin() + kPacketHeaderBytes);
  auto checksum_span = std::span<const uint8_t>(frame.data(), frame.size() - kPacketTrailerBytes);
  uint16_t cs = ComputeChecksum(checksum_span);
  frame[frame.size() - 2] = static_cast<uint8_t>((cs >> 8) & 0xFF);
  frame.back() = static_cast<uint8_t>(cs & 0xFF);
  return frame;
}

uint16_t ComputeChecksum(std::span<const uint8_t> bytes) {
  uint32_t sum = 0;
  for (uint8_t byte : bytes) {
    sum += byte;
  }
  sum &= 0xFFFF;
  uint16_t checksum = static_cast<uint16_t>(0xFFFF - static_cast<uint16_t>(sum));
  return checksum;
}

bool ValidateFrame(std::span<const uint8_t> frame, std::string* error) {
  if (frame.size() < kPacketHeaderBytes + kPacketTrailerBytes) {
    if (error) {
      *error = "frame too short";
    }
    return false;
  }

  uint8_t payload_len = frame[4];
  std::size_t expected_size = kPacketHeaderBytes + payload_len + kPacketTrailerBytes;
  if (frame.size() != expected_size) {
    if (error) {
      *error = "payload length does not match frame size";
    }
    return false;
  }

  std::span<const uint8_t> without_crc(frame.data(), frame.size() - kPacketTrailerBytes);
  uint16_t expected_crc = ComputeChecksum(without_crc);
  uint16_t actual_crc = ReadBigEndianUint16(frame.last(kPacketTrailerBytes));
  if (expected_crc != actual_crc) {
    if (error) {
      *error = "checksum mismatch";
    }
    return false;
  }

  return true;
}

std::optional<Packet> ParsePacket(std::span<const uint8_t> frame, std::string* error) {
  if (!ValidateFrame(frame, error)) {
    return std::nullopt;
  }

  Packet packet;
  packet.command_id = static_cast<uint32_t>(frame[0]) |
                      (static_cast<uint32_t>(frame[1]) << 8) |
                      (static_cast<uint32_t>(frame[2]) << 16) |
                      (static_cast<uint32_t>(frame[3]) << 24);

  uint8_t payload_len = frame[4];
  packet.payload.assign(frame.begin() + kPacketHeaderBytes,
                        frame.begin() + kPacketHeaderBytes + payload_len);
  packet.checksum = ReadBigEndianUint16(frame.last(kPacketTrailerBytes));
  return packet;
}

}  // namespace fujitsu::airstage

