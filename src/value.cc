#include <musique.hh>

template<typename T, typename Index, typename Expected>
concept Indexable = requires(T t, Index i) {
	{ t[i] } -> std::convertible_to<Expected>;
};

/// Create hash out of note literal like `c` or `e#`
constexpr u16 hash_note(Indexable<usize, char> auto const& note)
{
	return u8(note[0]) | (note[1] << 8);
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

Result<Value> Value::from(Token t)
{
	switch (t.type) {
	case Token::Type::Numeric:
		return Value::number(Try(Number::from(std::move(t))));

	case Token::Type::Symbol:
		return Value::symbol(std::string(t.source));

	case Token::Type::Keyword:
		if (t.source == "false") return Value::boolean(false);
		if (t.source == "nil")   return Value{};
		if (t.source == "true")  return Value::boolean(true);
		unreachable();

	case Token::Type::Chord:
		if (t.source.size() == 1 || (t.source.size() == 2 && t.source.back() == '#')) {
			unimplemented();
		}

		unimplemented("only simple note values (like c or e#) are supported now");

	default:
		unimplemented();
	}
}

Value Value::boolean(bool b)
{
	Value v;
	v.type = Value::Type::Bool;
	v.b = b;
	return v;
}

Value Value::number(Number n)
{
	Value v;
	v.type = Type::Number;
	v.n = std::move(n).simplify();
	return v;
}

Value Value::symbol(std::string s)
{
	Value v;
	v.type = Type::Symbol;
	v.s = std::move(s);
	return v;
}

Value Value::block(Block &&block)
{
	Value v;
	v.type = Type::Block;
	v.blk = std::move(block);
	return v;
}

Result<Value> Value::operator()(Interpreter &i, std::vector<Value> args)
{
	switch (type) {
	case Type::Intrinsic: return intr(i, std::move(args));
	case Type::Block:     return blk(i, std::move(args));
	default:
		// TODO Fill location
		return errors::not_callable(std::nullopt, type);
	}
}

bool Value::truthy() const
{
	switch (type) {
	case Type::Bool:   return b;
	case Type::Nil:    return false;
	case Type::Number: return n != Number(0);
	case Type::Block:
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
	case Type::Music:     unimplemented();
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

	case Value::Type::Music:
		unimplemented();
	}
	unreachable();
}

std::string_view type_name(Value::Type t)
{
	switch (t) {
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
