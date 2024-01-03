#pragma once

#include <pro.h>
#include <kernwin.hpp>
#include <ua.hpp>
#include <unordered_map>

#include "json/json.h"
#include <capstone/capstone.h>

#include <cpr/cpr.h>

#include "imgui_markdown.h"

struct assembler_documentation_provider
{
	void init();
	void shutdown();

	enum class loading_e
	{
		none,
		loading,
		success
	};

	struct instruction_description_t
	{
		const char* data = nullptr;
		size_t size = 0;

		bool is_valid_data() const
		{
			return data != nullptr && size > 0;
		}

		std::string mnemonic_name;
	};

	std::string get_mnemonic_description(const insn_t& in_instruction) const;

	loading_e try_to_get_instruction_description(const ea_t in_ea, instruction_description_t& out_description);

	std::stringstream get_function_description(const ea_t in_ea);

private:
	void save_mnemonics_data() const;

	std::string parse_operation(const insn_t& insn, std::set<unsigned>& processed_insn);

	std::stringstream mnemonic_description;

	std::unique_ptr<Json::Value> mnemonics_data;
	bool mnemonics_data_changed = false;

	csh cs_handle = -1;

	static ImGui::MarkdownConfig md_config;

	std::unordered_map<uint32, loading_e> instructions_states;
	std::unordered_map<uint32, cpr::AsyncResponse> instructions_async_states;
	std::unordered_map<uint32, std::string> instructions_data;
};

