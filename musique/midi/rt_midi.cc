#include <musique/midi/midi.hh>
#include <musique/errors.hh>

// Copyright notice for RtMidi library
__asm__(R"license(.ident  "\
RtMidi: realtime MIDI i/o C++ classes\
Copyright (c) 2003-2021 Gary P. Scavone"
)license");

void midi::Rt_Midi::list_ports(std::ostream &out) const
try {
	RtMidiIn input;
	RtMidiOut output;
	{
		unsigned ports_count = input.getPortCount();
		out << "Found " << ports_count << " MIDI input ports\n";
		for (auto i = 0u; i < ports_count; ++i) {
			std::string port_name = input.getPortName(i);
			std::cout << "  Input Port #" << i+1 << ": " << port_name << "\n";
		}
	}
	std::cout << std::endl;
	{
		unsigned ports_count = output.getPortCount();
		out << "Found " << ports_count << " MIDI output ports\n";
		for (auto i = 0u; i < ports_count; ++i) {
			std::string port_name = output.getPortName(i);
			std::cout << "  Output Port #" << i+1 << ": " << port_name << "\n";
		}
	}
} catch (RtMidiError &error) {
	// TODO(error)
	std::cerr << "Failed to use MIDI connection: " << error.getMessage() << std::endl;
	std::exit(33);
}

void midi::Rt_Midi::connect_output()
try {
	ensure(not output.has_value(), "Reconeccting is not supported yet");
	output.emplace();
	output->openVirtualPort("Musique");
} catch (RtMidiError &error) {
	// TODO(error)
	std::cerr << "Failed to use MIDI connection: " << error.getMessage() << std::endl;
	std::exit(33);
}

void midi::Rt_Midi::connect_output(unsigned target)
try {
	ensure(not output.has_value(), "Reconeccting is not supported yet");
	output.emplace();
	output->openPort(target);
} catch (RtMidiError &error) {
	// TODO(error)
	std::cerr << "Failed to use MIDI connection: " << error.getMessage() << std::endl;
	std::exit(33);
}

bool midi::Rt_Midi::supports_output() const
{
	return bool(output);
}

template<std::size_t N>
inline void send_message(RtMidiOut &out, std::array<std::uint8_t, N> message)
try {
	out.sendMessage(message.data(), message.size());
} catch (RtMidiError &error) {
	// TODO(error)
	std::cerr << "Failed to use MIDI connection: " << error.getMessage() << std::endl;
	std::exit(33);
}

enum : std::uint8_t
{
	Control_Change = 0b1011'0000,
	Note_Off       = 0b1000'0000,
	Note_On        = 0b1001'0000,
	Program_Change = 0b1100'0000,
};

void midi::Rt_Midi::send_note_on(uint8_t channel, uint8_t note_number, uint8_t velocity)
{
	send_message(*output, std::array { std::uint8_t(Note_On + channel), note_number, velocity });
}

void midi::Rt_Midi::send_note_off(uint8_t channel, uint8_t note_number, uint8_t velocity)
{
	send_message(*output, std::array { std::uint8_t(Note_Off + channel), note_number, velocity });
}

void midi::Rt_Midi::send_program_change(uint8_t channel, uint8_t program)
{
	send_message(*output, std::array { std::uint8_t(channel), program });
}

void midi::Rt_Midi::send_controller_change(uint8_t channel, uint8_t controller_number, uint8_t value)
{
	send_message(*output, std::array { std::uint8_t(Control_Change + channel), controller_number, value });
}

