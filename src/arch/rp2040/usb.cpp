#include "arch/rp2040/usb.hpp"

#include "core/command.hpp"
#include "pico/time.h"
#include "tusb.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <span>

namespace yk::rp2040{
    namespace {

        // USB descriptor - device + 1HID keyboard + 1 CDC-ACM interface pair
        constexpr std::uint8_t kStrManufacturer = 1;
        constexpr std::uint8_t kStrProduct = 2;
        constexpr std::uint8_t kStrSerial = 3;
        constexpr std::uint8_t kStrConfig = 4;

        constexpr std::uint8_t kItfCdc = 0;  // CDC occupies two interfaces: 0 and 1
        constexpr std::uint8_t kItfHid = 2;
        constexpr std::uint8_t kItfTotal = 3;

        constexpr std::uint8_t kEpCdcNotif = 0x81;
        constexpr std::uint8_t kEpCdcOut = 0x02;
        constexpr std::uint8_t kEpCdcIn = 0x82;
        constexpr std::uint8_t kEpHidIn = 0x83;

        tusb_desc_device_t const desc_device = {
            .bLength = sizeof(tusb_desc_device_t),
            .bDescriptorType = TUSB_DESC_DEVICE,
            .bcdUSB = 0x0200,
            .bDeviceClass = TUSB_CLASS_MISC,
            .bDeviceSubClass = MISC_SUBCLASS_COMMON,
            .bDeviceProtocol = MISC_PROTOCOL_IAD,
            .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
            .idVendor = 0xCAFE,
            .idProduct = 0x0001,
            .bcdDevice = 0x0100,
            .iManufacturer = kStrManufacturer,
            .iProduct = kStrProduct,
            .iSerialNumber = kStrSerial,
            .bNumConfigurations = 1,
        };

        std::array<std::uint8_t, 45> const desc_hid_report = {
            0x05, 0x01,  // Usage Page (Generic Desktop)
            0x09, 0x06,  // Usage (Keyboard)
            0xA1, 0x01,  // Collection (Application)
            0x05, 0x07,  //   Usage Page (Key Codes)
            0x19, 0xE0,  //   Usage Minimum (224)   modifiers
            0x29, 0xE7,  //   Usage Maximum (231)
            0x15, 0x00,  //   Logical Minimum (0)
            0x25, 0x01,  //   Logical Maximum (1)
            0x75, 0x01,  //   Report Size (1)
            0x95, 0x08,  //   Report Count (8)
            0x81, 0x02,  //   Input (Data, Variable, Absolute)
            0x95, 0x01,  //   Report Count (1)
            0x75, 0x08,  //   Report Size (8)
            0x81, 0x01,  //   Input (Constant)      reserved
            0x95, 0x06,  //   Report Count (6)
            0x75, 0x08,  //   Report Size (8)
            0x15, 0x00,  //   Logical Minimum (0)
            0x25, 0x65,  //   Logical Maximum (101)
            0x05, 0x07,  //   Usage Page (Key Codes)
            0x19, 0x00,  //   Usage Minimum (0)
            0x29, 0x65,  //   Usage Maximum (101)
            0x81, 0x00,  //   Input (Data, Array)   key codes
            0xC0,        // End Collection
        };

