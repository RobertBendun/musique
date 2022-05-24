#include <musique.hh>

template<typename T, typename Index, typename Expected>
concept Indexable = requires(T t, Index i) {
	{ t[i] } -> std::convertible_to<Expected>;
};

/// Create hash out of note literal like `c` or `e#`
constexpr u16 hash_note(Indexable<usize, char> auto const& note)
{
	/// TODO Some assertion that we have snd character
	u8 snd = note[1];
	if (snd != '#') snd = 0;
	return u8(note[0]) | (snd << 8);
}

/// Finds numeric value of note. This form is later used as in
/// note to midi resolution in formula octave * 12 + note_index
constexpr u8 note_index(Indexable<usize, char> auto const& note)
{
	switch (hash_note(note)) {
	case hash_note("c"):  return  0;
	case hash_note("c#"): return  1;
	case hash_note("d"):  return  2;
	case hash_note("d#"): return  3;
	case hash_note("e"):  return  4;
	case hash_note("e#"): return  5;
	case hash_note("f"):  return  5;
	case hash_note("f#"): return  6;
	case hash_note("g"):  return  7;
	case hash_note("g#"): return  8;
	case hash_note("a"):  return  9;
	case hash_note("a#"): return 10;
	case hash_note("h"):  return 11;
	case hash_note("b"):  return 11;
	case hash_note("h#"): return 12;
	case hash_note("b#"): return 12;
	}
	// This should be unreachable since parser limits what character can pass as notes
	// but just to be sure return special value
	return -1;
}

constexpr std::string_view note_index_to_string(u8 note_index)
{
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
	case 12:	return "b#";
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

		unimplemented("only simple note values (like c or e#) are supported now");

	default:
		unimplemented();
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

Value Value::from(Note n)
{
	Value v;
	v.type = Type::Music;
	v.note = n;
	return v;
}

Result<Value> Value::operator()(Interpreter &i, std::vector<Value> args)
{
	switch (type) {
	case Type::Intrinsic: return intr(i, std::move(args));
	case Type::Block:     return blk(i, std::move(args));
	case Type::Music:
		{
			assert(args.size() == 1 || args.size() == 2, "music value can be called only in form note <octave> [<length>]"); // TODO(assert)
			assert(args[0].type == Type::Number, "expected octave to be a number"); // TODO(assert)

			note.octave = args[0].n.as_int();

			if (args.size() == 2) {
				assert(args[1].type == Type::Number, "expected length to be a number"); // TODO(assert)
				note.length = args[1].n;
			}

			return *this;
		}
	default:
		// TODO Fill location
		return errors::not_callable(std::nullopt, type);
	}
}

Result<Value> Value::index(Interpreter &i, unsigned position)
{
	switch (type) {
	case Type::Block:
		return blk.index(i, position);

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
	if (type != other.type)
		return false;

	switch (type) {
	case Type::Nil:       return true;
	case Type::Number:    return n == other.n;
	case Type::Symbol:    return s == other.s;
	case Type::Intrinsic: return intr == other.intr;
	case Type::Block:     return false; // TODO Reconsider if functions are comparable
	case Type::Bool:      return b == other.b;
	case Type::Music:     return note == other.note;
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
		return os << v.note;
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

	assert(parameters.size() == arguments.size(), "wrong number of arguments");

	for (usize j = 0; j < parameters.size(); ++j) {
		i.env->force_define(parameters[j], std::move(arguments[j]));
	}

	Ast body_copy = body;
	auto result = i.eval(std::move(body_copy));

	i.env = old_scope;
	return result;
}

// TODO Add memoization
Result<Value> Block::index(Interpreter &i, unsigned position)
{
	assert(parameters.size() == 0, "cannot index into block with parameters (for now)");
	if (body.type != Ast::Type::Sequence) {
		assert(position == 0, "Out of range"); // TODO(assert)
		return i.eval((Ast)body);
	}

	assert(position < body.arguments.size(), "Out of range"); // TODO(assert)
	return i.eval((Ast)body.arguments[position]);
}

usize Block::size() const
{
	return body.type == Ast::Type::Sequence ? body.arguments.size() : 1;
}

Result<Value> Array::index(Interpreter &, unsigned position)
{
	assert(position < elements.size(), "Out of range"); // TODO(assert)
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
	if (auto note = note_index(literal); note != u8(-1)) {
		return Note { .base = note };
	}
	return std::nullopt;
}

std::optional<u8> Note::into_midi_note() const
{
	return octave ? std::optional(into_midi_note(0)) : std::nullopt;
}

u8 Note::into_midi_note(i8 default_octave) const
{
	auto const octave = this->octave.has_value() ? *this->octave : default_octave;
	// octave is in range [-1, 9] where Note { .base = 0, .octave = -1 } is midi note 0
	return (octave + 1) * 12 + base;
}

std::ostream& operator<<(std::ostream& os, Note const& note)
{
	os << note_index_to_string(note.base);
	os << ":oct=";
	if (note.octave) {
		os << int(*note.octave);
	} else {
		os << '_';
	}
	os << ":len=";
	if (note.length) {
		os << *note.length;
	} else {
		os << '_';
	}
	return os;
}

bool Note::operator==(Note const& other) const
{
	return octave == other.octave && base == other.base && length == other.length;
}
