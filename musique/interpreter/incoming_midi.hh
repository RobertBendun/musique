#ifndef MUSIQUE_INCOMING_MIDI
#define MUSIQUE_INCOMING_MIDI

#include <musique/interpreter/interpreter.hh>

struct Interpreter::Incoming_Midi_Callbacks
{
	Value note_on{};
	Value note_off{};

	inline Incoming_Midi_Callbacks() = default;

	Incoming_Midi_Callbacks(Incoming_Midi_Callbacks &&) = delete;
	Incoming_Midi_Callbacks(Incoming_Midi_Callbacks const&) = delete;

	Incoming_Midi_Callbacks& operator=(Incoming_Midi_Callbacks &&) = delete;
	Incoming_Midi_Callbacks& operator=(Incoming_Midi_Callbacks const&) = delete;


	inline void add_callbacks(midi::Connection &midi, Interpreter &interpreter)
	{
		register_callback(midi.note_on_callback, note_on, interpreter);
		register_callback(midi.note_off_callback, note_off, interpreter);
	}

	template<typename ...T>
	inline void register_callback(std::function<void(T...)> &target, Value &callback, Interpreter &i)
	{
		if (&callback == &note_on || &callback == &note_off) {
			// This messages have MIDI note number as second value, so they should be represented
			// in our own note abstraction, not as numbers.
			target = [interpreter = &i, callback = &callback](T ...source_args)
			{
				if (!std::holds_alternative<Nil>(callback->data)) {
					std::vector<Value> args { Number(source_args)... };
					args[1] = Note {
						.base = i32(std::get<Number>(args[1].data).num % 12),
						.octave = std::get<Number>(args[1].data).num / 12
					};
					auto result = (*callback)(*interpreter, std::move(args));
					// We discard this since callback is running in another thread.
					(void) result;
				}
			};
		} else {
			// Generic case, preserve all passed parameters as numbers
			target = [interpreter = &i, callback = &callback](T ...source_args)
			{
				if (!std::holds_alternative<Nil>(callback->data)) {
					auto result = (*callback)(*interpreter, { Number(source_args)...  });
					// We discard this since callback is running in another thread.
					(void) result;
				}
			};
		}
	}
};


#endif
