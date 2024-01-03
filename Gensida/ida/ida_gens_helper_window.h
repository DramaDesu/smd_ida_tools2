#pragma once

#ifdef SUPPORT_VISUALIZATION

#include <atomic>
#include <mutex>

#include "helper_window/assembler_documentation_provider.h"
#include "helper_window/assembler_markdown.h"
#include "helper_window/text_editor.h"

struct ida_gens_helper_window
{
	void init();
	void shutdown();

private:
	static void make_font_pixel_perfect();
	void process();

    bool init_sdl();
	void shutdown_sdl() const;

	void draw_layout();
    void draw_current_command();

	void try_to_open_tutorial(const std::string& in_mnemonic_name) const;

	std::thread gens_helper_thread;

	std::mutex visualization_process_mutex;

	std::atomic_bool should_stop_visualization;
	std::atomic_bool visualization_is_stopped;

	std::condition_variable visualization_is_stopped_cv;

	struct SDL_Window* window = nullptr;
	struct SDL_Renderer* renderer = nullptr;

	TextEditor function_editor;

	ui::assembler_markdown assembler_markdown;

	assembler_documentation_provider documentation_provider;
};

#endif