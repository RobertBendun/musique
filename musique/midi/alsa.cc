#define MIDI_ENABLE_ALSA_SUPPORT
#include <musique/midi/midi.hh>

#include <cassert>
#include <iomanip>
#include <iostream>

/// Ensures that operation performed by ALSA was successfull
static int ensure_snd(int maybe_error, std::string_view message)
{
	if (maybe_error < 0) {
		std::cerr << "ALSA ERROR: " << message << ": " << snd_strerror(maybe_error) << std::endl;
		std::exit(1);
	}
	return maybe_error;
}

static void create_source_port(midi::ALSA &alsa);
static void create_queue(midi::ALSA &alsa);
static void connect_port(midi::ALSA &alsa);
static void send_note_event(midi::ALSA &alsa, snd_seq_event_type type, uint8_t channel, uint8_t note, uint8_t velocity);

midi::ALSA::ALSA(std::string connection_name)
	: connection_name(connection_name)
{
}

void midi::ALSA::init_sequencer()
{
	ensure_snd(snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0), "opening sequencer");
	ensure_snd(snd_seq_set_client_name(seq, connection_name.c_str()), "setting client name");
	ensure_snd(client = snd_seq_client_id(seq),                       "getting client id");
}

void midi::ALSA::connect_output(std::string const& target)
{
	assert(!connected);
	ensure_snd(snd_seq_parse_address(seq, &output_port_addr, target.c_str()), "Invalid port specification");
	create_source_port(*this);
	create_queue(*this);
	connect_port(*this);
	connected = true;
}

void midi::ALSA::connect_input(std::string const& target)
{
	ensure_snd(snd_seq_parse_address(seq, &input_port_addr, target.c_str()), "Invalid port specification");
	input_port = ensure_snd(snd_seq_create_simple_port(seq, connection_name.c_str(),
		SND_SEQ_PORT_CAP_WRITE |
		SND_SEQ_PORT_CAP_SUBS_WRITE,
		SND_SEQ_PORT_TYPE_MIDI_GENERIC |
		SND_SEQ_PORT_TYPE_APPLICATION), "create simple port");

	ensure_snd(snd_seq_connect_from(seq, input_port, input_port_addr.client, input_port_addr.port),
		"connect from");
}

void midi::ALSA::list_ports(std::ostream& out) const
{
	snd_seq_client_info_t *c;
	snd_seq_port_info_t *p;

	// Wonders of C based API <3
	snd_seq_client_info_alloca(&c);
	snd_seq_port_info_alloca(&p);

	out << " Port    Client name                      Port name\n";

	snd_seq_client_info_set_client(c, -1);
	while (snd_seq_query_next_client(seq, c) >= 0) {
		int client = snd_seq_client_info_get_client(c);

		snd_seq_port_info_set_client(p, client);
		snd_seq_port_info_set_port(p, -1);
		while (snd_seq_query_next_port(seq, p) >= 0) {
			// Port must support MIDI messages
			if (!(snd_seq_port_info_get_type(p) & SND_SEQ_PORT_TYPE_MIDI_GENERIC)) continue;

			// Both WRITE and SUBS_WRITE are required
			// TODO Aquire knowladge why
			auto const exp = SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE;
			if ((snd_seq_port_info_get_capability(p) & exp) != exp) continue;

			out <<
				std::setw(3) << std::right << snd_seq_port_info_get_client(p) << ':' <<
				std::setw(3) << std::left  << snd_seq_port_info_get_port(p) << "  " <<
				std::setw(32) << std::left << snd_seq_client_info_get_name(c) << " " <<
				snd_seq_port_info_get_name(p) << '\n';
		}
	}
	out << std::flush;
}

void midi::ALSA::send_note_on(uint8_t channel, uint8_t note_number, uint8_t velocity)
{
	send_note_event(*this, SND_SEQ_EVENT_NOTEON, channel, note_number, velocity);
}

void midi::ALSA::send_note_off(uint8_t channel, uint8_t note_number, uint8_t velocity)
{
	send_note_event(*this, SND_SEQ_EVENT_NOTEOFF, channel, note_number, velocity);
}

void midi::ALSA::send_program_change(uint8_t channel, uint8_t program)
{
	snd_seq_event_t ev;
	snd_seq_ev_clear(&ev);
	ev.queue = queue;
	ev.source.port = 0;
	ev.flags = SND_SEQ_TIME_STAMP_TICK;
	ev.type = SND_SEQ_EVENT_PGMCHANGE;
	ev.time.tick = 0;
	ev.dest = output_port_addr;

	snd_seq_ev_set_fixed(&ev);
	ev.data.control.channel = channel;
	ev.data.control.value = program;
	ensure_snd(snd_seq_event_output(seq, &ev), "output event");
	ensure_snd(snd_seq_drain_output(seq), "drain output queue");
}

