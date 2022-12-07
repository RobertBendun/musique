#include <musique/serialport/serialport.hh>
#include <iostream>
#include <thread>
#include <serial/serial.h>

namespace serialport{
    int Device::test(){
        return 5;
    }

    void initialize()
    {
        std::cout << "initialize serial\n";
        std::jthread testowy = std::jthread();
        std::cout << "initialize serial\n";
    }

    std::uint8_t get_byte()
    {
        return 8;
    }

    void event_loop()
    {
        while(1){
            try {
                /// Search for the right device
                auto const ports = serial::list_ports();

                for (serial::PortInfo const& port : ports) {
                    std::cout << "Port: " << port.port << '\n';
                    std::cout << "Description: " << port.description << '\n';
                    std::cout << "Hardware ID: " << port.hardware_id << '\n';
                }

                /// Start connection
                serial::Serial serial_conn("/dev/ttyACM0", 115200, serial::Timeout::simpleTimeout(1000));

                if(serial_conn.isOpen())
                    std::cout << "[SERIAL] Serial open\n";
                else
                    std::cout << "[SERIAL] Serial not open\n";
                
                std::cout << "[SERIAL] commence serial communication\n";
                
                while(1){
                    std::string result = serial_conn.read(1);
                    std::cout << int(result[0]);
                }
            } catch (std::exception &e) {
                std::cerr << "Unhandled Exception: " << e.what() << '\n';
                std::cerr << "Terminating serial connection\n";
                return;
            }
        }
        return;
    }
}

