#include <musique/serialport/serialport.hh>
#include <iostream>
#include <thread>
#include <serial/serial.h>

namespace serialport{
    int State::test(){
        return 5;
    }

    Number State::get(unsigned position) const
    {
        return Number(state[position], MAX_VALUE);
    }
    void State::set(unsigned position, std::uint32_t value)
    {
        state[position] = value;
        return;
    }
    void State::send(uint8_t message_type, uint8_t note_number)
    {
        if((head + 1) % 128 != tail){
            message_data[head] = note_number;
            message_types[head] = message_type;
            head = (head + 1)%128;
        }
        return;
    }

    void initialize()
    {
        // std::jthread testowy = std::jthread();
        return;
    }

    std::uint8_t get_byte()
    {
        return 8;
    }

    void event_loop(std::atomic<bool> &stop, State &state)
    {
        while(!stop){
            try {
                /// Search for the right device
                auto const ports = serial::list_ports();
                /*for (serial::PortInfo const& port : ports) {
                    std::cout << "Port: " << port.port << '\n';
                    std::cout << "Description: " << port.description << '\n';
                    std::cout << "Hardware ID: " << port.hardware_id << '\n';
                }*/

                int found_port = 0;
                std::string connection_port = "";
                while(found_port == 0){
                    for (serial::PortInfo const& port : ports) {
                        if(port.description.find("STM32")){
                            connection_port = port.port;
                            found_port = 1;
                            break;
                        }
                    }
                }


                /// Start connection
                serial::Serial serial_conn(connection_port, 115200, serial::Timeout::simpleTimeout(1000));



                /*if(serial_conn.isOpen())
                    std::cout << "[SERIAL] Serial open\n";
                else
                    std::cout << "[SERIAL] Serial not open\n";

                std::cout << "[SERIAL] commence serial communication\n";
                */

                /// Set up received data buffer

                while(1){
                    /// After serial connection established

                    //serial_conn.flushInput();

                    std::string result = serial_conn.readline(65536, "\n");
                    int control = result.at(0);
                    uint32_t value = std::stoi(result.substr(1, 4), nullptr, 10);

                    state.set(control - 65, value);

                    while(state.head != state.tail){
                        if(state.message_types[state.tail] == 0){
                            uint8_t bytes_to_send[3] = {0b10010000, state.message_data[state.tail], 0b00000000};
                            serial_conn.write(bytes_to_send, 3);
                        }else if(state.message_types[state.tail] == 1){
                            uint8_t bytes_to_send[3] = {0b10000000, state.message_data[state.tail], 0b00000000};
                            serial_conn.write(bytes_to_send, 3);
                        }
                        state.tail = (state.tail + 1) % 128;
                    }
                }
            } catch (std::exception &e) {
                /// No connection to the device
							state.error_message = e.what();
                // std::cerr << "Unhandled Exception: " << e.what() << '\n';
								//
							// Sleep until error message is cleared since we don't need to try always to reconnect
							while (!state.error_message->empty()) {
								std::this_thread::yield();
							}
            }
        }
        return;
    }
}

