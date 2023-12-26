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
const static std::vector<te_variable> M68K_ENVIRONMENT = {
	{
		"D0", &main68k_context.dreg[0]
	},
    {
        "D1", &main68k_context.dreg[1]
    },
    {
        "D2", &main68k_context.dreg[2]
    }};

bool BreakpointCondition::check_condition() const
{
	if (condition_expr == nullptr)
	{
		return true;
	}
    return te_logic_eval(condition_expr) > 0;
}

bool Breakpoint::check_condition() const
{
    return breakpoint_condition.check_condition();
}

void Breakpoint::try_to_compile_condition(const std::string& in_condition)
{
	if (in_condition.empty())
	{
		return;
	}

    BreakpointConditionWorker::get().enqueue([in_type = type, in_start = start, in_condition]
    {
    	te_expr* expr = te_compile(in_condition.c_str(), true, M68K_ENVIRONMENT.data(), M68K_ENVIRONMENT.size(), nullptr);

        for (auto i = M68kDW.Breakpoints.begin(); i != M68kDW.Breakpoints.end(); ++i) 
        {
            if (i->type == in_type && i->start == in_start)
            {
                te_free(i->breakpoint_condition.condition_expr.load());
                i->breakpoint_condition.condition_expr.store(expr);
	            break;
            }
        }
    });
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