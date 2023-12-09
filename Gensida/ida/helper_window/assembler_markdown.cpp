#include "assembler_markdown.h"

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
	}
}
