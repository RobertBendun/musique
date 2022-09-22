#include <musique.hh>
#include <musique_internal.hh>

#include <iostream>
#include <numeric>

/// Finds numeric value of note. This form is later used as in
/// note to midi resolution in formula octave * 12 + note_index
constexpr u8 note_index(u8 note)
{
	switch (note) {
	case 'c':  return  0;
	case 'd':  return  2;
	case 'e':  return  4;
	case 'f':  return  5;
	case 'g':  return  7;
	case 'a':  return  9;
	case 'h':  return 11;
	case 'b':  return 11;
	}
	// Parser should limit range of characters that is called with this function
	unreachable();
}

constexpr std::string_view note_index_to_string(int note_index)
{
	note_index %= 12;
	if (note_index < 0) {
		note_index = 12 + note_index;
	}

	switch (note_index) {
	case  0:	return "c";
	case  1:	return "c#";
	case  2:	return "d";
	case  3:	return "d#";
	case  4:	return "e";
	case  5:	return "f";
	case  6:	return "f#";
	case  7:	return "g";
	case  8:	return "g#";
	case  9:	return "a";
	case 10:	return "a#";
	case 11:	return "b";
	}
	unreachable();
}

Result<Value> Value::from(Token t)
{
	switch (t.type) {
	case Token::Type::Numeric:
		return Value::from(Try(Number::from(std::move(t))));

	case Token::Type::Symbol:
		return Value::from(std::string(t.source));

	case Token::Type::Keyword:
		if (t.source == "false") return Value::from(false);
		if (t.source == "nil")   return Value{};
		if (t.source == "true")  return Value::from(true);
		unreachable();

	case Token::Type::Chord:
		if (t.source.size() == 1 || (t.source.size() == 2 && t.source.back() == '#')) {
			auto maybe_note = Note::from(t.source);
			assert(maybe_note.has_value(), "Somehow parser passed invalid note literal");
			return Value::from(*maybe_note);
		}

		return Value::from(Chord::from(t.source));

	default:
		unreachable();
	}
}

Value Value::from(Explicit_Bool b)
{
	Value v;
	v.type = Value::Type::Bool;
	v.b = b;
	return v;
}

Value Value::from(Number n)
{
	Value v;
	v.type = Type::Number;
	v.n = std::move(n).simplify();
	return v;
}

Value Value::from(std::string s)
{
	Value v;
	v.type = Type::Symbol;
	v.s = std::move(s);
	return v;
}

Value Value::from(std::string_view s)
{
	Value v;
	v.type = Type::Symbol;
	v.s = std::move(s);
	return v;
}

Value Value::from(char const* s)
{
	Value v;
	v.type = Type::Symbol;
	v.s = std::move(s);
	return v;
}

Value Value::from(Block &&block)
{
	Value v;
	v.type = Type::Block;
	v.blk = std::move(block);
	return v;
}

Value Value::from(Array &&array)
{
	Value v;
	v.type = Type::Array;
	v.array = std::move(array);
	return v;
}

Value Value::from(std::vector<Value> &&array)
{
	Value v;
	v.type = Type::Array;
	v.array = Array { .elements = std::move(array) };
	return v;
}

Value Value::from(Note n)
{
	Value v;
	v.type = Type::Music;
	v.chord = { .notes = { n } };
	return v;
}

Value Value::from(Chord chord)
{
	Value v;
	v.type = Type::Music;
	v.chord = std::move(chord);
	return v;
}

Result<Value> Value::operator()(Interpreter &i, std::vector<Value> args)
{
	switch (type) {
	case Type::Intrinsic: return intr(i, std::move(args));
	case Type::Block:     return blk(i, std::move(args));
	case Type::Music:     return chord(i, std::move(args));
	default:
		return Error {
			.details = errors::Not_Callable { .type = type_name(type) },
			.location = std::nullopt,
		};
	}
}

Result<Value> Value::index(Interpreter &i, unsigned position)
{
	switch (type) {
	case Type::Block:
		return blk.index(i, position);

	case Type::Array:
		return array.index(i, position);

	default:
		assert(false, "Block indexing is not supported for this type"); // TODO(assert)
	}

	unreachable();
}

