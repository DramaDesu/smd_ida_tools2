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

bool handled_ida_event;

void Handle_Gens_Messages();
extern int Gens_Running;
extern "C" int Clear_Sound_Buffer(void);

bool BreakpointCondition::check_condition() const
{
    if (!is_compiled)
    {
	    return false;
    }

    return true;
}

bool Breakpoint::check_condition() const
{
    return breakpoint_condition.check_condition();
}

#include "shunting-yard.h"

void Breakpoint::try_to_compile_condition(const std::string& in_condition)
{
	if (in_condition.empty())
	{
		return;
	}

    breakpoint_condition.is_compiled = false;

	cparse::TokenMap vars;
    vars["pi"] = 3.14;
	try
	{
        const auto token_value = cparse::calculator::calculate("7+1", &vars);
        if (token_value.asDouble())
        {

        }
	}
	catch (...)
	{
        std::cout << "WTF" << std::endl;
	}
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