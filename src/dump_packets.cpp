#include "fujitsu/capture_reader.h"
#include "fujitsu/messages.h"
#include "fujitsu/packet.h"
#include "fujitsu/register_db.h"

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

using fujitsu::airstage::BusDirection;
using fujitsu::airstage::CommandToString;
using fujitsu::airstage::DecodeReadRequest;
using fujitsu::airstage::DecodeReadResponse;
using fujitsu::airstage::DecodeWriteRequest;
using fujitsu::airstage::DecodeWriteResponse;
using fujitsu::airstage::Frame;
using fujitsu::airstage::FrameSet;
using fujitsu::airstage::LoadCapture;
using fujitsu::airstage::LookupRegister;
using fujitsu::airstage::Packet;
using fujitsu::airstage::ParsePacket;
using fujitsu::airstage::ToString;

namespace {

constexpr double kDefaultGapThreshold = 0.004;  // seconds

std::string FormatHex(uint16_t value) {
  std::ostringstream oss;
  oss << "0x" << std::uppercase << std::hex << std::setw(4) << std::setfill('0') << value;
  return oss.str();
}

std::string FormatRegister(uint16_t address) {
  std::ostringstream oss;
  oss << FormatHex(address);
  if (auto info = LookupRegister(address)) {
    oss << "(" << info->name << ")";
  }
  return oss.str();
}

std::string FormatRegisterValue(uint16_t address, uint16_t value) {
  std::ostringstream oss;
  oss << FormatRegister(address) << "=" << FormatHex(value) << "(" << std::dec
      << static_cast<int>(value) << ")";
  return oss.str();
}

std::string FormatByteVector(const std::vector<uint8_t>& bytes) {
  std::ostringstream oss;
  oss << std::uppercase << std::hex << std::setfill('0');
  for (std::size_t i = 0; i < bytes.size(); ++i) {
    if (i) {
      oss << ' ';
    }
    oss << "0x" << std::setw(2) << static_cast<int>(bytes[i]);
  }
  return oss.str();
}

void PrintUsage(const char* program) {
  std::cout << "Usage: " << program << " [--gap <seconds>] <capture.csv>...\n";
  std::cout << "  --gap    Override inter-byte gap threshold for frame detection (default "
            << kDefaultGapThreshold << ")\n";
}

void DescribePacket(const Frame& frame, const Packet& packet, std::ostream& os) {
  os << "PACKET id=0x" << std::uppercase << std::hex << std::setw(8) << std::setfill('0')
     << packet.command_id << std::dec;
  os.fill(' ');
  os << " len=" << packet.payload_length();

  auto read_request = DecodeReadRequest(packet);
  if (read_request) {
    os << " ReadRequest addresses=[";
    for (std::size_t i = 0; i < read_request->addresses.size(); ++i) {
      if (i) {
        os << ", ";
      }
      os << FormatRegister(read_request->addresses[i]);
    }
    os << "]";
    return;
  }

  auto read_response = DecodeReadResponse(packet);
  if (read_response) {
    os << " ReadResponse status=0x" << std::uppercase << std::hex << std::setw(2)
       << std::setfill('0') << static_cast<int>(read_response->status) << std::dec;
    os.fill(' ');
    os << " values=[";
    for (std::size_t i = 0; i < read_response->values.size(); ++i) {
      if (i) {
        os << ", ";
      }
      const auto& entry = read_response->values[i];
      os << FormatRegisterValue(entry.address, entry.value);
    }
    os << "]";
    return;
  }

  auto write_request = DecodeWriteRequest(packet);
  if (write_request) {
    os << " WriteRequest values=[";
    for (std::size_t i = 0; i < write_request->values.size(); ++i) {
      if (i) {
        os << ", ";
      }
      const auto& entry = write_request->values[i];
      os << FormatRegisterValue(entry.address, entry.value);
    }
    os << "]";
    return;
  }

  auto write_response = DecodeWriteResponse(packet);
  if (write_response) {
    os << " WriteResponse status=0x" << std::uppercase << std::hex << std::setw(2)
       << std::setfill('0') << static_cast<int>(write_response->status) << std::dec;
    os.fill(' ');
    return;
  }

  os << " command=" << CommandToString(packet.command_id);
  if (!packet.payload.empty()) {
    os << " payload=[" << FormatByteVector(packet.payload) << "]";
  }
}

void DescribeFrame(const Frame& frame, std::ostream& os) {
  os.fill(' ');
  os << "[" << std::setw(10) << std::fixed << std::setprecision(6) << frame.start_time << "] ";
  os << ToString(frame.direction) << ' ';

  switch (frame.type) {
    case Frame::Type::Break:
      os << "BREAK";
      break;
    case Frame::Type::Raw:
      os << "RAW " << FormatByteVector(frame.bytes);
      break;
    case Frame::Type::Packet: {
      std::string error;
      auto packet = ParsePacket(frame.bytes, &error);
      if (!packet) {
        os << "PACKET(parse error: " << error << ") raw=" << FormatByteVector(frame.bytes);
      } else {
        DescribePacket(frame, *packet, os);
      }
      break;
    }
  }
}

}  // namespace

int main(int argc, char* argv[]) {
  double gap_threshold = kDefaultGapThreshold;
  std::vector<std::filesystem::path> paths;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--help" || arg == "-h") {
      PrintUsage(argv[0]);
      return 0;
    }
    if (arg == "--gap" && i + 1 < argc) {
      gap_threshold = std::stod(argv[++i]);
      continue;
    }
    paths.emplace_back(arg);
  }

  if (paths.empty()) {
    PrintUsage(argv[0]);
    return 1;
  }

  for (std::size_t idx = 0; idx < paths.size(); ++idx) {
    const auto& path = paths[idx];
    try {
      FrameSet frames = LoadCapture(path, gap_threshold);
      std::cout << "== " << path << " ==\n";
      for (const auto& frame : frames.frames) {
        DescribeFrame(frame, std::cout);
        std::cout << '\n';
      }
      if (idx + 1 < paths.size()) {
        std::cout << '\n';
      }
    } catch (const std::exception& ex) {
      std::cerr << "Error processing " << path << ": " << ex.what() << '\n';
      return 2;
    }
  }

  return 0;
}

