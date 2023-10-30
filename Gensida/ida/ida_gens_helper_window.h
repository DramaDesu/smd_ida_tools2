#pragma once

#include <atomic>
#include <mutex>

struct ida_gens_helper_window
{
	void init();
	void shutdown();

private:
	void process();

    bool init_sdl();
	void shutdown_sdl() const;

    void draw_current_command();

	std::thread gens_helper_thread;

	std::mutex visualization_process_mutex;

	std::atomic_bool should_stop_visualization;
	std::atomic_bool visualization_is_stopped;

	std::condition_variable visualization_is_stopped_cv;

	struct SDL_Window* window = nullptr;
	struct SDL_Renderer* renderer = nullptr;
};

