# define IMGUI_DEFINE_MATH_OPERATORS

#include "ida_gens_helper_window.h"

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

void ida_gens_helper_window::draw_current_node()
{
    ImGui::Begin("Content");

    auto& io = ImGui::GetIO();

    ImGui::Text("FPS: %.2f (%.2gms)", io.Framerate, io.Framerate ? 1000.0f / io.Framerate : 0.0f);

    ImGui::Separator();

    ax::NodeEditor::SetCurrentEditor(editor_context);
    ax::NodeEditor::Begin("My Editor", ImVec2(0.0, 0.0f));
    int uniqueId = 1;
    // Start drawing nodes.
    ax::NodeEditor::BeginNode(uniqueId++);
    ImGui::Text("Node A");
    ax::NodeEditor::BeginPin(uniqueId++, ax::NodeEditor::PinKind::Input);
    ImGui::Text("-> In");
    ax::NodeEditor::EndPin();
    ImGui::SameLine();
    ax::NodeEditor::BeginPin(uniqueId++, ax::NodeEditor::PinKind::Output);
    ImGui::Text("Out ->");
    ax::NodeEditor::EndPin();
    ax::NodeEditor::EndNode();
    ax::NodeEditor::End();
    ax::NodeEditor::SetCurrentEditor(nullptr);

    ImGui::End();
}

void ida_gens_helper_window::draw_layout()
{
    if (ImGui::Begin("Example: Simple layout", nullptr, ImGuiWindowFlags_MenuBar))
    {
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Close", "Ctrl+W"))
                {
	                
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        auto& io = ImGui::GetIO();

        ImGui::Text("FPS: %.2f (%.2gms)", io.Framerate, io.Framerate ? 1000.0f / io.Framerate : 0.0f);

        ImGui::Separator();

        draw_current_command();
    }
    ImGui::End();
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

    ax::NodeEditor::Config config;
    config.SettingsFile = "Simple.json";
    editor_context = ax::NodeEditor::CreateEditor(&config);

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

    ax::NodeEditor::DestroyEditor(editor_context);

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

    // SDL_RenderSetScale(renderer, 1.5f, 1.5f);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    // ImGui::GetFont()->Font

    // SDL_SetTextureScaleMode(bd->FontTexture, SDL_ScaleModeNearest);

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

    const char* markdown_data = nullptr;
    size_t markdown_data_size = 0;
    const assembler_documentation_provider::loading_e state = documentation_provider.try_to_get_instruction_description(ea, markdown_data, markdown_data_size);

    // ImGui::Begin("Current Operation");
    if (state == assembler_documentation_provider::loading_e::success && markdown_data_size > 0)
    {
        assembler_markdown.draw(markdown_data, markdown_data_size);
    }
    else
    {
        ImGui::Text("Loading...");
    }
    // ImGui::End();

    //const std::string& func_text = documentation_provider.get_function_description(ea).str();
    //function_editor.SetText(func_text);

    //function_editor.Render("Current Function");

    ImGui::GetFont()->Scale = previous_font_scale;
    ImGui::PopFont();
}

#endif