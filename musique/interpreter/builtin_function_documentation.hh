#ifndef MUSIQUE_BUILTIN_FUNCTION_DOCUMENTATION_HH
#define MUSIQUE_BUILTIN_FUNCTION_DOCUMENTATION_HH

#include <optional>
#include <string_view>

std::optional<std::string_view> find_documentation_for_builtin(std::string_view builtin_name);

#endif
