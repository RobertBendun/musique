#include <musique/accessors.hh>
#include <musique/algo.hh>
#include <musique/guard.hh>
#include <musique/interpreter/env.hh>
#include <musique/interpreter/interpreter.hh>
#include <musique/try.hh>
#include <musique/value/value.hh>

#include <iostream>
#include <numeric>
#include <compare>
#include <cstring>

Value::Value() = default;

std::strong_ordering operator<=>(std::string const& lhs, std::string const& rhs)
{
	if (auto cmp = lhs.size() <=> rhs.size(); cmp == 0) {
		if (auto cmp = std::strncmp(lhs.c_str(), rhs.c_str(), lhs.size()); cmp == 0) {
			return std::strong_ordering::equal;
		} else if (cmp < 0) {
			return std::strong_ordering::less;
		} else {
			return std::strong_ordering::greater;
		}
	} else {
		return cmp;
	}
}

Result<Value> Value::from(Token t)
{
	switch (t.type) {
	case Token::Type::Numeric:
		return Try(Number::from(std::move(t)));

	case Token::Type::Symbol:
		return t.source;

	case Token::Type::Keyword:
		if (t.source == "false") return false;
		if (t.source == "nil")   return {};
		if (t.source == "true")  return true;
		unreachable();

	case Token::Type::Chord:
		if (t.source.size() == 1 || (t.source.size() == 2 && t.source.back() == '#')) {
			auto maybe_note = Note::from(t.source);
			ensure(maybe_note.has_value(), "Somehow parser passed invalid note literal");
			return *maybe_note;
		}

		return Chord::from(t.source);

	default:
		unreachable();
	}
}

Value::Value(Explicit_Bool b)
	: data{b.value}
{
}

Value::Value(Number n)
	: data(n.simplify())
{
}

Value::Value(std::string s)
	: data(Symbol(std::move(s)))
{
}

Value::Value(std::string_view s)
	: data(Symbol(s))
{
}

Value::Value(char const* s)
	: data(Symbol(s))
{
}

Value::Value(Block &&block)
	: data{std::move(block)}
{
}

Value::Value(Array &&array)
	: data{std::move(array)}
{
}

Value::Value(std::vector<Value> &&array)
	: data(Array(std::move(array)))
{
}

Value::Value(Note n)
	: data(Chord(n))
{
}

Value::Value(Chord chord)
	: data(std::move(chord))
{
}

Value::Value(Set &&set)
	: data(std::move(set))
{
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
	ensure(false, "Block indexing is not supported for this type"); // TODO(assert)
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

	ensure(false, "This type does not support Value::size()"); // TODO(assert)
	unreachable();
}

std::partial_ordering Value::operator<=>(Value const& rhs) const
{
	// TODO Block - array comparison should be allowed
	return std::visit(Overloaded {
		[](Nil, Nil) { return std::partial_ordering::equivalent; },
		[](Array const& lhs, Array const& rhs) {
			return algo::lexicographical_compare(lhs.elements, rhs.elements);
		},
		[](Chord const& lhs, Chord const& rhs) {
			return algo::lexicographical_compare(lhs.notes, rhs.notes);
		},
		[]<typename T>(T const& lhs, T const& rhs) -> std::partial_ordering requires Three_Way_Comparable<T> {
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
		[&](auto const& s) {
			static_assert(requires { os << s; });
			os << s;
		}
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
		[&](Set const&)       { return "set"; },
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

