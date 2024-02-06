#include <Arduino.h>

#include <debounced_button.hpp>
#include <event_cycle_detector.hpp>
#include <optional>
#include <passive_buzzer.hpp>
#include <pitches.hpp>
#include <rgb_led.hpp>
#include <sleep.hpp>
#include <extra/time.hpp>
#include <tuple>
#include <ultrasonic_sensor.hpp>
#include <utility>

#include "debounce.hpp"

using namespace ardent;

struct interval
{
    centimeters start, end;
    rgb color;
    pitches pitch;
    estd::milliseconds duration, pause;
};

constexpr interval ranges[] PROGMEM = {
    {ultrasonic_sensor::min_distance, centimeters{30}, color::red, pitches::a4, estd::milliseconds{100},
     estd::milliseconds{0}},
    {centimeters{30}, centimeters{70}, color::yellow, pitches::a4, estd::milliseconds{100}, estd::milliseconds{250}},
    {centimeters{70}, centimeters{150}, color::green, pitches::a4, estd::milliseconds{100}, estd::milliseconds{500}},
    {centimeters{150}, ultrasonic_sensor::max_distance, color::white, pitches::a4, estd::milliseconds{0},
     estd::milliseconds{0}}};

interval get_interval(int index)
{
    interval result;
    memcpy_P(&result, &ranges[index], sizeof(interval));
    return result;
}

void setup()
{
    Serial.begin(9600);
}

void loop()
{
    static ultrasonic_sensor radar{9, 10};
    static rgb_led<common_cathode_tag> led{6, 5, 3};
    static ardent::passive_buzzer buzzer{12};
    static bool measuring = false;
    static auto press_detector = event_cycle_detector(push_button{8}, []() {
        measuring = !measuring;
        if (!measuring)
        {
            led.turn_off();
            buzzer.stop_beeping();
        }
    });

    press_detector.update();
    buzzer.update();

    if (measuring)
    {
        constexpr auto sample_interval = estd::milliseconds{25};
        static auto last_sample_time = estd::milliseconds{0};

        const auto now = ardent::millis();
        if (now.value - last_sample_time.value > sample_interval.value)
        {
            const auto distance = radar.get_distance();
            last_sample_time = now;

            for (auto i = 0u; i < sizeof(ranges) / sizeof(ranges[0]); i++)
            {
                const auto range = get_interval(i);
                if (distance.value >= range.start.value && distance.value < range.end.value)
                {
                    const auto color = range.color;
                    led.set_color(color);
                    const auto tone = range.pitch;
                    const auto tone_duration = range.duration;
                    const auto tone_pause = range.pause;

                    if (tone_duration.value && !buzzer.is_beeping())
                    {
                        buzzer.beep(static_cast<ardent::frequency>(tone), tone_duration,
                                    [&]() { buzzer.mute(tone_pause, []() {}); });
                        break;
                    }
                }
            }
        }
    }
}
