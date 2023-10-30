#include "ida_gens_helper_window.h"

#include <bytes.hpp>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>
#include <pro.h>
#include <kernwin.hpp>
#include <ua.hpp>

#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_video.h>

void ida_gens_helper_window::init()
{
	gens_helper_thread = std::thread(&ida_gens_helper_window::process, this);
}

void ida_gens_helper_window::shutdown()
{
	if (!gens_helper_thread.joinable())
	{
		return;
	}

	should_stop_visualization = true;
	gens_helper_thread.join();
}

void ida_gens_helper_window::process()
{
    constexpr std::uint32_t _60fps = 1000u / 60u;

    const bool sdl_is_activated = init_sdl();
    if (!sdl_is_activated)
    {
	    return;
    }

    while (!should_stop_visualization)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                should_stop_visualization = true;
            }

            ImGui_ImplSDL2_ProcessEvent(&event);
        }

        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        draw_current_command();

        ImGui::Render();

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());

        SDL_RenderPresent(renderer);

        SDL_Delay(_60fps);
    }

    shutdown_sdl();
}

bool ida_gens_helper_window::init_sdl()
{
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
    {
        return false;
    }

    window = SDL_CreateWindow("Hello World!", 100, 100, 620, 387, SDL_WINDOW_SHOWN);
    if (window == nullptr)
    {
        return false;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == nullptr)
    {
        if (window != nullptr)
        {
            SDL_DestroyWindow(window);
        }
        SDL_Quit();
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    return true;
}

void ida_gens_helper_window::shutdown_sdl() const
{
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void ida_gens_helper_window::draw_current_command()
{
    const ea_t ea = get_screen_ea();
    if (!is_mapped(ea))
    {
        return;
    }

    qstring mnemonic;
    print_insn_mnem(&mnemonic, ea);
    if (mnemonic.empty())
    {
        return;
    }

    qvector<qstring> mnemonics;
    constexpr char point_separate[] = ".";
    mnemonic.split(&mnemonics, point_separate);
    if (mnemonics.empty())
    {
        return;
    }

    qstring target_mnemonic;
    target_mnemonic.swap(mnemonics[0]);

    ImGui::Text(target_mnemonic.c_str());
    if (mnemonics.size() > 1)
    {
        ImGui::SameLine();
        ImGui::Text(mnemonics[1].c_str());
    }
}
