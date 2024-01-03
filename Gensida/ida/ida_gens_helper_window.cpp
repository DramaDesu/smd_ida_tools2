# define IMGUI_DEFINE_MATH_OPERATORS

#include "ida_gens_helper_window.h"

#include "helper_window/assembler_documentation_provider.h"
#include "helper_window/assembler_documentation_provider.h"

#ifdef SUPPORT_VISUALIZATION

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

#include <windows.h>
#include <shellapi.h>

#include "helper_window/text_editor.h"

#include <imgui-node-editor/imgui_canvas.h>
#include <imgui-node-editor/imgui_node_editor.h>

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

void ida_gens_helper_window::draw_layout()
{
    draw_current_command();
}

struct ImGui_ImplSDLRenderer2_Data
{
    SDL_Renderer* SDLRenderer = nullptr;
    SDL_Texture* FontTexture = nullptr;
};

void ida_gens_helper_window::make_font_pixel_perfect()
{
	const ImGui_ImplSDLRenderer2_Data* bd = ImGui::GetCurrentContext() ? static_cast<ImGui_ImplSDLRenderer2_Data*>(ImGui::GetIO().BackendRendererUserData) : nullptr;
	if (bd != nullptr)
	{
		SDL_SetTextureScaleMode(bd->FontTexture, SDL_ScaleModeNearest);
	}
}

void ida_gens_helper_window::process()
{
    constexpr std::uint32_t _60fps = 1000u / 60u;

    const bool sdl_is_activated = init_sdl();
    if (!sdl_is_activated)
    {
	    return;
    }

    make_font_pixel_perfect();

    documentation_provider.init();

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

        make_font_pixel_perfect();

        draw_layout();

        ImGui::Render();

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());

        SDL_RenderPresent(renderer);

        SDL_Delay(_60fps);
    }

    documentation_provider.shutdown();

    shutdown_sdl();
}

