# pico-yubikey

A YubiKey-style OTP token for the Raspberry Pi Pico (RP2040), written in
idiomatic **C++20**. It presents itself as a USB keyboard and types
one-time passwords — press the button, the code appears wherever the cursor
is.

| Slot | Short press        | Long press        |
|------|--------------------|-------------------|
| 1    | HOTP or TOTP code  | HOTP or TOTP code |
| 2    | (same model, two slots) |            |

Both slots are configurable independently: HOTP, TOTP, challenge-response,
or a static password.

## Feature set

- **HOTP** (RFC 4226) and **TOTP** (RFC 6238), 6-8 digits
- **Challenge-response** (HMAC-SHA1) over the config channel
- **Static password** slot (types a stored string)
- **Two independent slots**: short press = slot 1, long press = slot 2
- **Config over USB CDC**: set slots, secrets, time, and query status
- **Persistent storage** in on-chip flash with CRC-32 integrity checking and a
  monotonic HOTP counter (flash wear-aware, never rolls back)
- **Debounced button** with short/long-press detection (pure logic, unit-tested)
- **SHA-1 / HMAC-SHA1 written from scratch** in clean C++20

## Quick start

```sh
./dev.sh build     # build firmware (needs Pico SDK)
./dev.sh test      # run host tests in Docker
./dev.sh flash     # flash connected Pico via USB
./dev.sh renode    # launch Renode emulator
```

## Repository layout

```
include/core/     portable logic, host-testable, no hardware dependencies
  sha1.hpp        streaming SHA-1
  hmac.hpp        HMAC-SHA1
  otp.hpp         HOTP / TOTP
  slot.hpp        slot model + button-press behavior
  storage.hpp     flash record serialization + CRC-32
  command.hpp     config protocol parser (pure)
  input.hpp       debounced short/long-press scanner
  crc.hpp         consteval CRC-32 table
src/core/         implementations (sha1, command)
include/arch/     RP2040 bindings (gpio, flash, usb, registers)
src/arch/         RP2040 implementations (flash, usb/TinyUSB)
src/main.cpp      firmware entry point
tests/            host unit tests (RFC known-answer vectors)
renode/           Renode emulator config and robot tests
```

## Build

### Host tests (no hardware needed)

```sh
cmake -B build-host
cmake --build build-host
ctest --test-dir build-host --output-on-failure
```

This builds and runs the portable core against RFC 4226/6238/2202 vectors.

Or via Docker (works on macOS and Linux):

```sh
./dev.sh test
```

### Firmware

Requires the Pico SDK and an ARM toolchain (`arm-none-eabi-gcc`):

```sh
cmake -B build-fw -DPICO_SDK_PATH=$HOME/pico-sdk
cmake --build build-fw
```

Output: `build-fw/src/pico_yubikey.uf2`. Hold BOOTSEL, plug in, copy the file
to the mass-storage device.

On macOS with the toolchain from Homebrew, the SDK selects it automatically;
if your toolchain is elsewhere, point CMake at it with `PICO_TOOLCHAIN_PATH`.

### Flashing

With picotool installed, one command handles BOOTSEL entry and flashing:

```sh
./dev.sh flash
```

This will prompt you to hold BOOTSEL on the Pico, then load and reboot it.

Install picotool:
```sh
brew install picotool         # macOS
# or build from source: https://github.com/raspberrypi/picotool
```

### Renode emulation

Run the firmware without hardware using Renode:

```sh
./dev.sh renode          # interactive session with UART console
./dev.sh renode-test     # headless robot test (CI-friendly)
```

Install Renode:
```sh
brew install renode       # macOS
# or: https://github.com/renode/renode/releases (Linux x86_64/ARM64)
```

## Hardware

- Raspberry Pi Pico (RP2040), 2 MB flash
- Button on **GP15** (active low, internal pull-up)
- Onboard LED on GP25 (heartbeat)

## Config protocol

The device enumerates as a CDC-ACM serial port plus a keyboard. Config
commands are binary frames terminated by `0x04` (EOT):

| Byte | Command            | Payload                                  |
|------|--------------------|------------------------------------------|
| 0x01 | GET_STATUS         | -> status, slot count, per-slot modes    |
| 0x02 | SET_SLOT           | slot, mode, digits, period, secret...    |
| 0x03 | CLEAR_SLOT         | slot                                     |
| 0x04 | GET_SLOT           | slot -> status, mode, digits, secret...  |
| 0x05 | CHALLENGE          | slot, len, challenge -> HMAC-SHA1 (20 B) |
| 0x06 | SET_TIME           | epoch (LE u64) -- device adds offset     |
| 0x07 | GET_TIME           | -> epoch (LE u64)                        |
| 0x08 | SET_STATIC         | slot, len, text                          |

Responses start with a status byte (`0` = ok). E.g. on a POSIX host:

```sh
printf '\x01\x04' > /dev/tty.usbmodem*        # GET_STATUS
printf '\x02\x00\x01\x06\x1e\x00\x00\x00\x14<secret>\x04' > /dev/tty.usbmodem*
```

## dev.sh commands

| Command       | What it does                                      |
|---------------|---------------------------------------------------|
| `test`        | Build and run host tests in Docker (default)      |
| `build`       | Build firmware (requires Pico SDK + ARM toolchain)|
| `flash`       | Flash .uf2 to Pico via picotool (USB BOOTSEL)    |
| `renode`      | Launch Renode with UART console                   |
| `renode-test` | Run Renode headless robot test (CI)               |
| `shell`       | Interactive shell inside Docker container         |
| `lint`        | Run clang-tidy on core headers in Docker          |

## Design notes

- The core (`include/core`) is pure and unit-tested on the host; the arch
  layer (`arch/rp2040`) is the only thing that touches the SDK.
- Flash records use an explicit little-endian layout + CRC-32, so host and
  device always agree regardless of struct padding.
- The HOTP counter lives in flash and is write-only-up: `FlashStore::save`
  refuses to roll a stored counter back, protecting against races.
- The button's short/long-press classifier is a small state machine with real
  debouncing -- unit-tested with simulated timing.
