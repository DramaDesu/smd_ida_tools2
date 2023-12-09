#pragma once

#include "imgui_markdown.h"

namespace ui
{
	struct assembler_markdown
	{
		assembler_markdown();

		void draw(const char* in_data, size_t in_data_size) const;

	private:
		ImGui::MarkdownConfig md_config;
	};
}