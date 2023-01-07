#ifndef MUSIQUE_SERIALPORT_SERIALPORT_HH
#define MUSIQUE_SERIALPORT_SERIALPORT_HH

#include <string>
#include <serial/serial.h>
#include <stop_token>
#include <memory>
#include <array>
#include <atomic>
#include <cstdint>
#include <musique/value/number.hh>

namespace serialport{
    constexpr std::size_t MAX_STATE_COUNT = 8;
    constexpr std::size_t MAX_VALUE = 4095;
    struct State{
        std::array<std::atomic<std::uint32_t>, MAX_STATE_COUNT> state;
        int test();
        Number get(unsigned position) const;
        void set(unsigned position, std::uint32_t value);
    };
    
    void initialize();
    void event_loop(std::stop_token token, State &state);
    std::uint8_t get_byte();
}

#endif //MUSIQUE_SERIALPORT_SERIALPORT_HH