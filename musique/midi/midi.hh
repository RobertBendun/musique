#pragma once
#include <RtMidi.h>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

// Documentation of midi messages available at http://midi.teragonaudio.com/tech/midispec.htm
namespace midi
{
	struct Connection
	{
		virtual ~Connection() = default;

		virtual bool supports_output() const = 0;

		virtual void send_note_on (uint8_t channel, uint8_t note_number, uint8_t velocity) = 0;
		virtual void send_note_off(uint8_t channel, uint8_t note_number, uint8_t velocity) = 0;
		virtual void send_program_change(uint8_t channel, uint8_t program) = 0;
		virtual void send_controller_change(uint8_t channel, uint8_t controller_number, uint8_t value) = 0;

		void send_all_sounds_off(uint8_t channel);
	};

	struct Rt_Midi : Connection
	{
		~Rt_Midi() override = default;

		bool connect_or_create_output();

		/// Connect with MIDI virtual port
		void connect_output();

		/// Connect with specific MIDI port for outputing MIDI messages
		void connect_output(unsigned target);

		/// List available ports
		void list_ports(std::ostream &out) const;


		bool supports_output() const override;

		void send_note_on (uint8_t channel, uint8_t note_number, uint8_t velocity) override;
		void send_note_off(uint8_t channel, uint8_t note_number, uint8_t velocity) override;
		void send_program_change(uint8_t channel, uint8_t program) override;
		void send_controller_change(uint8_t channel, uint8_t controller_number, uint8_t value) override;

		std::optional<RtMidiOut> output;
	};

	/// All defined controllers for controller change message
	enum class Controller : uint8_t
	{
		All_Notes_Off = 120,
	};
}
