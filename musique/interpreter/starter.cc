#include <ableton/Link.hpp>
#include <musique/errors.hh>
#include <musique/interpreter/starter.hh>

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
	std::atomic<bool> stop = false;
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
			state->stop = true;
		}
	});

	std::thread starter([quantum = quantum, state = state]
	{
		auto counter = 0;
		while (!state->stop) {
			auto const time = state->link->clock().micros();
			auto sessionState = state->link->captureAppSessionState();
			if (counter == 2) {
				sessionState.setIsPlaying(true, time);
				state->link->commitAppSessionState(sessionState);
				state->condition.notify_all();
				state->stop = true;
				return;
			}
			auto const phase = sessionState.phaseAtTime(time, quantum);
			counter += phase == 0;
		}
	});
	starter.detach();

	// TODO Rejection via CTRL-C while waiting
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

size_t Starter::peers() const
{
	ensure(impl != nullptr, "Starter wasn't initialized properly");
	auto &link = impl->link;

	return link.numPeers();
}
