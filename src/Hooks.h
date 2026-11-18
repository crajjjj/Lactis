#pragma once

namespace Lactis::Hooks
{
	// Registers the frame driver: an input-event sink that queues one main-thread
	// task per frame, which runs the milk update. Call at kInputLoaded.
	void Install();

	// Drops all milk and every cached body (load, new game, main menu).
	void ResetSession();
}
