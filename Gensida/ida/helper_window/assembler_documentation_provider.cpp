#include "assembler_documentation_provider.h"

#include <fstream>
#include <dbg.hpp>
#include <iomanip>
#include <sys/stat.h>

namespace
{
	const char* mnemonics_data_file_name = "mnemonics.json";
	const std::string base_instructions_url("https://raw.githubusercontent.com/prb28/m68k-instructions-documentation/master/instructions/");
}

void assembler_documentation_provider::init()
{
	std::ifstream mnemonics_data_file(mnemonics_data_file_name);
	if (!mnemonics_data_file.is_open())
	{
		return;
	}
	Json::Reader reader;
	mnemonics_data = std::make_unique<Json::Value>();
	if (reader.parse(mnemonics_data_file, *mnemonics_data))
	{
		msg("Book: %s", (*mnemonics_data)["book"].asString());
		msg("Year: %i", (*mnemonics_data)["year"].asUInt());
	}
	else
	{
		msg("Error: %s", reader.getFormattedErrorMessages());
	}

	cs_open(CS_ARCH_M68K, CS_MODE_M68K_000, &cs_handle);
	cs_option(cs_handle, CS_OPT_DETAIL, CS_OPT_ON);
}

void assembler_documentation_provider::shutdown()
{
	cs_close(&cs_handle);

	save_mnemonics_data();
}

std::string assembler_documentation_provider::get_mnemonic_description(const insn_t& in_instruction) const
{
	std::vector<std::uint8_t> instruction_data(in_instruction.size);
	get_bytes(instruction_data.data(), in_instruction.size, in_instruction.ea);

	cs_insn* insn;
	const size_t code_size = cs_disasm(cs_handle, instruction_data.data(), instruction_data.size(), in_instruction.ea, instruction_data.size(), &insn);

	std::stringstream ss;
	ss << insn->mnemonic << "\t\t" << insn->op_str << " // insn-mnem: " << cs_insn_name(cs_handle, insn->id) << std::endl;
	if (insn->detail->regs_read_count > 0)
	{
		ss << "\tImplicit registers read: ";
		for (int i = 0; i < insn->detail->regs_read_count; ++i)
		{
			ss << cs_reg_name(cs_handle, insn->detail->regs_read[i]);
		}
		ss << std::endl;
	}
	if (insn->detail->regs_write_count > 0)
	{
		ss << "\tImplicit registers write: ";
		for (int i = 0; i < insn->detail->regs_write_count; ++i)
		{
			ss << cs_reg_name(cs_handle, insn->detail->regs_write[i]);
		}
		ss << std::endl;
	}
	if (insn->detail->groups_count > 0)
	{
		ss << "\tThis instruction belongs to groups: ";
		for (int i = 0; i < insn->detail->groups_count; ++i)
		{
			ss << static_cast<unsigned>(insn->detail->groups[i]);
		}
		ss << std::endl;
	}

	if (insn->detail->m68k.op_count)
	{
		ss << std::endl;
		ss << "\tNumber of operands: " << static_cast<unsigned>(insn->detail->m68k.op_count) << std::endl;
	}

	for (int i = 0; i < insn->detail->m68k.op_count; ++i)
	{
		cs_m68k_op* op = &(insn->detail->m68k.operands[i]);

		switch (op->type)
		{
			case M68K_OP_REG:
			{
				ss << "operands[" << i << "].type: REG = " << cs_reg_name(cs_handle, op->reg) << std::endl;
				break;
			}
			case M68K_OP_IMM:
			{
				ss << "operands[" << i << "].type: IMM  = " << op->imm << std::endl;
				break;
			}
			case M68K_OP_MEM:
			{
				ss << "operands[" << i << "].type: MEM" << std::endl;
				break;
			}
		}
	}

	cs_free(insn, code_size);

	return ss.str();
}

assembler_documentation_provider::loading_e assembler_documentation_provider::try_to_get_instruction_description(const ea_t in_ea, const char*& out_data, size_t& out_data_size)
{
	if (!is_mapped(in_ea))
	{
		return loading_e::none;
	}

	const asize_t instruction_size = get_item_size(in_ea);

	std::vector<std::uint8_t> instruction_data(instruction_size);
	get_bytes(instruction_data.data(), instruction_size, in_ea);

	cs_insn* insn;
	const size_t code_size = cs_disasm(cs_handle, instruction_data.data(), instruction_data.size(), in_ea, instruction_data.size(), &insn);
	if (code_size == 0)
	{
		return loading_e::none;
	}

	const uint32 mnemonic_id = insn->id;
	const std::string mnemonic_name = cs_insn_name(cs_handle, mnemonic_id);

	cs_free(insn, code_size);

	const auto& loading_state_ptr = instructions_states.find(mnemonic_id);
	if (loading_state_ptr == instructions_states.cend())
	{
		cpr::Url url{ base_instructions_url + mnemonic_name + ".md"};

		instructions_states.insert_or_assign(mnemonic_id, loading_e::loading);
		instructions_async_states.insert_or_assign(mnemonic_id, cpr::GetAsync(url));

		return loading_e::loading;
	}

	if (loading_state_ptr->second == loading_e::loading)
	{
		const auto response = instructions_async_states.find(mnemonic_id)->second.get();
		if (response.error.code == cpr::ErrorCode::OK)
		{
			instructions_data.insert_or_assign(mnemonic_id, response.text);
			instructions_states[mnemonic_id] = loading_e::success;
			return loading_e::loading;
		}
	}

	if (loading_state_ptr->second == loading_e::success)
	{
		out_data = instructions_data.find(mnemonic_id)->second.c_str();
		out_data_size = instructions_data.find(mnemonic_id)->second.size();
	}

	return loading_state_ptr->second;
}

std::stringstream assembler_documentation_provider::get_function_description(const ea_t in_ea)
{
	std::stringstream ss;

	const func_t* func_ptr = get_func(in_ea);
	if (func_ptr == nullptr)
	{
		ss << "Not a function" << std::endl;
		return ss;
	}

	qstring func_name;
	get_func_name(&func_name, in_ea);

	ss << func_name.c_str() << std::endl;

	std::set<ea_t> processed_operations;
	for (ea_t current_operation = func_ptr->start_ea; current_operation < func_ptr->end_ea; current_operation++)
	{
		if (processed_operations.find(current_operation) != processed_operations.cend())
		{
			continue;
		}

		insn_t out_insn;
		decode_insn(&out_insn, current_operation);

		const std::string& out_operation_text = parse_operation(out_insn, processed_operations);



		processed_operations.insert(current_operation);
	}

	return ss;
}

void assembler_documentation_provider::save_mnemonics_data() const
{
	if (!mnemonics_data)
	{
		return;
	}

	struct stat file_stat{};
	if (stat(mnemonics_data_file_name, &file_stat) != -1 && !mnemonics_data_changed)
	{
		return;
	}

	std::ofstream out_file(mnemonics_data_file_name, std::ios::out);
	out_file << *mnemonics_data;
}

std::string assembler_documentation_provider::parse_operation(const insn_t& insn, std::set<unsigned>& processed_insn)
{
	return "";
}
