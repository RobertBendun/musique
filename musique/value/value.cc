#include <musique/accessors.hh>
#include <musique/guard.hh>
#include <musique/interpreter/env.hh>
#include <musique/interpreter/interpreter.hh>
#include <musique/try.hh>
#include <musique/value/value.hh>

#include <iostream>
#include <numeric>

Value::Value() = default;

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
	v.data = b.value;
	return v;
}

Value Value::from(Number n)
{
	Value v;
	v.data = std::move(n).simplify();
	return v;
}

Value Value::from(std::string s)
{
	Value v;
	v.data = Symbol(std::move(s));
	return v;
}

Value Value::from(std::string_view s)
{
	Value v;
	v.data = Symbol(std::move(s));
	return v;
}

Value Value::from(char const* s)
{
	Value v;
	v.data = Symbol(s);
	return v;
}

Value Value::from(Block &&block)
{
	Value v;
	v.data = std::move(block);
	return v;
}

Value Value::from(Array &&array)
{
	Value v;
	v.data = std::move(array);
	return v;
}

Value Value::from(std::vector<Value> &&array)
{
	Value v;
	v.data = Array(std::move(array));
	return v;
}

Value Value::from(Note n)
{
	Value v;
	v.data = Chord(n);
	return v;
}

Value Value::from(Chord chord)
{
	Value v;
	v.data = std::move(chord);
	return v;
}

Result<Value> Value::operator()(Interpreter &i, std::vector<Value> args) const
{
	if (auto func = get_if<Function>(data)) {
		return (*func)(i, std::move(args));
	}

	return errors::Not_Callable { .type = type_name(*this) };
}


Result<Value> Value::index(Interpreter &i, unsigned position) const
{
	if (auto collection = get_if<Collection>(data)) {
		return collection->index(i, position);
	}
	assert(false, "Block indexing is not supported for this type"); // TODO(assert)
	unreachable();
}

bool Value::truthy() const
{
	return std::visit(Overloaded {
		[](Bool b)          { return b; },
		[](Nil)             { return false; },
		[](Number const& n) { return n != Number(0); },
		[](Array const& a)  { return a.size() != 0; },
		[](Block const& b)  { return b.size() != 0; },
		[](auto&&)          { return true; }
	}, data);
}

bool Value::falsy() const
{
	return not truthy();
}

bool Value::operator==(Value const& other) const
{
	return std::visit(Overloaded {
		[]<typename T>(T const& lhs, T const& rhs) -> bool requires (!std::is_same_v<T, Block>) {
			return lhs == rhs;
		},
		[](auto&&...) { return false; }
	}, data, other.data);
}

usize Value::size() const
{
	if (auto collection = get_if<Collection>(data)) {
		return collection->size();
	}

	assert(false, "This type does not support Value::size()"); // TODO(assert)
	unreachable();
}

std::partial_ordering Value::operator<=>(Value const& rhs) const
{
	// TODO Block - array comparison should be allowed
	return std::visit(Overloaded {
		[](Nil, Nil) { return std::partial_ordering::equivalent; },
		[](Array const& lhs, Array const& rhs) {
			return std::lexicographical_compare_three_way(
				lhs.elements.begin(), lhs.elements.end(),
				rhs.elements.begin(), rhs.elements.end()
			);
		},
		[](Chord const& lhs, Chord const& rhs) {
			return std::lexicographical_compare_three_way(
				lhs.notes.begin(), lhs.notes.end(),
				rhs.notes.begin(), rhs.notes.end()
			);
		},
		[]<typename T>(T const& lhs, T const& rhs) -> std::partial_ordering requires std::three_way_comparable<T> {
			return lhs <=> rhs;
		},
		[](auto&&...) { return std::partial_ordering::unordered; }
	}, data, rhs.data);
}

std::ostream& operator<<(std::ostream& os, Value const& v)
{
	std::visit(Overloaded {
		[&](Bool b) { os << std::boolalpha << b; },
		[&](Nil)    { os << "nil"; },
		[&](Intrinsic) { os << "<intrinisic>"; },
		[&](Block const&) { os << "<block>"; },
		[&](auto const& s) { os << s; }
	}, v.data);
	return os;
}


std::string_view type_name(Value const& v)
{
	return std::visit(Overloaded {
		[&](Array const&)     { return "array"; },
		[&](Block const&)     { return "block"; },
		[&](Bool const&)      { return "bool"; },
		[&](Chord const&)     { return "music"; },
		[&](Intrinsic const&) { return "intrinsic"; },
		[&](Nil const&)       { return "nil"; },
		[&](Number const&)    { return "number"; },
		[&](Symbol const&)    { return "symbol"; },
	}, v.data);
}

Result<std::vector<Value>> flatten(Interpreter &interpreter, std::span<Value> args)
{
	std::vector<Value> result;
	for (auto &x : args) {
		if (auto collection = get_if<Collection>(x)) {
			for (usize i = 0; i < collection->size(); ++i) {
				result.push_back(Try(collection->index(interpreter, i)));
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
	auto const value_hash = std::visit(Overloaded {
		[](Nil) { return std::size_t(0); },
		[](Intrinsic i) { return size_t(i.function_pointer); },
		[](Block const& b) { return hash_combine(std::hash<Ast>{}(b.body), b.parameters.size()); },
		[this](Array const& array) {
			return std::accumulate(
				array.elements.begin(), array.elements.end(), size_t(0),
				[this](size_t h, Value const& v) { return hash_combine(h, operator()(v)); }
			);
		},
		[](Chord const& chord) {
			return std::accumulate(chord.notes.begin(), chord.notes.end(), size_t(0), [](size_t h, Note const& n) {
				h = hash_combine(h, std::hash<std::optional<int>>{}(n.base));
				h = hash_combine(h, std::hash<std::optional<Number>>{}(n.length));
				h = hash_combine(h, std::hash<std::optional<i8>>{}(n.octave));
				return h;
			});
		},
		[]<typename T>(T const& t) { return std::hash<T>{}(t); },
	}, value.data);

	return hash_combine(value_hash, size_t(value.data.index()));
}
