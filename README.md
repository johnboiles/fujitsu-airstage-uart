# Fujitsu Airstage UART Tools

This repository explores the Fujitsu Airstage AMUH24KUAS UART protocol that is normally used by the UTY-TFSXJ4 Wi-Fi adapter. Using Saleae captures from a real adapter, we reverse-engineered the packet framing and a number of register-level interactions. The project provides a standalone C++ library for parsing/constructing packets and a command-line tool for dumping human-readable traces from Saleae CSV exports.

NOTE: TX/RX are swapped in the captures!

## Packet Format Summary

* **Break** — The bus idles with `0xFF 0xFF 0x00 0x00`. The decoder treats this as a frame of type `Break`.
* **Packet framing**
  * 32‑bit command identifier (`uint32_t`, **little-endian**)
  * 8‑bit payload length `N`
  * `N` payload bytes
  * 16‑bit checksum (big-endian). The checksum is the ones'-complement of the 16-bit sum of every preceding byte, so the entire frame sums to `0xFFFF`.

The command identifier determines the semantics of the payload. Observed values:

| Command | Direction | Meaning |
|---------|-----------|---------|
| `0x00000000` | Indoor → Wi-Fi | Handshake request (payload `0x00 0x00 0x00 0x00`)
| `0x00000000` | Wi-Fi → Indoor | Handshake response (payload `0x01`)
| `0x00000001` | Indoor → Wi-Fi | Extended handshake payload (observed `0x00 0x01 0x00 0x01 0x00 0x04 0x00 0x00`)
| `0x00000001` | Wi-Fi → Indoor | Extended handshake acknowledgement (`0x01`)
| `0x00000003` | Indoor → Wi-Fi | Register read request — payload is a sequence of 16-bit big-endian register addresses
| `0x00000003` | Wi-Fi → Indoor | Register read response — payload starts with status (`0x01` == OK) followed by repeated `{address_hi, address_lo, value_hi, value_lo}` entries
| `0x00000002` | Indoor → Wi-Fi | Single register write
| `0x00000004` | Indoor → Wi-Fi | Single register write (used for mode/fan commands)
| `0x00000005` | Indoor → Wi-Fi | Multi-register write (multiple `{address,value}` pairs)
| `0x0000000X` (same id) | Wi-Fi → Indoor | Write acknowledgement; payload is a single status byte (`0x00` or `0x01` observed)

Other command identifiers default to “raw” output from the CLI until more traffic is decoded.

### Known Registers

Based on the captures, the following registers have clear meanings:

| Address | Name | Notes |
|---------|------|-------|
| `0x1000` | `PowerState` | Observed `0x0001` while unit is running (exact semantics still tentative)
| `0x1001` | `OperationMode` | `0` Auto, `1` Cool, `2` Dry, `3` Fan, `4` Heat
| `0x1002` | `TemperatureSetpoint` | `0x00C8` == 200 decimal (68 °F). Appears to be temperature in tenths of degrees Fahrenheit
| `0x1003` | `FanSpeed` | `0` Auto, `2` Quiet, `5` Low, `8` Medium, `11` High
| `0x1108` | `EnergySavingFan` | `0` disabled, `1` enabled

All other addresses are still labelled generically by the tooling, but the CLI prints their raw values for further analysis.

## Building

```bash
cmake -S . -B build
cmake --build build
```

This produces:

* `libfujitsu_airstage.a` — static library containing the packet/capture utilities
* `fujitsu_dump` — command-line decoder tool

## Command-Line Decoder

```
./build/fujitsu_dump [--gap <seconds>] <capture.csv>...
```

Example output excerpt:

```text
[ 29.222162] RX PACKET id=0x00000000 len=4 command=Handshake0 payload=[0x00 0x00 0x00 0x00]
[ 29.242543] TX PACKET id=0x00000000 len=1 command=Handshake0 payload=[0x01]
[ 29.867107] RX PACKET id=0x00000003 len=4 ReadRequest addresses=[0x0001, 0x0004]
[ 29.893267] TX PACKET id=0x00000003 len=9 ReadResponse status=0x01 values=[0x0001=0x0001(1), 0x0004=0xFFFF(65535)]
```

The decoder understands read/write transactions and decorates known registers with human-friendly names where available. Unknown packets are emitted with raw hex payloads so that additional behaviour can be reverse-engineered iteratively.

## Next Steps

* Expand the register database as more behaviour is understood.
* Cross-check temperature scaling, power-state semantics, and other register meanings.
* Add encoders for higher-level control surfaces (e.g., convenience functions for changing mode/fan speed).
* Integrate the parser into ESPHome or other automation stacks once the protocol coverage is complete.

