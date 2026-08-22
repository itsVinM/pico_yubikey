#ifndef _PICO_YUBIKEY_TUSB_CONFIG_H_
#define _PICO_YUBIKEY_TUSB_CONFIG_H_

// CFG_TUSB_MCU (OPT_MCU_RP2040) and CFG_TUSB_OS (OPT_OS_PICO) are supplied by
// the Pico SDK's tinyusb_device target; the guards keep this file safe to
// include in any context.

#ifndef CFG_TUSB_MCU
#define CFG_TUSB_MCU OPT_MCU_RP2040
#endif

#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS OPT_OS_PICO
#endif

// RP2040 is a full-speed-only device controller.
#ifndef CFG_TUSB_RHPORT0_MODE
#define CFG_TUSB_RHPORT0_MODE (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)
#endif

#ifndef CFG_TUD_ENDPOINT0_SIZE
#define CFG_TUD_ENDPOINT0_SIZE 64
#endif

// Device classes in use: one HID keyboard + one CDC-ACM (config transport).
#define CFG_TUD_HID 1
#define CFG_TUD_CDC 1
#define CFG_TUD_MSC 0
#define CFG_TUD_MIDI 0
#define CFG_TUD_VENDOR 0

#define CFG_TUD_HID_EP_BUFSIZE 16
#define CFG_TUD_CDC_RX_BUFSIZE 128
#define CFG_TUD_CDC_TX_BUFSIZE 128

#endif // _PICO_YUBIKEY_TUSB_CONFIG_H_
