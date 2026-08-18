#pragma once
#include <cstdint>

namespace yk::core {
    // "short"  = pressed and released within short_press_us
    // "long"   = still held when long_press_us elapses; released long is swallowed
    // "repeat" = long press trigger, repeated every repeat_us while held

    class PressScanner{
        public:
            enum class Event {
                none,
                short_press,
                long_press,
                repeat
            };
            PressScanner() noexcept = default;

            //Feed raw button state (true=pressed, false=released) to the scanner.
            [[nodiscard]] Event update(bool pressed, std::uint64_t now_us) noexcept;

            void set_debounce_us(std::uint64_t v) noexcept { debounce_us_ = v; }
            void set_short_press_us(std::uint64_t v) noexcept { short_press_us_ = v; }
            void set_long_press_us(std::uint64_t v) noexcept { long_press_us_ = v; }
            void set_repeat_us(std::uint64_t v) noexcept { repeat_us_ = v; }
        private:
            std::uint64_t debounce_us_ = 5'000;
            std::uint64_t short_press_us_ = 200'000;
            std::uint64_t long_press_us_ = 1'000'000;
            std::uint64_t repeat_us_ = 500'000;

            // Internal scan state.
            std::uint64_t raw_change_us_ = 0;   // when the raw level last changed
            std::uint64_t press_us_ = 0;        // debounced press start
            bool raw_level_ = false;
            bool level_ = false;                // debounced level
            bool long_press_fired_ = false;     // current press already became "long"
            std::uint64_t last_repeat_us_ = 0;
    };
    inline PressScanner::Event PressScanner::update(
        bool pressed,
        std::uint64_t now_us) noexcept {
        
        if (pressed != raw_level_) {
            raw_level_ = pressed;
            raw_change_us_ = now_us;
        }
        const bool stable = (now_us - raw_change_us_) >= debounce_us_;
        if (stable && level_ != raw_level_) {
            level_ = raw_level_;
            if (level_) {
                // Pressed (debounced).
                press_us_ = now_us;
                long_press_fired_ = false;
            } else {
                // Released.
                if (!long_press_fired_) return Event::short_press;
                return Event::none;
            }
        }

        // While held, wait for long-press / repeat thresholds.
        if (level_) {
            if (!long_press_fired_ && (now_us - press_us_) >= long_press_us_) {
                long_press_fired_ = true;
                last_repeat_us_ = now_us;
                return Event::long_press;
            }
            if (long_press_fired_ && (now_us - last_repeat_us_) >= repeat_us_) {
                last_repeat_us_ = now_us;
                return Event::repeat;
            }
        }
        return Event::none;
    }
} // namespace yk::core