bool Value::truthy() const
{
	switch (type) {
	case Type::Bool:   return b;
	case Type::Nil:    return false;
	case Type::Number: return n != Number(0);
	case Type::Array: // for array and block maybe test emptyness?
	case Type::Block: //
	case Type::Intrinsic:
	case Type::Music:
	case Type::Symbol: return true;
	}
	unreachable();
}

bool Value::falsy() const
{
	return not truthy();
}

bool Value::operator==(Value const& other) const
{
	if (type != other.type) {
		return false;
	}

	switch (type) {
	case Type::Nil:       return true;
	case Type::Number:    return n == other.n;
	case Type::Symbol:    return s == other.s;
	case Type::Intrinsic: return intr == other.intr;
	case Type::Block:     return false; // TODO Reconsider if functions are comparable
	case Type::Bool:      return b == other.b;
	case Type::Music:     return chord == other.chord;
	case Type::Array:     return array == other.array;
	}

	unreachable();
}

usize Value::size() const
{
	switch (type) {
	case Type::Array:
		return array.size();

	case Type::Block:
		return blk.size();

	default:
		assert(false, "This type does not support Value::size()"); // TODO(assert)
	}

	unreachable();
}

std::partial_ordering Value::operator<=>(Value const& rhs) const
{
	// TODO Block - array comparison should be allowed
	if (type != rhs.type) {
		return std::partial_ordering::unordered;
	}

	switch (type) {
	case Type::Nil:
		return std::partial_ordering::equivalent;

	case Type::Bool:
		return b <=> rhs.b;

	case Type::Symbol:
		return s <=> rhs.s;

	case Type::Number:
		return n <=> rhs.n;

	case Type::Array:
		return std::lexicographical_compare_three_way(
			array.elements.begin(), array.elements.end(),
			rhs.array.elements.begin(), rhs.array.elements.end()
		);

	case Type::Music:
		return chord.notes.front() <=> rhs.chord.notes.front();

	// Block should be compared but after evaluation so for now it's Type::Block
	case Type::Block:
	case Type::Intrinsic:
		return std::partial_ordering::unordered;
	}

	unreachable();
}

std::ostream& operator<<(std::ostream& os, Value const& v)
{
	switch (v.type) {
	case Value::Type::Nil:
		return os << "nil";

	case Value::Type::Number:
		return os << v.n;

	case Value::Type::Symbol:
		return os << v.s;

	case Value::Type::Bool:
		return os << (v.b ? "true" : "false");

	case Value::Type::Intrinsic:
		return os << "<intrinsic>";

	case Value::Type::Block:
		return os << "<block>";

	case Value::Type::Array:
		return os << v.array;

	case Value::Type::Music:
		return os << v.chord;
	}
	unreachable();
}

std::string_view type_name(Value::Type t)
{
	switch (t) {
	case Value::Type::Array:     return "array";
	case Value::Type::Block:     return "block";
	case Value::Type::Bool:      return "bool";
	case Value::Type::Intrinsic: return "intrinsic";
	case Value::Type::Music:     return "music";
	case Value::Type::Nil:       return "nil";
	case Value::Type::Number:    return "number";
	case Value::Type::Symbol:    return "symbol";
	}
	unreachable();
}

Result<Value> Block::operator()(Interpreter &i, std::vector<Value> arguments)
{
	auto old_scope = std::exchange(i.env, context);
	i.enter_scope();

	if (parameters.size() != arguments.size()) {
		return errors::Wrong_Arity_Of {
			.type = errors::Wrong_Arity_Of::Function,
			 // TODO Let user defined functions have name of their first assigment (Zig like)
			 //      or from place of definition like <block at file:line:column>
			.name = "<block>",
			.expected_arity = parameters.size(),
			.actual_arity = arguments.size(),
		};
	}

	for (usize j = 0; j < parameters.size(); ++j) {
		i.env->force_define(parameters[j], std::move(arguments[j]));
	}

	Ast body_copy = body;
	auto result = i.eval(std::move(body_copy));

	i.env = old_scope;
	return result;
}

/// Helper that produces error when trying to access container with too few elements for given index
static inline Result<void> guard_index(unsigned index, unsigned size)
{
	if (index < size) return {};
	return Error {
		.details = errors::Out_Of_Range { .required_index = index, .size = size }
	};
}

