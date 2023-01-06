#include <musique/interpreter/starter.hh>
#include <ableton/Link.hpp>
#include <musique/errors.hh>

struct Starter::Implementation
{
	ableton::Link link = {30};

	Implementation()
	{
		link.enable(true);
		link.enableStartStopSync(true);
	}
};

Starter::Starter()
	: impl{std::make_shared<Starter::Implementation>()}
{
}

struct State
{
	ableton::Link *link;
	std::mutex mutex;
	std::condition_variable condition;
};

void Starter::start()
{
	ensure(impl != nullptr, "Starter wasn't initialized properly");
	auto &link = impl->link;

	auto const quantum = 4;

	auto const sessionState = link.captureAppSessionState();

	// If we already started continue
	if (sessionState.isPlaying()) {
		return;
	}

	auto state = std::make_shared<State>();
	state->link = &link;

	link.setStartStopCallback([state = state](bool is_playing)
	{
		if (is_playing) {
			state->condition.notify_all();
		}
	});

	std::jthread starter([quantum = quantum, state = state](std::stop_token token)
	{
		auto counter = 0;
		while (!token.stop_requested()) {
			auto const time = state->link->clock().micros();
			auto sessionState = state->link->captureAppSessionState();
			if (counter == 2) {
				sessionState.setIsPlaying(true, time);
				state->link->commitAppSessionState(sessionState);
				return;
			}
			auto const phase = sessionState.phaseAtTime(time, quantum);
			counter += phase == 0;
		}
	});

	std::unique_lock lock(state->mutex);
	state->condition.wait(lock);
}

void Starter::stop()
{
	ensure(impl != nullptr, "Starter wasn't initialized properly");
	auto &link = impl->link;

	auto const time = link.clock().micros();
	auto sessionState = link.captureAppSessionState();
	sessionState.setIsPlaying(false, time);
	link.commitAppSessionState(sessionState);
}