        // Config descriptor
        std::array<std::uint8_t, TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN + TUD_CDC_DESC_LEN> const desc_configuration = {
            TUD_CONFIG_DESCRIPTOR(1, kItfTotal, kStrConfig,
                                TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN +
                                    TUD_CDC_DESC_LEN,
                                TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
            TUD_HID_DESCRIPTOR(kItfHid, 0, HID_ITF_PROTOCOL_KEYBOARD,
                            desc_hid_report.size(), kEpHidIn, 16, 10),
            TUD_CDC_DESCRIPTOR(kItfCdc, kStrConfig, kEpCdcNotif, 8, kEpCdcOut,
                            kEpCdcIn, 64),
        };

        char const* const string_desc[] = {
            "\x04\x09",                      // 0: language list (0x0409 = English)
            "Pico Yubikey",                  // 1: manufacturer
            "OTP Token (HOTP/TOTP)",         // 2: product
            "000001",                        // 3: serial
            "Pico Yubikey Config",           // 4: configuration
        };

        std::array<std::uint16_t, 33> string_buffer{};

        // Keystroke queue (ASCII -> HID press, delay. release)
        struct KeyStroke{
            std::uint8_t modifier;
            std::uint8_t keycode;
        };

        std::array<KeyStroke, 64> key_queue{};
        std::size_t key_head = 0;
        std::size_t key_tail = 0;
        bool key_pressed = false;
        std::uint64_t key_press_us = 0;
        constexpr std::uint64_t kKeyHeldUs = 20'000;

        // ASCII -> (modifier, keycode) using the standard HID usage table.
        KeyStroke ascii_to_key(std::uint8_t ch) noexcept {
            constexpr std::uint8_t kLShift = 0x02;
            if (ch >= 'a' && ch <= 'z') return {0, static_cast<std::uint8_t>(0x04 + ch - 'a')};
            if (ch >= 'A' && ch <= 'Z') return {kLShift, static_cast<std::uint8_t>(0x04 + ch - 'A')};
            if (ch >= '0' && ch <= '9') return {0, static_cast<std::uint8_t>(0x1E + ch - '0')};
            switch (ch) {
                case ' ': return {0, 0x2C};
                case '\n': case '\r': return {0, 0x28};
                case '\t': return {0, 0x2B};
                case '-': return {0, 0x2D};
                case '_': return {kLShift, 0x2D};
                case '=': return {0, 0x2E};
                case '+': return {kLShift, 0x2E};
                case '[': return {0, 0x2F};
                case '{': return {kLShift, 0x2F};
                case ']': return {0, 0x30};
                case '}': return {kLShift, 0x30};
                case '\\': return {0, 0x31};
                case '|': return {kLShift, 0x31};
                case ';': return {0, 0x33};
                case ':': return {kLShift, 0x33};
                case '\'': return {0, 0x34};
                case '"': return {kLShift, 0x34};
                case '`': return {0, 0x35};
                case '~': return {kLShift, 0x35};
                case ',': return {0, 0x36};
                case '<': return {kLShift, 0x36};
                case '.': return {0, 0x37};
                case '>': return {kLShift, 0x37};
                case '/': return {0, 0x38};
                case '?': return {kLShift, 0x38};
                case '!': return {kLShift, 0x1E};  // Shift-1
                case '@': return {kLShift, 0x1F};  // Shift-2
                case '#': return {kLShift, 0x20};
                case '$': return {kLShift, 0x21};
                case '%': return {kLShift, 0x22};
                case '^': return {kLShift, 0x23};
                case '&': return {kLShift, 0x24};
                case '*': return {kLShift, 0x25};
                case '(': return {kLShift, 0x26};
                case ')': return {kLShift, 0x27};
                default: return {0, 0};  // unmapped
            }
        }

        // CDC config transport 
        std::array<std::uint8_t, 64> cdc_frame{};
        std::size_t cdc_frame_len = 0;

        void process_cdc_frame(
            yk::core::cmd::Session& session,
            std::uint64_t epoch_now_secs) noexcept {

            std::array<std::uint8_t, 96> resp{};
            const std::size_t n = yk::core::cmd::dispatch(
                session,
                std::span<const std::uint8_t>(cdc_frame).first(cdc_frame_len), resp,
                epoch_now_secs);
            if (!tud_cdc_connected()) return;
            tud_cdc_write(resp.data(), n);
            const std::uint8_t eot = yk::core::cmd::kFrameEnd;
            tud_cdc_write(&eot, 1);
            tud_cdc_write_flush();
        }

    } // namespace

