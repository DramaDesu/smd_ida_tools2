#ifndef DEBUG_WINDOW_H
#define DEBUG_WINDOW_H

#include <atomic>
#include <functional>
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

#include "tinyexpr/tinyexpr.h"

struct Breakpoint;
typedef unsigned int uint32;
typedef unsigned short ushort;

enum class bp_type
{
    BP_PC = 1,
    BP_READ,
    BP_WRITE,
};

struct BreakpointConditionWorker
{
	static BreakpointConditionWorker& get()
	{
		static BreakpointConditionWorker instance;
        return instance;
	}

    void start()
	{
        should_stop = false;
        worker_thread = std::thread(&BreakpointConditionWorker::worker, this);
	}

    void enqueue(std::function<void()>&& in_task)
    {
		if (should_stop)
		{
			return;
		}
		{
            std::lock_guard<std::mutex> lock(task_mutex);
            tasks.emplace(in_task);
		}
        
        task_cv.notify_one();
    }

    void stop()
    {
		if (worker_thread.joinable())
		{
            should_stop = true;
            task_cv.notify_one();

            worker_thread.join();
		}
    }

private:
    void worker()
    {
	    while (!should_stop)
	    {
		    std::unique_lock<std::mutex> lock(task_mutex);
            task_cv.wait(lock, [&] { return !tasks.empty() || should_stop; });
            if (tasks.empty())
            {
	            continue;
            }

            const std::function<void()> task = tasks.front();
            tasks.pop();

            lock.unlock();

            task();
	    }
    }

    std::mutex task_mutex; 
    std::condition_variable task_cv; 

    std::atomic<bool> should_stop;
    std::thread worker_thread;

    std::queue<std::function<void()>> tasks;
};

struct BreakpointCondition
{
	BreakpointCondition() = default;
    ~BreakpointCondition();

	BreakpointCondition(const BreakpointCondition& other);
	BreakpointCondition(BreakpointCondition&& other) noexcept = default;

    BreakpointCondition& operator=(const BreakpointCondition& other);
    BreakpointCondition& operator=(BreakpointCondition&& other) noexcept = default;

    bool check_condition() const;
    void try_to_compile_condition(bp_type in_type, uint32 in_start, const std::string& in_condition);

    void load_compiled_condition(te_expr* in_expr) const;

private:
	static void try_to_compile_condition_inner(bp_type in_type, uint32 in_start, const std::string& in_condition);

	struct condition_state_t
	{
        bp_type type{};
        uint32 start{};

        std::string raw_expr;
        std::atomic<te_expr*> expr;
	};

    std::unique_ptr<condition_state_t> condition_state;
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
    void load_compiled_condition(te_expr* in_expr) const;
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
