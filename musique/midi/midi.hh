#pragma once
#include <cstdint>
#include <functional>

#ifdef MIDI_ENABLE_ALSA_SUPPORT
#include <alsa/asoundlib.h>
#include <ostream>
#include <stop_token>

#ifdef assert
#undef assert
#endif

#endif

// Documentation of midi messages available at http://midi.teragonaudio.com/tech/midispec.htm
namespace midi
{
	struct Connection
	{
		virtual bool supports_output() const = 0;
		virtual bool supports_input () const = 0;

		virtual void send_note_on (uint8_t channel, uint8_t note_number, uint8_t velocity) = 0;
		virtual void send_note_off(uint8_t channel, uint8_t note_number, uint8_t velocity) = 0;
		virtual void send_program_change(uint8_t channel, uint8_t program) = 0;
		virtual void send_controller_change(uint8_t channel, uint8_t controller_number, uint8_t value) = 0;

		void send_all_sounds_off(uint8_t channel);

		std::function<void(uint8_t, uint8_t, uint8_t)> note_on_callback = nullptr;
		std::function<void(uint8_t, uint8_t)> note_off_callback = nullptr;
	};

#ifdef MIDI_ENABLE_ALSA_SUPPORT
	struct ALSA : Connection
	{
		explicit ALSA(std::string connection_name);

		/// Initialize connection with ALSA
		void init_sequencer();

		/// Connect with specific ALSA port for outputing MIDI messages
		void connect_output(std::string const& target);

		/// Connect with specific ALSA port for reading MIDI messages
		void connect_input(std::string const& target);

		/// List available ports
		void list_ports(std::ostream &out) const;

		bool supports_output() const override;
		bool supports_input () const override;

		void send_note_on (uint8_t channel, uint8_t note_number, uint8_t velocity) override;
		void send_note_off(uint8_t channel, uint8_t note_number, uint8_t velocity) override;
		void send_program_change(uint8_t channel, uint8_t program) override;
		void send_controller_change(uint8_t channel, uint8_t controller_number, uint8_t value) override;

		void input_event_loop(std::stop_token);

		/// Name that is used by ALSA to describe our connection
		std::string connection_name = {};
		bool connected = false;
		snd_seq_t *seq = nullptr;
		long client = 0;
		int queue = 0;
		snd_seq_addr_t input_port_addr{};
		snd_seq_addr_t output_port_addr{};
		int input_port = -1;
		int output_port = -1;
	};
#endif

	/// All defined controllers for controller change message
	enum class Controller : uint8_t
	{
		All_Notes_Off = 120,
	};
}