    void usb_init() noexcept {tusb_init();}
    void usb_poll(yk::core::cmd::Session& session, std::uint64_t epoch_now_secs) noexcept{
        tud_task();
    
        // CDC: accumulate until the frame terminator, then dispatch.
        while (tud_cdc_available()) {
            std::uint8_t b = 0;
            tud_cdc_read(&b, 1);
            if (cdc_frame_len < cdc_frame.size()) cdc_frame[cdc_frame_len++] = b;
            if (b == yk::core::cmd::kFrameEnd || cdc_frame_len == cdc_frame.size()) {
                // Strip the terminator byte before dispatch.
                if (cdc_frame_len > 0 && cdc_frame[cdc_frame_len - 1] == yk::core::cmd::kFrameEnd)
                    cdc_frame_len--;
                process_cdc_frame(session, epoch_now_secs);
                cdc_frame_len = 0;
            }
        }

        // Keystroke state machine: press, hold, release, next.
        const std::uint64_t now = time_us_64();
        if (!key_pressed && key_head != key_tail) {
            if (tud_hid_ready() && tud_hid_keyboard_report(
                                    0, key_queue[key_head].modifier,
                                    &key_queue[key_head].keycode)) {
                key_pressed = true;
                key_press_us = now;
            }
        } else if (key_pressed && (now - key_press_us) >= kKeyHeldUs) {
            const std::uint8_t release[6] = {0, 0, 0, 0, 0, 0};
            tud_hid_keyboard_report(0, 0, release);
            key_pressed = false;
            key_head = (key_head + 1) % key_queue.size();
        }
    }
    
    void usb_type_text (std::span<const char> text) noexcept {
        for(const char ch : text){
            const auto stroke = ascii_to_key(static_cast<std::uint8_t>(ch));
            if (stroke.keycode == 0 && ch != ' ') continue;  // unmapped, skip
            const std::size_t next = (key_tail + 1) % key_queue.size();
            if (next == key_head) break;  // queue full, drop the rest
            key_queue[key_tail] = stroke;
            key_tail = next;
        }
    }

    // TinyUSB callbacks

    extern "C" {
        uint8_t const* tud_descriptor_device_cb(void){
            return reinterpret_cast<std::uint8_t const*>(&desc_device);
        }
        uint8_t const* tud_descriptor_configuration_cb(uint8_t index) {
            (void)index;
            return desc_configuration.data();
        }

        uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
            (void)langid;
            std::size_t count = 0;
            if (index == 0) {
                // Language list: English (0x0409).
                string_buffer[1] = 0x0409;
                count = 1;
            } else {
                if (index >= sizeof(string_desc) / sizeof(string_desc[0])) return nullptr;
                const char* str = string_desc[index];
                for (count = 0; str[count] && count < 32; count++)
                    string_buffer[1 + count] = static_cast<std::uint8_t>(str[count]);
            }
            string_buffer[0] = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(TUSB_DESC_STRING) << 8) | (2 * count + 2));
            return string_buffer.data();
        }

        uint8_t const* tud_hid_descriptor_report_cb(uint8_t itf) {
            (void)itf;
            return desc_hid_report.data();
        }

        void tud_hid_set_report_cb(
            uint8_t itf, 
            uint8_t report_id,
            hid_report_type_t report_type, 
            uint8_t const* buffer,
            uint16_t bufsize) {

            (void)itf; 
            (void)report_id; 
            (void)report_type; 
            (void)buffer; 
            (void)bufsize;
        }

        uint16_t tud_hid_get_report_cb(
            uint8_t itf, 
            uint8_t report_id,
            hid_report_type_t report_type, 
            uint8_t* buffer,
            uint16_t reqlen) {

            (void)itf; 
            (void)report_id; 
            (void)report_type; 
            (void)buffer; 
            (void)reqlen;

            return 0;
        }

    } // extern "C"
} // namespace yk::rp2040