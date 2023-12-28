#include "resource.h"
#include "gens.h"
#include "save.h"
#include "g_main.h"
#include "ramwatch.h"
#include "debugwindow.h"
#include "g_ddraw.h"
#include "star_68k.h"
#include "vdp_io.h"
#include <vector>
#include <iostream>

#include "m68k_debugwindow.h"

bool handled_ida_event;

void Handle_Gens_Messages();
extern int Gens_Running;
extern "C" int Clear_Sound_Buffer(void);

// const static std::vector<te_variable> M68K_ENVIRONMENT = { {"d0.b", &D0}, {"d1.w", &D1}, {"a0", &A0}, {"addr_val", addr_val, TE_FUNCTION1} };

#define ADD_DATA_REG(num) { "D" ## #num, &main68k_context.dreg[num] }
#define ADD_ADDR_REG(num) { "A" ## #num, &main68k_context.areg[num] }

const static std::vector<te_variable> M68K_ENVIRONMENT = {
    ADD_DATA_REG(0),
    ADD_DATA_REG(1),
    ADD_DATA_REG(2),
    ADD_DATA_REG(3),
    ADD_DATA_REG(4),
    ADD_DATA_REG(5),
    ADD_DATA_REG(6),
    ADD_DATA_REG(7),

    ADD_ADDR_REG(0),
    ADD_ADDR_REG(1),
    ADD_ADDR_REG(2),
    ADD_ADDR_REG(3),
    ADD_ADDR_REG(4),
    ADD_ADDR_REG(5),
    ADD_ADDR_REG(6),
    ADD_ADDR_REG(7)
};

BreakpointCondition::~BreakpointCondition()
{
    if (condition_state)
    {
        te_expr* compiled_expr = condition_state->expr.load();
        condition_state->expr.store(nullptr);
	    te_free(compiled_expr);
    }
}

BreakpointCondition::BreakpointCondition(const BreakpointCondition& other)
{
    if (!other.condition_state)
    {
	    return;
    }

    try_to_compile_condition(other.condition_state->type, other.condition_state->start, other.condition_state->raw_expr);
}

BreakpointCondition& BreakpointCondition::operator=(const BreakpointCondition& other)
{
    if (this == &other)
    {
	    return *this;
    }

    if (other.condition_state)
    {
        try_to_compile_condition(other.condition_state->type, other.condition_state->start, other.condition_state->raw_expr);
    }

    return *this;
}

bool BreakpointCondition::check_condition() const
{
	if (!condition_state || condition_state->expr.load() == nullptr)
	{
		return true;
	}

    return te_logic_eval(condition_state->expr.load()) > 0;
}

void BreakpointCondition::try_to_compile_condition(bp_type in_type, uint32 in_start, const std::string& in_condition)
{
    if (in_condition.empty())
    {
        return;
    }

    if (!condition_state)
    {
        condition_state = std::make_unique<condition_state_t>();
    }

    condition_state->type = in_type;
    condition_state->start = in_start;
    condition_state->raw_expr = in_condition;

    try_to_compile_condition_inner(in_type, in_start, in_condition);
}

void BreakpointCondition::load_compiled_condition(te_expr* in_expr) const
{
    if (!condition_state)
    {
        // Should be fatal or something
        return;
    }

    te_free(condition_state->expr.load());
    condition_state->expr.store(in_expr);
}

void BreakpointCondition::try_to_compile_condition_inner(bp_type in_type, uint32 in_start, const std::string& in_condition)
{
    BreakpointConditionWorker::get().enqueue([in_type, in_start, in_condition]
    {
        te_expr* expr = te_compile(in_condition.c_str(), true, M68K_ENVIRONMENT.data(), M68K_ENVIRONMENT.size(), nullptr);
        if (expr == nullptr)
        {
            // Log or do something
            return;
        }

        for (auto i = M68kDW.Breakpoints.begin(); i != M68kDW.Breakpoints.end(); ++i)
        {
            if (i->type == in_type && i->start == in_start)
            {
                i->load_compiled_condition(expr);
                break;
            }
        }
    });
}

