#ifndef DEBUG_WINDOW_H
#define DEBUG_WINDOW_H

#include <vector>
#include <string>
#include <map>
#include <memory>

typedef unsigned int uint32;
typedef unsigned short ushort;

enum class bp_type
{
    BP_PC = 1,
    BP_READ,
    BP_WRITE,
};

struct BreakpointCondition
{
    bool check_condition() const;

    bool is_compiled = true;
};

struct Breakpoint
{
    bp_type type;

    uint32 start;
    uint32 end;

    bool enabled;
    bool is_forbid;

#ifdef DEBUG_68K
    bool is_vdp;

    Breakpoint(bp_type _type, uint32 _start, uint32 _end, bool _enabled, bool _is_vdp, bool _is_forbid) :
      type(_type), start(_start), end(_end), enabled(_enabled), is_forbid(_is_forbid), is_vdp(_is_vdp) {}
#else
    Breakpoint(bp_type _type, uint32 _start, uint32 _end, bool _enabled, bool _is_forbid) :
      type(_type), start(_start), end(_end), enabled(_enabled), is_forbid(_is_forbid) {};
#endif

    bool check_condition() const;
    void try_to_compile_condition(const std::string& in_condition);

private:
    BreakpointCondition breakpoint_condition;
};

typedef std::vector<Breakpoint> bp_list;

#ifdef DEBUG_68K
#define MAX_ROM_SIZE 0x800000
#else
#define MAX_ROM_SIZE 0x10000 // including possible z80 code in 0x8000 - 0xFFFF
#endif

struct DebugWindow
{
    DebugWindow();
    std::vector<uint32> callstack;
    std::map<uint32_t, uint32_t> changed;
    bp_list Breakpoints;

    bool DebugStop;

    bool StepInto;
    uint32 StepOver;

    void Breakpoint(int pc);

    bool BreakPC(int pc) const;
#ifdef DEBUG_68K
    bool BreakRead(int pc, uint32 start, uint32 stop, bool is_vdp);
    bool BreakWrite(int pc, uint32 start, uint32 stop, bool is_vdp);
#else
    bool BreakRead(int pc, uint32 start, uint32 stop);
    bool BreakWrite(int pc, uint32 start, uint32 stop);
#endif

    virtual void DoStepOver();
    virtual void TracePC(int pc);
    virtual void TraceRead(uint32 start, uint32 stop);
    virtual void TraceWrite(uint32 start, uint32 stop);
    virtual ~DebugWindow();
};

extern void send_pause_event(int pc, std::map<uint32_t, uint32_t> changed);

#endif