// TODO Add memoization
Result<Value> Block::index(Interpreter &i, unsigned position)
{
	assert(parameters.size() == 0, "cannot index into block with parameters (for now)");
	if (body.type != Ast::Type::Sequence) {
		Try(guard_index(position, 1));
		return i.eval((Ast)body);
	}

	Try(guard_index(position, body.arguments.size()));
	return i.eval((Ast)body.arguments[position]);
}

usize Block::size() const
{
	return body.type == Ast::Type::Sequence ? body.arguments.size() : 1;
}

Result<Value> Array::index(Interpreter &, unsigned position)
{
	Try(guard_index(position, elements.size()));
	return elements[position];
}

usize Array::size() const
{
	return elements.size();
}

std::ostream& operator<<(std::ostream& os, Array const& v)
{
	os << '[';
	for (auto it = v.elements.begin(); it != v.elements.end(); ++it) {
		os << *it;
		if (std::next(it) != v.elements.end()) {
			os << "; ";
		}
	}
	return os << ']';
}

std::optional<Note> Note::from(std::string_view literal)
{
	if (literal.starts_with('p')) {
		return Note {};
	}

	if (auto const base = note_index(literal[0]); base != u8(-1)) {
		Note note { .base = base };

		while (literal.remove_prefix(1), not literal.empty()) {
			switch (literal.front()) {
			case '#': case 's': ++*note.base; break;
			case 'b': case 'f': --*note.base; break;
			default:  return note;
			}
		}
		return note;
	}
	return std::nullopt;
}

std::optional<u8> Note::into_midi_note() const
{
	return octave ? std::optional(into_midi_note(0)) : std::nullopt;
}

u8 Note::into_midi_note(i8 default_octave) const
{
	assert(bool(this->base), "Pause don't translate into MIDI");
	auto const octave = this->octave.has_value() ? *this->octave : default_octave;
	// octave is in range [-1, 9] where Note { .base = 0, .octave = -1 } is midi note 0
	return (octave + 1) * 12 + *base;
}

void Note::simplify_inplace()
{
	if (base && octave) {
		octave = std::clamp(*octave + int(*base / 12), -1, 9);
		if ((*base %= 12) < 0) {
			base = 12 + *base;
		}
	}
}

std::partial_ordering Note::operator<=>(Note const& rhs) const
{
	if (base.has_value() == rhs.base.has_value()) {
		if (!base.has_value()) {
			if (length.has_value() == rhs.length.has_value() && length.has_value()) {
				return *length <=> *rhs.length;
			}
			return std::partial_ordering::unordered;
		}

		if (octave.has_value() == rhs.octave.has_value()) {
			if (octave.has_value())
				return (12 * *octave) + *base <=> (12 * *rhs.octave) + *rhs.base;
			return *base <=> *rhs.base;
		}
	}
	return std::partial_ordering::unordered;
}

std::ostream& operator<<(std::ostream& os, Note note)
{
	note.simplify_inplace();
	if (note.base) {
		os << note_index_to_string(*note.base);
		if (note.octave) {
			os << ":oct=" << int(*note.octave);
		}
	} else {
		os << "p";
	}
	if (note.length) {
		os << ":len=" << *note.length;
	}
	return os;
}

bool Note::operator==(Note const& other) const
{
	return octave == other.octave && base == other.base && length == other.length;
}

Chord Chord::from(std::string_view source)
{
	auto note = Note::from(source);
	assert(note.has_value(), "don't know how this could happen");

	Chord chord;
	source.remove_prefix(1 + (source[1] == '#'));
	chord.notes.push_back(*std::move(note));

	if (note->base) {
		for (char digit : source) {
			chord.notes.push_back(Note { .base = note->base.value() + i32(digit - '0') });
		}
	}

	return chord;
}

