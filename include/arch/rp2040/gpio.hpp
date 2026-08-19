#pragma once

#include "pico/stdlib.h"

namespace yk::rp2040{
	// onboard led with sw heartbeat
	class Led{
	public:
		explicit Led(std::uint32_t pin) noexcept : pin_(pin) {
			gpio_init(pin_);
			gpio_set_dir(pin_, GPIO_OUT);
		}
		void on() noexcept {gpio_put(pin_, true);}
		void off() noexcept {gpio_put(pin_, false);}
		void toggle() noexcept {gpio_put(pin_, !gpio_get(pin_));}

	private:
		std::uint32_t pin_;
	};

	// Push-button, active low internal pull-up
	class Button{
	public:
		explicit Button(std::uin32_t pin) noexcept : pin_(pin) {
			gpio_init(pin_);
			gpio_set_dir(pin_, GPIO_IN);
			gpio_pull_up(pin_);
		}

		[[nodiscard]] bool is_pressed() const noexcept {return !gpio_get(pin_);}
	private:
		std::uint32_t pin_;
	};
	
}//namespace yk::rp2040