void midi::ALSA::send_controller_change(uint8_t channel, uint8_t controller_number, uint8_t value)
{
	snd_seq_event_t ev;
	snd_seq_ev_clear(&ev);
	ev.queue = queue;
	ev.source.port = 0;
	ev.flags = SND_SEQ_TIME_STAMP_TICK;
	ev.type = SND_SEQ_EVENT_CONTROLLER;
	ev.time.tick = 0;
	ev.dest = output_port_addr;

	snd_seq_ev_set_fixed(&ev);
	ev.data.control.channel = channel;
	ev.data.control.param = controller_number;
	ev.data.control.value = value;
	ensure_snd(snd_seq_event_output(seq, &ev), "output event");
	ensure_snd(snd_seq_drain_output(seq), "drain output queue");
}

void midi::ALSA::input_event_loop(std::stop_token stop_token)
{
	int const poll_fd_count = snd_seq_poll_descriptors_count(seq, POLLIN);
	std::vector<pollfd> poll_fds(poll_fd_count);
	ensure_snd(snd_seq_poll_descriptors(seq, poll_fds.data(), poll_fds.size(), POLLIN), "poll descriptors");
	while (!stop_token.stop_requested()) {
		if (poll(poll_fds.data(), poll_fds.size(), -1) < 0) {
			return;
		}
		while (!stop_token.stop_requested()) {
			snd_seq_event_t *event;
			if (snd_seq_event_input(seq, &event) < 0) { break; }
			if (!event) { continue; }

			switch (event->type) {
			case SND_SEQ_EVENT_NOTEON:
				if (!event->data.note.velocity)
					goto noteoff;
				if (note_on_callback)
					note_on_callback(event->data.note.channel, event->data.note.note, event->data.note.velocity);
				break;

			case SND_SEQ_EVENT_NOTEOFF:
			noteoff:
				if (note_off_callback)
					note_off_callback(event->data.note.channel, event->data.note.note);
				break;

			default:
				// TODO Add all events as callbacks
				break;
			}
		}
	}
}

bool midi::ALSA::supports_output() const
{
	return output_port < 0;
}

bool midi::ALSA::supports_input () const
{
	return input_port < 0;
}

static void create_source_port(midi::ALSA &alsa)
{
	snd_seq_port_info_t *pinfo;
	snd_seq_port_info_alloca(&pinfo);

	snd_seq_port_info_set_port(pinfo, alsa.output_port);
	snd_seq_port_info_set_port_specified(pinfo, 1);
	snd_seq_port_info_set_name(pinfo, alsa.connection_name.c_str());

	snd_seq_port_info_set_capability(pinfo, 0);
	snd_seq_port_info_set_type(pinfo, SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
	ensure_snd(snd_seq_create_port(alsa.seq, pinfo), "create port");
}

static void create_queue(midi::ALSA &alsa)
{
	ensure_snd(alsa.queue = snd_seq_alloc_named_queue(alsa.seq, alsa.connection_name.c_str()), "create queue");
	snd_seq_queue_tempo_t *queue_tempo;
	snd_seq_queue_tempo_alloca(&queue_tempo);

	// We assume time division as ticks per quarter
	snd_seq_queue_tempo_set_tempo(queue_tempo, 500'000); // 120 BPM
	snd_seq_queue_tempo_set_ppq(queue_tempo, 96);
	ensure_snd(snd_seq_set_queue_tempo(alsa.seq, alsa.queue, queue_tempo), "set queue tempo");
}

static void connect_port(midi::ALSA &alsa)
{
	ensure_snd(snd_seq_connect_to(alsa.seq, alsa.output_port, alsa.output_port_addr.client, alsa.output_port_addr.port), "connect to port");
}

static void send_note_event(midi::ALSA &alsa, snd_seq_event_type type, uint8_t channel, uint8_t note, uint8_t velocity)
{
	snd_seq_event_t ev;
	snd_seq_ev_clear(&ev);
	ev.queue = alsa.queue;
	ev.source.port = alsa.output_port;
	ev.flags = SND_SEQ_TIME_STAMP_TICK;

	ev.type = type;
	ev.time.tick = 0;
	ev.dest = alsa.output_port_addr;

	snd_seq_ev_set_fixed(&ev);
	ev.data.note.channel = channel;
	ev.data.note.note = note;
	ev.data.note.velocity = velocity;

	ensure_snd(snd_seq_event_output(alsa.seq, &ev), "output event");
	ensure_snd(snd_seq_drain_output(alsa.seq), "drain output queue");
}
