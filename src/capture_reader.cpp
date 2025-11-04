#include "fujitsu/capture_reader.h"

#include "fujitsu/packet.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace fujitsu::airstage {

namespace {

struct CsvByte {
  BusDirection direction = BusDirection::Rx;
  double time = 0.0;
  uint8_t value = 0;
  bool has_error = false;
};

struct PendingBuffer {
  std::vector<CsvByte> bytes;
  std::optional<double> last_time;
};

int DirectionIndex(BusDirection dir) {
  return dir == BusDirection::Rx ? 0 : 1;
}

bool SplitCsvLine(const std::string& line, std::vector<std::string>* fields) {
  fields->clear();
  std::string field;
  bool in_quotes = false;
  for (std::size_t i = 0; i < line.size(); ++i) {
    char ch = line[i];
    if (in_quotes) {
      if (ch == '"') {
        if (i + 1 < line.size() && line[i + 1] == '"') {
          field.push_back('"');
          ++i;
        } else {
          in_quotes = false;
        }
      } else {
        field.push_back(ch);
      }
    } else {
      if (ch == '"') {
        in_quotes = true;
      } else if (ch == ',') {
        fields->push_back(field);
        field.clear();
      } else {
        field.push_back(ch);
      }
    }
  }
  fields->push_back(field);
  return true;
}

uint8_t ParseByteValue(const std::string& token) {
  std::size_t idx = 0;
  int base = 10;
  if (token.size() > 2 && token[0] == '0' && (token[1] == 'x' || token[1] == 'X')) {
    idx = 2;
    base = 16;
  }
  return static_cast<uint8_t>(std::stoul(token.substr(idx), nullptr, base));
}

void EmitRawFrame(const PendingBuffer& buffer, BusDirection dir, std::size_t start, std::size_t end,
                  std::vector<Frame>* out) {
  if (start >= end) {
    return;
  }
  Frame frame;
  frame.type = Frame::Type::Raw;
  frame.direction = dir;
  frame.start_time = buffer.bytes[start].time;
  frame.bytes.reserve(end - start);
  for (std::size_t i = start; i < end; ++i) {
    frame.bytes.push_back(buffer.bytes[i].value);
  }
  out->push_back(std::move(frame));
}

void EmitBreakFrame(const PendingBuffer& buffer, BusDirection dir, std::size_t start,
                    std::vector<Frame>* out) {
  Frame frame;
  frame.type = Frame::Type::Break;
  frame.direction = dir;
  frame.start_time = buffer.bytes[start].time;
  frame.bytes = {0xFF, 0xFF, 0x00, 0x00};
  out->push_back(std::move(frame));
}

void EmitPacketFrame(const PendingBuffer& buffer, BusDirection dir, std::size_t start, std::size_t length,
                     std::vector<Frame>* out) {
  Frame frame;
  frame.type = Frame::Type::Packet;
  frame.direction = dir;
  frame.start_time = buffer.bytes[start].time;
  frame.bytes.reserve(length);
  for (std::size_t i = 0; i < length; ++i) {
    frame.bytes.push_back(buffer.bytes[start + i].value);
  }
  out->push_back(std::move(frame));
}

void ParseAvailable(PendingBuffer& buffer, BusDirection dir, std::vector<Frame>* out,
                    bool final_flush = false) {
  while (!buffer.bytes.empty()) {
    // Break frame detection
    if (buffer.bytes.size() >= 4 && buffer.bytes[0].value == 0xFF &&
        buffer.bytes[1].value == 0xFF && buffer.bytes[2].value == 0x00 &&
        buffer.bytes[3].value == 0x00) {
      EmitBreakFrame(buffer, dir, 0, out);
      buffer.bytes.erase(buffer.bytes.begin(), buffer.bytes.begin() + 4);
      continue;
    }

    if (buffer.bytes.size() < kPacketHeaderBytes) {
      if (final_flush) {
        EmitRawFrame(buffer, dir, 0, buffer.bytes.size(), out);
        buffer.bytes.clear();
      }
      break;
    }

    uint8_t payload_length = buffer.bytes[4].value;
    std::size_t total_length = kPacketHeaderBytes + payload_length + kPacketTrailerBytes;
    if (buffer.bytes.size() < total_length) {
      if (final_flush) {
        EmitRawFrame(buffer, dir, 0, buffer.bytes.size(), out);
        buffer.bytes.clear();
      }
      break;
    }

    std::vector<uint8_t> candidate(total_length);
    for (std::size_t i = 0; i < total_length; ++i) {
      candidate[i] = buffer.bytes[i].value;
    }

    if (!ValidateFrame(candidate)) {
      // Unable to decode a packet at the buffer head. Emit the first byte as raw and retry.
      EmitRawFrame(buffer, dir, 0, 1, out);
      buffer.bytes.erase(buffer.bytes.begin());
      continue;
    }

    EmitPacketFrame(buffer, dir, 0, total_length, out);
    buffer.bytes.erase(buffer.bytes.begin(), buffer.bytes.begin() + total_length);
  }
}

}  // namespace

FrameSet LoadCapture(const std::filesystem::path& path, double gap_threshold) {
  std::ifstream input(path);
  if (!input.is_open()) {
    throw std::runtime_error("failed to open capture file: " + path.string());
  }

  std::string line;
  if (!std::getline(input, line)) {
    return {};
  }

  std::vector<CsvByte> events;
  std::vector<std::string> fields;

  while (std::getline(input, line)) {
    if (line.empty()) {
      continue;
    }
    SplitCsvLine(line, &fields);
    if (fields.size() < 5) {
      continue;
    }

    std::string name = fields[0];
    if (name == "RX" || name == "\"RX\"") {
      name = "RX";
    } else if (name == "TX" || name == "\"TX\"") {
      name = "TX";
    }

    BusDirection direction = (name == "RX") ? BusDirection::Rx : BusDirection::Tx;

    if (fields[1] != "data" && fields[1] != "\"data\"") {
      continue;
    }

    double time = std::stod(fields[2]);
    uint8_t value = ParseByteValue(fields[4]);
    bool has_error = fields.size() > 5 && !fields[5].empty();

    events.push_back(CsvByte{direction, time, value, has_error});
  }

  std::stable_sort(events.begin(), events.end(),
                   [](const CsvByte& a, const CsvByte& b) { return a.time < b.time; });

  std::array<PendingBuffer, 2> buffers;
  FrameSet result;

  for (const auto& event : events) {
    PendingBuffer& buffer = buffers[DirectionIndex(event.direction)];
    if (buffer.last_time.has_value()) {
      double delta = event.time - *buffer.last_time;
      if (delta > gap_threshold && !buffer.bytes.empty()) {
        ParseAvailable(buffer, event.direction, &result.frames, /*final_flush=*/true);
      }
    }

    buffer.last_time = event.time;
    buffer.bytes.push_back(event);
    ParseAvailable(buffer, event.direction, &result.frames, /*final_flush=*/false);
  }

  // Flush remaining buffers at end of file.
  for (std::size_t i = 0; i < buffers.size(); ++i) {
    ParseAvailable(buffers[i], i == 0 ? BusDirection::Rx : BusDirection::Tx, &result.frames,
                   /*final_flush=*/true);
  }

  std::stable_sort(result.frames.begin(), result.frames.end(),
                   [](const Frame& a, const Frame& b) { return a.start_time < b.start_time; });

  return result;
}

}  // namespace fujitsu::airstage