bool Breakpoint::check_condition() const
{
    return breakpoint_condition.check_condition();
}

void Breakpoint::load_compiled_condition(te_expr* in_expr) const
{
    breakpoint_condition.load_compiled_condition(in_expr);
}

void Breakpoint::try_to_compile_condition(const std::string& in_condition)
{
    breakpoint_condition.try_to_compile_condition(type, start, in_condition);
}

DebugWindow::DebugWindow()
{
    DebugStop = false;
    HWnd = NULL;

    StepInto = false;
    StepOver = -1;
}

DebugWindow::~DebugWindow()
{
}

void DebugWindow::TracePC(int pc) {}
void DebugWindow::TraceRead(uint32 start, uint32 stop) {}
void DebugWindow::TraceWrite(uint32 start, uint32 stop) {}
void DebugWindow::DoStepOver() {}

void DebugWindow::Breakpoint(int pc)
{
    if (!handled_ida_event)
    {
      send_pause_event(pc, changed);
      changed.clear();
    }

    Show_Genesis_Screen(HWnd);
    Update_RAM_Watch();
    Clear_Sound_Buffer();

    if (!DebugStop)
    {
        DebugStop = true;
        MSG msg = { 0 };
        for (; Gens_Running && DebugStop;)
        {
            Handle_Gens_Messages();
        }
        //DebugDummyHWnd=(HWND)0;
    }
}

bool DebugWindow::BreakPC(int pc) const
{
    for (auto i = Breakpoints.cbegin(); i != Breakpoints.cend(); ++i)
    {
        if (i->type != bp_type::BP_PC) continue;
        if (!(i->enabled)) continue;

        if (pc <= (int)(i->end) && pc >= (int)(i->start))
        {
			if (!i->check_condition())
			{
				return false;
			}
            return !(i->is_forbid);
        }
    }
    return false;
}

#ifdef DEBUG_68K
bool DebugWindow::BreakRead(int pc, uint32 start, uint32 stop, bool is_vdp)
#else
bool DebugWindow::BreakRead(int pc, uint32 start, uint32 stop)
#endif
{
    bool brk = false;

    for (auto i = Breakpoints.cbegin(); i != Breakpoints.cend(); ++i)
    {
        if (i->type != bp_type::BP_READ) continue;
        if (!i->enabled) continue;
#ifdef DEBUG_68K
        if (i->is_vdp != is_vdp) continue;
#endif

        if (start <= i->end && stop >= i->start)
        {
            brk = !(i->is_forbid);
            break;
        }
    }

    if (!brk) return false;

    for (auto i = Breakpoints.cbegin(); i != Breakpoints.cend(); ++i)
    {
        if (i->type != bp_type::BP_PC) continue;

        if (i->enabled && i->is_forbid)
        {
            if (pc <= (int)(i->end) && pc >= (int)(i->start))
                return false;
        }
    }

    return true;
}

#ifdef DEBUG_68K
bool DebugWindow::BreakWrite(int pc, uint32 start, uint32 stop, bool is_vdp)
#else
bool DebugWindow::BreakWrite(int pc, uint32 start, uint32 stop)
#endif
{
    bool brk = false;

    for (auto i = Breakpoints.cbegin(); i != Breakpoints.cend(); ++i)
    {
        if (i->type != bp_type::BP_WRITE) continue;
        if (!i->enabled) continue;
#ifdef DEBUG_68K
        if (i->is_vdp != is_vdp) continue;
#endif

        if (start <= i->end && stop >= i->start)
        {
            brk = !(i->is_forbid);
            break;
        }
    }

    if (!brk) return false;

    for (auto i = Breakpoints.cbegin(); i != Breakpoints.cend(); ++i)
    {
        if (i->type != bp_type::BP_PC) continue;

        if (i->enabled && i->is_forbid)
        {
            if (pc <= (int)(i->end) && pc >= (int)(i->start))
                return false;
        }
    }

    return true;
}