#ifndef MUSIQUE_SERIALPORT_SERIALPORT_HH
#define MUSIQUE_SERIALPORT_SERIALPORT_HH

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <musique/value/number.hh>
#include <optional>
#include <serial/serial.h>
#include <string>

namespace serialport{
    constexpr std::size_t MAX_STATE_COUNT = 8;
    constexpr std::size_t MAX_VALUE = 4095;
    struct State{
        std::array<std::atomic<std::uint32_t>, MAX_STATE_COUNT> state;
        std::array<std::atomic<std::uint8_t>, 128> message_types;
        std::array<std::atomic<std::uint8_t>, 128> message_data;
        std::atomic<std::uint8_t> head = 0;
        std::atomic<std::uint8_t> tail = 0;

				std::optional<std::string> error_message;

        int test();
        Number get(unsigned position) const;
        void send(uint8_t message_type, uint8_t note_number);
        void set(unsigned position, std::uint32_t value);
    };

    void initialize();
    void event_loop(std::atomic<bool> &stop, State &state);
    std::uint8_t get_byte();
}

#endif //MUSIQUE_SERIALPORT_SERIALPORT_HH
