#ifndef MUSIQUE_SERIALPORT_SERIALPORT_HH
#define MUSIQUE_SERIALPORT_SERIALPORT_HH

#include <string>
#include <serial/serial.h>
#include <stop_token>
#include <memory>

namespace serialport{
    struct Device{
        int test();
    };
    
    void initialize();
    void event_loop();
    std::uint8_t get_byte();
}

#endif //MUSIQUE_SERIALPORT_SERIALPORT_HH