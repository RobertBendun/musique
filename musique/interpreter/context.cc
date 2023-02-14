#include <musique/interpreter/context.hh>

Note Context::fill(Note note) const
{
	if (not note.octave) note.octave = octave;
	if (not note.length) note.length = length;
	return note;
}

std::chrono::duration<float> Context::length_to_duration(std::optional<Number> length) const
{
	auto const len = length ? *length : this->length;
	return std::chrono::duration<float>(float(len.num * (60.f / (float(bpm) / 4))) / len.den);
}

template<>
struct std::hash<midi::Port>
{
	std::size_t operator()(midi::Port const& value) const
	{
		using namespace midi;
		return hash_combine(value.index(), std::visit(Overloaded {
			[](Virtual_Port) { return 0u; },
			[](Established_Port port) { return port; },
		}, value));
	}
};

std::unordered_map<midi::Port, std::shared_ptr<midi::Connection>> Context::established_connections;

/// Establish connection to given port
std::optional<Error> Context::connect(std::optional<midi::Port> desired_port)
{
	// FIXME This function doesn't support creating virtual ports when established ports are available
	if (!desired_port) {
		if (not established_connections.empty()) {
			port = established_connections.begin()->second;
			return std::nullopt;
		}
		auto connection = std::make_shared<midi::Rt_Midi>();
		established_connections[connection->establish_any_connection()] = connection;
		port = connection;
		return std::nullopt;
	}

	if (auto it = established_connections.find(*desired_port); it != established_connections.end()) {
		port = it->second;
		return std::nullopt;
	}

	auto connection = std::make_shared<midi::Rt_Midi>();
	connection->connect(*desired_port);
	established_connections[*desired_port] = connection;
	port = connection;

	return std::nullopt;
}

