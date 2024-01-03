#include "assembler_markdown.h"

#include <algorithm>
#include <string>
#include <array>
#include <vector>

using namespace std::string_literals;

inline void assembler_markdown_format_callback(const ImGui::MarkdownFormatInfo& in_markdown_info, bool in_start);

ui::assembler_markdown::assembler_markdown()
{
	md_config.userData = this;
	md_config.formatCallback = assembler_markdown_format_callback;
}

void ui::assembler_markdown::draw(const char* in_data, size_t in_data_size) const
{
	ImGui::Markdown(in_data, in_data_size, md_config);
}

#define DATA_REGISTER(N) ("D" #N)
#define ADDR_REGISTER(N) ("A" #N)

constexpr const char* registers_key_words[] = {
	DATA_REGISTER(0),
	DATA_REGISTER(1),
	DATA_REGISTER(2),
	DATA_REGISTER(3),
	DATA_REGISTER(4),
	DATA_REGISTER(5),
	DATA_REGISTER(6),
	DATA_REGISTER(7),

	ADDR_REGISTER(0),
	ADDR_REGISTER(1),
	ADDR_REGISTER(2),
	ADDR_REGISTER(3),
	ADDR_REGISTER(4),
	ADDR_REGISTER(5),
	ADDR_REGISTER(6),
	ADDR_REGISTER(7),

	"Dn",
	"An",

	"SR",
	"CCR",
	"SP",
	"<EA>",
	"<E>",

	"<REGISTER",
	"LIST>"
};

constexpr char value_key_symbols[] = {
	'#', '$', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};

inline void assembler_markdown_format_callback(const ImGui::MarkdownFormatInfo& in_markdown_info, bool in_start)
{
	switch (in_markdown_info.type)
	{
	case ImGui::MarkdownFormatType::NORMAL_TEXT:
	{
		break;
	}

	case ImGui::MarkdownFormatType::EMPHASIS:
		{
			ImGui::MarkdownHeadingFormat fmt{};
			// default styling for emphasis uses last headingFormats - for your own styling
			// implement EMPHASIS in your formatCallback
			if (in_markdown_info.level == 1)
			{
				// normal emphasis
				if (in_start)
				{
					ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
				}
				else
				{
					ImGui::PopStyleColor();
				}
			}
			else
			{
				// strong emphasis
				fmt = in_markdown_info.config->headingFormats[ImGui::MarkdownConfig::NUMHEADINGS - 1];
				if (in_start)
				{
					if (fmt.font)
					{
						ImGui::PushFont(fmt.font);
					}
				}
				else
				{
					if (fmt.font)
					{
						ImGui::PopFont();
					}
				}
			}
			break;
		}
	case ImGui::MarkdownFormatType::HEADING:
		{
			ImGui::MarkdownHeadingFormat fmt{};

			constexpr float scale_coef = 1.5f;

			const float scale = scale_coef / in_markdown_info.level + 0.5f;

			if (in_markdown_info.level > ImGui::MarkdownConfig::NUMHEADINGS)
			{
				fmt = in_markdown_info.config->headingFormats[ImGui::MarkdownConfig::NUMHEADINGS - 1];
			}
			else
			{
				fmt = in_markdown_info.config->headingFormats[in_markdown_info.level - 1];
			}
			if (in_start)
			{
				if (fmt.font)
				{
					ImGui::PushFont(fmt.font);
				}

				ImGui::GetFont()->Scale *= scale;
				ImGui::PushFont(ImGui::GetFont());

				ImGui::NewLine();
			}
			else
			{
				if (fmt.separator)
				{
					ImGui::Separator();
					ImGui::NewLine();
				}
				else
				{
					ImGui::NewLine();
				}
				if (fmt.font)
				{
					ImGui::PopFont();
				}

				ImGui::GetFont()->Scale /= scale;
				ImGui::PopFont();
			}
			break;
		}
	case ImGui::MarkdownFormatType::UNORDERED_LIST:
		break;
	case ImGui::MarkdownFormatType::LINK:
		if (in_start)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered]);
		}
		else
		{
			ImGui::PopStyleColor();
			if (in_markdown_info.itemHovered)
			{
				ImGui::UnderLine(ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered]);
			}
			else
			{
				ImGui::UnderLine(ImGui::GetStyle().Colors[ImGuiCol_Button]);
			}
		}
		break;

	case ImGui::MarkdownFormatType::CODE:
		{
			if (in_markdown_info.data == nullptr || in_markdown_info.data_len == 0)
			{
				return;
			}

			std::string token(in_markdown_info.data, in_markdown_info.data_len);
			std::transform(token.begin(), token.end(), token.begin(), std::toupper);

			const bool is_register = std::find_if(std::cbegin(registers_key_words), std::cend(registers_key_words), [&](const char* in_word)
			{
				return token.find(in_word) != std::string::npos;
			}) != std::cend(registers_key_words);

			const bool is_value = token.rfind('#', 0) == 0 || token.rfind('$', 0) == 0;

			const bool is_comment = in_markdown_info.is_comment;

			if (!is_comment && !is_register && !is_value)
			{
				return;
			}

			if (!in_start)
			{
				ImGui::PopStyleColor();
				return;
			}

			if (is_comment)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(255.0f / 255.0f, 133.0f / 255.0f, 77.0f / 255.0f, 1.0f));
			}
			else if (is_register)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(93.0f / 255.0f, 173.0f / 255.0f, 173.0f / 255.0f, 1.0f));
			}
			else if (is_value)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(210.0f / 255.0f, 80.0f / 255.0f, 50.0f / 255.0f, 1.0f));
			}

			break;
		}
	}
}
