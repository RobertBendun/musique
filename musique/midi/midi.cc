#include <musique/midi/midi.hh>

#include <concepts>
#include <type_traits>

template<typename T>
concept Enum = std::is_enum_v<T>;

/// Converts enum to underlying type
static constexpr auto u(Enum auto e)
{
	return static_cast<std::underlying_type_t<decltype(e)>>(e);
}

void midi::Connection::send_all_sounds_off(uint8_t channel)
{
	send_controller_change(channel, u(Controller::All_Notes_Off), 0);
}
