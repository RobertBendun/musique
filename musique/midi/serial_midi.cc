#include <musique/errors.hh>
#include <musique/midi/midi.hh>
#include <musique/serialport/serialport.hh>
#include <musique/interpreter/interpreter.hh>

bool midi::Serial_Midi::supports_output() const 
{
	return true;
}

void midi::Serial_Midi::send_note_on (uint8_t channel, uint8_t note_number, uint8_t velocity) 
{	
	serialport->send(0, note_number);
}

void midi::Serial_Midi::send_note_off(uint8_t channel, uint8_t note_number, uint8_t velocity) 
{
	serialport->send(1, note_number);
}

void midi::Serial_Midi::send_program_change(uint8_t channel, uint8_t program) 
{
	unimplemented();
}

void midi::Serial_Midi::send_controller_change(uint8_t channel, uint8_t controller_number, uint8_t value) 
{
	unimplemented();
}
