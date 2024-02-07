#include "debounce.hpp"
#include <Adafruit_GFX.h>
#include <Arduino.h>
#include <debounced_button.hpp>
#include <event_cycle_detector.hpp>
#include <optional>
#include <passive_buzzer.hpp>
#include <pitches.hpp>
#include <remote_control.hpp>
#include <rgb_led.hpp>
#include <sleep.hpp>
#include <time.hpp>
#include <tuple>
#include <ultrasonic_sensor.hpp>
#include <utility>

#include <Arduino.h>
#include <Wire.h>

// Simple I2C test for ebay 128x64 oled.
// Use smaller faster AvrI2c class in place of Wire.
// Edit AVRI2C_FASTMODE in SSD1306Ascii.h to change the default I2C frequency.
//
#include "SSD1306Ascii.h"
#include "SSD1306AsciiAvrI2c.h"

// 0X3C+SA0 - 0x3C or 0x3D
#define I2C_ADDRESS 0x3C

// Define proper RST_PIN if required.
#define RST_PIN -1

SSD1306AsciiAvrI2c oled;

void resetScreen()
{
    oled.clear();
    oled.println(F("Push to start"));
}

void startDisplay()
{
#if RST_PIN >= 0
    oled.begin(&Adafruit128x64, I2C_ADDRESS, RST_PIN);
#else  // RST_PIN >= 0
    oled.begin(&Adafruit128x64, I2C_ADDRESS);
#endif // RST_PIN >= 0
    // Call oled.setI2cClock(frequency) to change from the default frequency.

    oled.setFont(Adafruit5x7);
    oled.displayRemap(true);
    resetScreen();
}

using namespace ardent;

struct interval
{
    centimeters start, end;
    rgb color;
    pitches pitch;
    ardent::milliseconds duration, pause;
};

constexpr interval ranges[] PROGMEM = {{ultrasonic_sensor::min_distance, centimeters{30}, color::red, pitches::a4,
                                        ardent::milliseconds{100}, ardent::milliseconds{0}},
                                       {centimeters{30}, centimeters{70}, color::yellow, pitches::a4,
                                        ardent::milliseconds{100}, ardent::milliseconds{250}},
                                       {centimeters{70}, centimeters{150}, color::green, pitches::a4,
                                        ardent::milliseconds{100}, ardent::milliseconds{500}},
                                       {centimeters{150}, ultrasonic_sensor::max_distance, color::white, pitches::a4,
                                        ardent::milliseconds{0}, ardent::milliseconds{0}}};

interval get_interval(int index)
{
    interval result;
    memcpy_P(&result, &ranges[index], sizeof(interval));
    return result;
}

void setup()
{
    Serial.begin(9600);
    startDisplay();
}

uint8_t number_begin = 0;

void loop()
{
    static ultrasonic_sensor radar{9, 10};
    static rgb_led<common_cathode_tag> led{6, 5, 3};
    static ardent::passive_buzzer buzzer{12};

    static bool measuring = false;
    auto toggle_operation = []() {
        measuring = !measuring;
        if (!measuring)
        {
            led.turn_off();
            buzzer.stop_beeping();
            resetScreen();
        }
        else
        {
            oled.clear();

            static const auto constant_text = String(F("Distance: "));
            oled.print(constant_text.c_str());

            static const auto constant_width = static_cast<uint8_t>(oled.strWidth(constant_text.c_str()));
            number_begin = constant_width + 1;
            oled.setCol(number_begin);
        }
    };

    static ardent::remote_control remote{11, [&](remote_control_buttons button) {
                                             switch (button)
                                             {
                                             case remote_control_buttons::VOL_minus:
                                                 Serial.println("Mute");
                                                 buzzer.mute();
                                                 break;
                                             case remote_control_buttons::VOL_plus:
                                                 Serial.println("Unmute");
                                                 buzzer.unmute();
                                                 break;
                                             case remote_control_buttons::PAUSE:
                                                 toggle_operation();
                                                 break;
                                             default:
                                                 break;
                                             }
                                         }};

    static auto press_detector = event_cycle_detector(push_button{8}, [&]() { toggle_operation(); });

    press_detector.update();
    buzzer.update();
    remote.update();

    if (measuring)
    {
        constexpr auto sample_interval = ardent::milliseconds{25};
        static auto last_sample_time = ardent::milliseconds{0};

        static auto last_display_time = ardent::milliseconds{0};
        static auto display_interval = ardent::milliseconds{500};

        const auto now = ardent::millis();
        if (now.value - last_sample_time.value > sample_interval.value)
        {
            const auto distance = radar.get_distance();
            last_sample_time = now;

            if (now.value - last_display_time.value > display_interval.value)
            {
                last_display_time = now;

                if (distance.value < ultrasonic_sensor::max_distance.value &&
                    distance.value > ultrasonic_sensor::min_distance.value)
                {
                    static const String cm(F(" cm"));
                    oled.clearField(number_begin, 0, oled.displayWidth() - number_begin);
                    oled.println(String(distance.value) + cm);
                }
                else
                {
                    oled.clearField(number_begin, 0, oled.displayWidth() - number_begin);
                    oled.println(F("?"));
                }
            }

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
