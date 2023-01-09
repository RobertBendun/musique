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

std::size_t std::hash<midi::connections::Key>::operator()(midi::connections::Key const& value) const
{
	using namespace midi::connections;
	return hash_combine(value.index(), std::visit(Overloaded {
		[](Virtual_Port) { return 0u; },
		[](Serial_Port) { return 0u; },
		[](Established_Port port) { return port; },
	}, value));
}

std::unordered_map<midi::connections::Key, std::shared_ptr<midi::Connection>> Context::established_connections;

/// Establish connection to given port
std::optional<Error> Context::connect(std::optional<Port_Number> port_number)
{
	// FIXME This function doesn't support creating virtual ports when established ports are available
	using namespace midi::connections;
	auto const key = port_number ? Key(*port_number) : Key(Virtual_Port{});

	if (auto it = established_connections.find(key); it != established_connections.end()) {
		port = it->second;
		return std::nullopt;
	}

	if (port_number) {
		auto connection = std::make_shared<midi::Rt_Midi>();
		connection->connect_output(*port_number);
		established_connections[*port_number] = connection;
		port = connection;
		return std::nullopt;
	}

	auto connection = std::make_shared<midi::Rt_Midi>();
	if (connection->connect_or_create_output()) {
		established_connections[0u] = connection;
	} else {
		established_connections[Virtual_Port{}] = connection;
	}
	port = connection;
	return std::nullopt;
}

