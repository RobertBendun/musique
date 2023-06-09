#ifndef MUSIQUE_AST_HH
#define MUSIQUE_AST_HH

#include <musique/lexer/token.hh>
#include <vector>
#include <optional>

/// Representation of a node in program tree
struct Ast
{
	/// Constructs binary operator
	static Ast binary(std::string_view filepath, Token, Ast lhs, Ast rhs);

	/// Constructs block
	static Ast block(File_Range file, Ast seq = sequence({}));

	/// Constructs call expression
	static Ast call(std::vector<Ast> call);

	/// Constructs block with parameters
	static Ast lambda(File_Range file, Ast seq = sequence({}), std::vector<Ast> parameters = {});

	/// Constructs constants, literals and variable identifiers
	static Ast literal(std::string_view filepath, Token);

	/// Constructs sequence of operations
	static Ast sequence(std::vector<Ast> call);

	/// Constructs variable declaration
	static Ast variable_declaration(File_Range loc, std::vector<Ast> lvalues, std::optional<Ast> rvalue);

	/// Available ASt types
	enum class Type
	{
		Unary,                ///< Unary operator like -foo or +xd
		Binary,               ///< Binary operator application like `1 + 2`
		Lambda,               ///< Block expression beeing functions like `(i|i+1)`
		Call,                 ///< Function call application like `print 42`
		Literal,              ///< Compile time known constant like `c` or `1`
		Sequence,             ///< Several expressions sequences like `42`, `42; 32`
		Variable_Declaration, ///< Declaration of a variable with optional value assigment like `var x = 10` or `var y`
		If,                   ///< if_then_else expression
	};

	/// Type of AST node
	Type type;

	/// Location where node was created
	File_Range file;

	/// Associated token
	Token token;

	/// Child nodes
	std::vector<Ast> arguments{};
};

bool operator==(Ast const& lhs, Ast const& rhs);
std::ostream& operator<<(std::ostream& os, Ast::Type type);
std::ostream& operator<<(std::ostream& os, Ast const& tree);

/// Pretty print program tree for debugging purposes
void dump(Ast const& ast, unsigned indent = 0);

template<> struct std::hash<Ast>    { std::size_t operator()(Ast    const&) const; };

#endif
