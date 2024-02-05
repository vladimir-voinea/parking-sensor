#include <Arduino.h>
#include <passive_buzzer.hpp>
#include <pitches.hpp>
#include <sleep.hpp>
#include <utility>
#include <tuple>
#include <time.hpp>
#include <debounced_button.hpp>
#include <ultrasonic_sensor.hpp>
#include <rgb_led.hpp>
#include <optional>

using namespace ardent;

using interval = std::tuple<centimeters, centimeters, rgb, std::optional<pitches>, estd::milliseconds, estd::milliseconds>;
const interval ranges[] = {
  {ultrasonic_sensor::min_distance, centimeters{20}, color::red, pitches::a4, estd::milliseconds{100}, estd::milliseconds{0}},
  {centimeters{20}, centimeters{50}, color::yellow, pitches::a4, estd::milliseconds{100}, estd::milliseconds{250}},
  {centimeters{50}, centimeters{80}, color::green, pitches::a4, estd::milliseconds{100}, estd::milliseconds{500}},
  {centimeters{80}, ultrasonic_sensor::max_distance, color::white, {}, estd::milliseconds{100}, estd::milliseconds{100}}
};

void setup() {
  Serial.begin(9600);
}

void loop() {

  static ultrasonic_sensor radar {9, 10};
  static rgb_led<common_cathode_tag> led{6, 5, 3};
  static ardent::passive_buzzer buzzer{12};
  static bool measuring = false;
  static debounced_button button{{8}, [last_sample_time = estd::milliseconds{0}]() {
      measuring = !measuring;
      if(measuring) {
        led.turn_on();
      } else {
        led.turn_off();
        buzzer.stop_beeping();
      }
    }
  };

  button.update();

  if(measuring)
  {
      constexpr auto sample_interval = estd::milliseconds{25};
      static auto last_sample_time = estd::milliseconds{0};
      [[maybe_unused]]static auto distance = radar.get_distance();

      const auto now = ardent::millis();
      if(now.value - last_sample_time.value > sample_interval.value)
      {
        const auto distance = radar.get_distance();
        last_sample_time = now;

        for(const auto& range : ranges)
        {
          if(distance.value >= std::get<0>(range).value && distance.value < std::get<1>(range).value)
          {
            const auto color = std::get<2>(range);
            led.set_color(color);
            const auto tone = std::get<3>(range);
            const auto tone_duration = std::get<4>(range);
            const auto tone_pause = std::get<5>(range);

            if(tone.has_value())
            {
              buzzer.stop_beeping();
              buzzer.start_beeping(static_cast<ardent::frequency>(tone.value()));
              if(tone_duration.value > 0) {
                ardent::sleep(tone_duration);
                buzzer.stop_beeping();
                ardent::sleep(tone_pause);
              }
            }
            break;
          }
        }
    }
  }
}
