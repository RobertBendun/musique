#ifndef MUSIQUE_BUILTIN_FUNCTION_DOCUMENTATION_HH
#define MUSIQUE_BUILTIN_FUNCTION_DOCUMENTATION_HH

#include <optional>
#include <string_view>
#include <vector>

std::optional<std::string_view> find_documentation_for_builtin(std::string_view builtin_name);


/// Returns top 4 similar names to required
std::vector<std::string_view> similar_names_to_builtin(std::string_view builtin_name);

#endif
