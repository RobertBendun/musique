#ifndef MUSIQUE_CONTEXT_HH
#define MUSIQUE_CONTEXT_HH

#include <chrono>
#include <memory>
#include <musique/common.hh>
#include <musique/errors.hh>
#include <musique/midi/midi.hh>
#include <musique/value/note.hh>
#include <musique/value/number.hh>


/// Context holds default values for music related actions
struct Context
{
	/// Default note octave
	i8 octave = 4;

	/// Default note length
	Number length = Number(1, 4);

	/// Default BPM
	unsigned bpm = 120;

	/// Port that is currently used
	std::shared_ptr<midi::Connection> port;

	using Port_Number = unsigned int;

	/// Connections that have been established so far
	static std::unordered_map<midi::Port, std::shared_ptr<midi::Connection>> established_connections;

	/// Establish connection to given port
	///
	/// If port number wasn't provided connect to first existing one or create one
	std::optional<Error> connect(std::optional<midi::Port>);

	/// Fills empty places in Note like octave and length with default values from context
	Note fill(Note) const;

	/// Converts length to seconds with current bpm
	std::chrono::duration<float> length_to_duration(std::optional<Number> length) const;

	std::shared_ptr<Context> parent;
};

#endif
