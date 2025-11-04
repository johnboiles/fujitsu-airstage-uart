#pragma once

#include "fujitsu/packet.h"

#include <filesystem>
#include <string>
#include <vector>

namespace fujitsu::airstage {

enum class BusDirection {
  Rx,  // Captured as "RX" by the Saleae trace (indoor unit -> module)
  Tx,  // Captured as "TX" (module -> indoor unit)
};

inline constexpr const char* ToString(BusDirection dir) {
  return dir == BusDirection::Rx ? "RX" : "TX";
}

struct Frame {
  enum class Type {
    Packet,
    Break,   // 0xFF 0xFF 0x00 0x00 idle signalling
    Raw,     // bytes that could not be interpreted as a packet
  };

  Type type = Type::Raw;
  BusDirection direction = BusDirection::Rx;
  double start_time = 0.0;  // seconds from start of capture
  std::vector<uint8_t> bytes;  // raw bytes as captured (including header for packets)
};

struct FrameSet {
  std::vector<Frame> frames;
};

// Parse a Saleae CSV capture into frames grouped by packets. `gap_threshold` controls the
// maximum time between consecutive bytes that are considered part of the same frame.
// Returns all parsed frames (including raw/break frames if present). Throws std::runtime_error
// on I/O failures.
[[nodiscard]] FrameSet LoadCapture(const std::filesystem::path& path, double gap_threshold = 0.004);

}  // namespace fujitsu::airstage