bool ida_gens_helper_window::init_sdl()
{
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
    {
        return false;
    }

    window = SDL_CreateWindow("Ida Helper", 100, 100, 1920, 1080, SDL_WINDOW_SHOWN);
    // window = SDL_CreateWindow("Ida Helper", 100, 100, 1920, 1080, SDL_WINDOW_SHOWN | SDL_WINDOW_ALWAYS_ON_TOP);
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
    const float previous_font_scale = ImGui::GetFont()->Scale;
    ImGui::GetFont()->Scale *= 2.0f;
    ImGui::PushFont(ImGui::GetFont());

    const ea_t ea = get_screen_ea();

    assembler_documentation_provider::instruction_description_t out_description;
    const assembler_documentation_provider::loading_e state = documentation_provider.try_to_get_instruction_description(ea, out_description);

    ImGui::Begin("Current Operation");
    {
    	auto& io = ImGui::GetIO();
	    ImGui::Text("FPS: %.2f (%.2gms)", io.Framerate, io.Framerate ? 1000.0f / io.Framerate : 0.0f);

        if (!out_description.mnemonic_name.empty() && ImGui::Button(out_description.mnemonic_name.c_str()))
        {
            try_to_open_tutorial(out_description.mnemonic_name);
        }

        if (ImGui::BeginPopupModal("no_tutorial_popup")) 
        {
            ImGui::Text("No tutorial found for %s", out_description.mnemonic_name.c_str());
            if (ImGui::Button("Okay") || ImGui::IsKeyPressed(ImGuiKey_Enter))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if (state == assembler_documentation_provider::loading_e::success && out_description.is_valid_data())
        {
	        assembler_markdown.draw(out_description.data, out_description.size);
        }
        else
        {
	        ImGui::Text("Loading...");
        }
    }
    ImGui::End();

    ImGui::GetFont()->Scale = previous_font_scale;
    ImGui::PopFont();
}

void ida_gens_helper_window::try_to_open_tutorial(const std::string& in_mnemonic_name) const
{
	static const std::multimap<std::string, std::string> tutorials = {
        { "move", "https://mrjester.hapisan.com/04_MC68/Sect01Part05/Index.html" },

        { "add", "https://mrjester.hapisan.com/04_MC68/Sect02Part01/Index.html" },
        { "sub", "https://mrjester.hapisan.com/04_MC68/Sect02Part02/Index.html" },
        { "swap", "https://mrjester.hapisan.com/04_MC68/Sect02Part03/Index.html" },
        { "exg", "https://mrjester.hapisan.com/04_MC68/Sect02Part04/Index.html" },
        { "clr", "https://mrjester.hapisan.com/04_MC68/Sect02Part05/Index.html" },

        { "not", "https://mrjester.hapisan.com/04_MC68/Sect03Part01/Index.html" },
        { "and", "https://mrjester.hapisan.com/04_MC68/Sect03Part02/Index.html" },
        { "or", "https://mrjester.hapisan.com/04_MC68/Sect03Part03/Index.html" },
        { "eor", "https://mrjester.hapisan.com/04_MC68/Sect03Part04/Index.html" },
        { "eori", "https://mrjester.hapisan.com/04_MC68/Sect03Part04/Index.html" },
        { "bset", "https://mrjester.hapisan.com/04_MC68/Sect03Part05/Index.html" },
        { "bclr", "https://mrjester.hapisan.com/04_MC68/Sect03Part05/Index.html" },
        { "bchg", "https://mrjester.hapisan.com/04_MC68/Sect03Part05/Index.html" },

        { "neg", "https://mrjester.hapisan.com/04_MC68/Sect04Part03/Index.html" },
        { "ext", "https://mrjester.hapisan.com/04_MC68/Sect04Part04/Index.html" },
        { "lsl", "https://mrjester.hapisan.com/04_MC68/Sect04Part06/Index.html" },
        { "asl", "https://mrjester.hapisan.com/04_MC68/Sect04Part06/Index.html" },
        { "asr", "https://mrjester.hapisan.com/04_MC68/Sect04Part06/Index.html" },
        { "rol", "https://mrjester.hapisan.com/04_MC68/Sect04Part07/Index.html" },
        { "ror", "https://mrjester.hapisan.com/04_MC68/Sect04Part07/Index.html" },
        { "mulu", "https://mrjester.hapisan.com/04_MC68/Sect04Part08/Index.html" },
        { "muls", "https://mrjester.hapisan.com/04_MC68/Sect04Part08/Index.html" },
        { "divu", "https://mrjester.hapisan.com/04_MC68/Sect04Part09/Index.html" },
        { "divs", "https://mrjester.hapisan.com/04_MC68/Sect04Part09/Index.html" },

        { "jmp", "https://mrjester.hapisan.com/04_MC68/Sect05Part02/Index.html" },
        { "bra", "https://mrjester.hapisan.com/04_MC68/Sect05Part03/Index.html" },
        { "jsr", "https://mrjester.hapisan.com/04_MC68/Sect05Part05/Index.html" },
        { "rts", "https://mrjester.hapisan.com/04_MC68/Sect05Part05/Index.html" },
        { "bsr", "https://mrjester.hapisan.com/04_MC68/Sect05Part06/Index.html" },

        { "cmp", "https://mrjester.hapisan.com/04_MC68/Sect06Part01/Index.html" },
        { "addi", "https://mrjester.hapisan.com/04_MC68/Sect06Part01/Index.html" },
        { "subi", "https://mrjester.hapisan.com/04_MC68/Sect06Part01/Index.html" },
        { "cmp", "https://mrjester.hapisan.com/04_MC68/Sect06Part02/Index.html" },
        { "tst", "https://mrjester.hapisan.com/04_MC68/Sect06Part02/Index.html" },
        { "cmpi", "https://mrjester.hapisan.com/04_MC68/Sect06Part02/Index.html" },
        { "btst", "https://mrjester.hapisan.com/04_MC68/Sect06Part02/Index.html" },

        { "beq", "https://mrjester.hapisan.com/04_MC68/Sect06Part01/Index.html" },
        { "beq", "https://mrjester.hapisan.com/04_MC68/Sect06Part02/Index.html" },
	};

    bool found_any = false;

    for (auto it = tutorials.lower_bound(in_mnemonic_name); it != tutorials.upper_bound(in_mnemonic_name); ++it)
    {
        ShellExecute(0, 0, it->second.c_str(), 0, 0, SW_SHOW);
        found_any = true;
    }

    if (!found_any)
    {
        ImGui::OpenPopup("no_tutorial_popup");
    }
}

#endif
