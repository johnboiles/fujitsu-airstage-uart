#include "fujitsu/register_db.h"

namespace fujitsu::airstage {

namespace {

struct Entry {
  uint16_t address;
  RegisterInfo info;
};

constexpr Entry kRegisterTable[] = {
    {0x1000, {"PowerState", "Observed as 0x0001 when the system is running"}},
    {0x1001, {"OperationMode", "0=Auto, 1=Cool, 2=Dry, 3=Fan, 4=Heat"}},
    {0x1002, {"TemperatureSetpoint", "0x00C8 corresponds to 68°F (tentative scaling)"}},
    {0x1003, {"FanSpeed", "0=Auto, 2=Quiet, 5=Low, 8=Medium, 11=High"}},
    {0x1108, {"EnergySavingFan", "1 enables low-energy fan mode"}},
};

}  // namespace

std::optional<RegisterInfo> LookupRegister(uint16_t address) {
  for (const auto& entry : kRegisterTable) {
    if (entry.address == address) {
      return entry.info;
    }
  }
  return std::nullopt;
}

}  // namespace fujitsu::airstage