Result<Value> Chord::operator()(Interpreter& interpreter, std::vector<Value> args)
{
	std::vector<Value> array;
	std::vector<Chord> current = { *this };

	enum State {
		Waiting_For_Octave,
		Waiting_For_Length,
		Waiting_For_Note
	} state = Waiting_For_Octave;

	static constexpr auto guard = Guard<1> {
		.name = "note creation",
		.possibilities = {
			"(note:music [octave:number [duration:number]])+"
		}
	};

	auto const next = [&state] {
		switch (state) {
		break; case Waiting_For_Length: state = Waiting_For_Note;
		break; case Waiting_For_Note:   state = Waiting_For_Octave;
		break; case Waiting_For_Octave: state = Waiting_For_Length;
		}
	};

	auto const update = [&state](Chord &chord, Value &arg) -> Result<void> {
		auto const resolve = [&chord](auto field, auto new_value) {
			for (auto &note : chord.notes) {
				(note.*field) = new_value;
			}
		};

		switch (state) {
		break; case Waiting_For_Octave:
			resolve(&Note::octave, arg.n.floor().as_int());
			return {};

		break; case Waiting_For_Length:
			resolve(&Note::length, arg.n);
			return {};

		default:
			return guard.yield_error();
		}
	};

	for (auto &arg : args) {
		if (is_indexable(arg.type)) {
			if (state != Waiting_For_Length && state != Waiting_For_Octave) {
				return guard.yield_error();
			}

			auto const ring_size = current.size();
			for (usize i = 0; i < arg.size() && current.size() < arg.size(); ++i) {
				current.push_back(current[i % ring_size]);
			}

			for (usize i = 0; i < current.size(); ++i) {
				if (Value value = Try(arg.index(interpreter, i % arg.size())); value.type == Value::Type::Number) {
					Try(update(current[i], value));
					continue;
				}
			}
			next();
			continue;
		}

		if (arg.type == Value::Type::Number) {
			for (auto &chord : current) {
				Try(update(chord, arg));
			}
			next();
			continue;
		}

		if (arg.type == Value::Type::Music) {
			std::transform(current.begin(), current.end(), std::back_inserter(array),
				[](Chord &c) { return Value::from(std::move(c)); });
			current.clear();
			current.push_back(arg.chord);
			state = Waiting_For_Octave;
		}
	}

	std::transform(current.begin(), current.end(), std::back_inserter(array),
		[](Chord &c) { return Value::from(std::move(c)); });

	assert(not array.empty(), "At least *this should be in this array");
	return Value::from(Array{array});
}

std::ostream& operator<<(std::ostream& os, Chord const& chord)
{
	if (chord.notes.size() == 1) {
		return os << chord.notes.front();
	}

	os << "chord[";
	for (auto it = chord.notes.begin(); it != chord.notes.end(); ++it) {
		os << *it;
		if (std::next(it) != chord.notes.end())
			os << "; ";
	}
	return os << ']';
}

Result<std::vector<Value>> flatten(Interpreter &interpreter, std::span<Value> args)
{
	std::vector<Value> result;
	for (auto &x : args) {
		if (is_indexable(x.type)) {
			for (usize i = 0; i < x.size(); ++i) {
				result.push_back(Try(x.index(interpreter, i)));
			}
		} else {
			result.push_back(std::move(x));
		}
	}
	return result;
}

Result<std::vector<Value>> flatten(Interpreter &i, std::vector<Value> args)
{
	return flatten(i, std::span(args));
}

std::size_t std::hash<Value>::operator()(Value const& value) const
{
	size_t value_hash = 0;
	switch (value.type) {
	break; case Value::Type::Nil:       value_hash = 0;
	break; case Value::Type::Number:    value_hash = std::hash<Number>{}(value.n);
	break; case Value::Type::Symbol:    value_hash = std::hash<std::string>{}(value.s);
	break; case Value::Type::Bool:      value_hash = std::hash<bool>{}(value.b);
	break; case Value::Type::Intrinsic: value_hash = ptrdiff_t(value.intr);
	break; case Value::Type::Block:     value_hash = hash_combine(std::hash<Ast>{}(value.blk.body), value.blk.parameters.size());

	break; case Value::Type::Array:
		value_hash = std::accumulate(value.array.elements.begin(), value.array.elements.end(), value_hash, [this](size_t h, Value const& v) {
			return hash_combine(h, operator()(v));
		});

	break; case Value::Type::Music:
		value_hash = std::accumulate(value.chord.notes.begin(), value.chord.notes.end(), value_hash, [](size_t h, Note const& n) {
			h = hash_combine(h, std::hash<std::optional<int>>{}(n.base));
			h = hash_combine(h, std::hash<std::optional<Number>>{}(n.length));
			h = hash_combine(h, std::hash<std::optional<i8>>{}(n.octave));
			return h;
		});
	}

	return hash_combine(value_hash, size_t(value.type));
}
