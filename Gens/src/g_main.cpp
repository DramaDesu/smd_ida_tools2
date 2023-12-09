#include <Windows.h>
#include <vector>

#include <grpcpp/grpcpp.h>

#ifdef DEBUG_68K
#include "proto/debug_proto_68k.grpc.pb.h"
#else
#include "proto/debug_proto_z80.grpc.pb.h"
#endif

using grpc::Channel;
using grpc::ClientContext;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using idadebug::DbgServer;
using idadebug::DbgClient;
using idadebug::AnyRegValue;
using idadebug::GpRegsEnum;
using idadebug::GpReg;
using idadebug::GpRegValue;
using idadebug::GpRegs;
#ifdef DEBUG_68K
using idadebug::VdpRegsEnum;
using idadebug::VdpReg;
using idadebug::VdpRegValue;
using idadebug::VdpRegs;
using idadebug::DmaInfo;
#else
using idadebug::SoundBankMap;
using idadebug::SoundBankRange;
#endif
using idadebug::Changed;
using idadebug::MemData;
using idadebug::MemoryAS;
using idadebug::MemoryAD;
using idadebug::BpType;
using idadebug::DbgBreakpoint;
using idadebug::DbgBreakpoints;
using idadebug::Callstack;
using idadebug::PauseChanged;
using google::protobuf::Empty;
using google::protobuf::Map;

#include "gens.h"
#include "g_main.h"
#include "g_ddraw.h"
#include "g_dsound.h"
#include "g_input.h"
#include "rom.h"
#include "save.h"
#include "resource.h"
#include "misc.h"
#include "blit.h"
#include "ggenie.h"
#include "cpu_68k.h"
#include "star_68k.h"
#include "cpu_z80.h"
#include "z80.h"
#include "mem_m68k.h"
#include "mem_z80.h"
#include "joypads.h"
#include "psg.h"
#include "ym2612.h"
#include "pwm.h"
#include "vdp_io.h"
#include "vdp_rend.h"
#include "pcm.h"
#include "wave.h"
#include "ram_search.h"
#include "movie.h"
#include "ramwatch.h"
#include "luascript.h"
#include "hexeditor.h"
#include "parsecmdline.h"

#ifdef DEBUG_68K
#include "m68k_debugwindow.h"
#else
#include "z80_debugwindow.h"
#endif
#include "plane_explorer_kmod.h"
#include "tracer.h"

#include <iostream>
#include <commctrl.h>
#include <io.h>
#include <fcntl.h>
#include <errno.h>
#include <cstdio>
#include <cstring>

#undef min
#undef max

//std::shared_ptr<DbgClientClient> client;

extern "C" void Read_To_68K_Space(int adr);
extern void HexDestroyDialog();

bool hook_trace = 1;

#define WM_KNUX WM_USER + 3
#define GENS_VERSION   2.10
#define GENS_VERSION_H 2 * 65536 + 10

#define MINIMIZE                                \
{Clear_Sound_Buffer();                          \
if (Full_Screen)                                \
{                                               \
    Set_Render(HWnd, 0, -1, true);              \
    FS_Minimised = 1;                           \
}}

#define MENU_L(smenu, pos, flags, id, def) do{\
{InsertMenu((smenu), (pos), (flags), (id), def);} } while(0)

#define WORD_L(id, def)\
{SetDlgItemText(hDlg, id, def);}

#define MESSAGE_L(def)\
{Put_Info(def);}

#define MESSAGE_NUM_L(def, num)                                                             \
{                                                                                                   \
    char mes_tmp[1024];                                                                             \
    sprintf(mes_tmp, def, (num));                                                               \
    Put_Info(mes_tmp);                                                                              \
}

HINSTANCE ghInstance;
HACCEL hAccelTable = NULL;
WNDCLASS WndClass;
HWND HWnd;
HMENU Gens_Menu;
HMENU Context_Menu;
int Gens_Menu_Width = 0; // in pixels
HWND RamSearchHWnd = NULL; // modeless dialog
HWND RamWatchHWnd = NULL; // modeless dialog
HWND PlaneExplorerHWnd = NULL; // modeless dialog
HWND VDPRamHWnd = NULL; // modeless dialog
HWND YM2612DbgHWnd = NULL; // modeless dialog
HWND VDPSpritesHWnd = NULL; // modeless dialog
HWND RamCheatHWnd = NULL; // modeless dialog
std::vector<HWND> LuaScriptHWnds; // modeless dialogs
HWND VolControlHWnd = NULL;

int no_debug = 0;
char Str_Tmp[1024];
char Comment[256];
char Gens_Path[1024];
struct Rom *Game = NULL;
int Active = 0;
int Paused = 0;
int Full_Screen = -1;
int Resolution = 1;
int Fast_Blur = 0;
int Render_W = 0;
int Setting_Render = 0;
int Render_FS = 0;
int Show_FPS = 0;
int Show_Message = 0;
int FS_Minimised = 0;
int Auto_Pause = 0;
int Auto_Fix_CS = 0;
int Country = -1;
int Country_Order[3];
int Intro_Style = 0;
int Gens_Running = 0;
int WinNT_Flag = 0;
int Gens_Priority;
int SS_Actived;
int DialogsOpen = 0; //Modif
int SlowDownMode = 0; //Modif
int VideoLatencyCompensation = 0;
int disableVideoLatencyCompensationCount = 0;
float ScaleFactor = 1.0;

BOOL AutoFireKeyDown = 0;   //Modif N.
BOOL AutoHoldKeyDown = 0;   //Modif N.
BOOL AutoClearKeyDown = 0;  //Modif N.
BOOL FrameAdvanceKeyDown = 0; //Modif
BOOL FastForwardKeyDown = 0; //Modif
BOOL TurboToggle = 0;
BOOL TurboMode = 0;

int SlowDownSpeed = 1;  //Modif
int RecordMovieCanceled = 1;//Modif
int PlayMovieCanceled = 1; //Modif
int Disable_Blue_Screen = 1; //Modif
int Never_Skip_Frame = 0; //Modif
int SkipKeyIsPressed = 0; //Modif
int FrameCounterEnabled = 0; //Modif
int FrameCounterFrames = 0; //Modif N.
int LagCounterEnabled = 0; //Modif
int LagCounterFrames = 0; //Modif N.
int ShowInputEnabled = 0; //Modif
int AutoBackupEnabled = 0; //Modif
int LeftRightEnabled = 0; //Modif
int FrameCounterPosition = 16 * 336 + 32; //Modif
int MustUpdateMenu = 0; // Modif // menuNeedsBuilding buildMenuNeeded
bool RamSearchClosed = false;
bool RamWatchClosed = false;

unsigned char StateSelectCfg = 0;
bool PaintsEnabled = true;
extern "C" int g_dontResetAudioCache;

typeMovie SubMovie;
unsigned long SpliceFrame = 0;
unsigned long SeekFrame = 0;
char *TempName;
char SpliceMovie[1024];
//long x = 0, y = 0, xg = 0, yg = 0;

bool frameadvSkipLagForceDisable = false;
bool frameadvSkipLag = false;
bool skipLagNow = false;
bool lastFrameAdvancePaused = false;
ALIGN16 static unsigned char frameadvSkipLag_Rewind_State_Buffer[2][MAX_STATE_FILE_LENGTH];
long long frameadvSkipLag_Rewind_Input_Buffer[2] = { ~0, ~0 };
int frameadvSkipLag_Rewind_State_Buffer_Index = 0;
bool frameadvSkipLag_Rewind_State_Buffer_Valid = false;
extern void Update_Emulation_One_Before(HWND hWnd);
extern void Update_Emulation_After_Fast(HWND hWnd);
extern char Lua_Dir[1024];

int frameSearchFrames = -1;
bool frameSearchInitialized = false;
long long frameSearchInitialInput = ~0;
long long frameSearchFinalInput = ~0;
ALIGN16 static unsigned char frameSearch_Start_State_Buffer[MAX_STATE_FILE_LENGTH];
ALIGN16 static unsigned char frameSearch_End_State_Buffer[MAX_STATE_FILE_LENGTH];

// used to manage sound clearing (mainly so frame advance can have sound without looping that sound annoyingly)
DWORD tgtime = timeGetTime(); // time of last sound generation
bool soundCleared = false;

POINT Window_Pos;

LRESULT WINAPI WinProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK GGenieProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK ColorProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK DirectoriesProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK FilesProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK ControllerProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK OptionProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK AboutProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK PlayMovieProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK RecordMovieProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK RamSearchProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK RamWatchProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK HexEditorProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK VDPRamProc(HWND, UINT, WPARAM, LPARAM);
#ifdef DEBUG_Z80
LRESULT CALLBACK YM2612WndProcDialog(HWND, UINT, WPARAM, LPARAM);
#endif
LRESULT CALLBACK VDPSpritesProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK RamCheatProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK PromptSpliceFrameProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK PromptSeekFrameProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK LuaScriptProc(HWND, UINT, WPARAM, LPARAM);

LRESULT CALLBACK EditWatchProc(HWND, UINT, WPARAM, LPARAM);
void DoMovieSplice();

int Set_Render(HWND hWnd, int Full, int Num, int Force);
HMENU Build_Main_Menu(void);
HMENU Build_Context_Menu(void);

void ResetResults()
{
    reset_address_info();
    ResultCount = 0;
    if (RamSearchHWnd)
        ListView_SetItemCount(GetDlgItem(RamSearchHWnd, IDC_RAMLIST), ResultCount);
}

void CloseRamWindows() //Close the Ram Search & Watch windows when rom closes
{
    ResetWatches();
    ResetResults();
    if (RamSearchHWnd)
    {
        SendMessage(RamSearchHWnd, WM_CLOSE, NULL, NULL);
        RamSearchClosed = true;
    }
    if (RamWatchHWnd)
    {
        SendMessage(RamWatchHWnd, WM_CLOSE, NULL, NULL);
        RamWatchClosed = true;
    }
}

void ReopenRamWindows() //Reopen them when a new Rom is loaded
{
    HWND hwnd = GetActiveWindow();

    if (RamSearchClosed)
    {
        RamSearchClosed = false;
        if (!RamSearchHWnd)
        {
            reset_address_info();
            RamSearchHWnd = CreateDialog(ghInstance, MAKEINTRESOURCE(IDD_RAMSEARCH), HWnd, (DLGPROC)RamSearchProc);
            DialogsOpen++;
        }
    }
    if (RamWatchClosed || AutoRWLoad)
    {
        RamWatchClosed = false;
        if (!RamWatchHWnd)
        {
            if (AutoRWLoad) OpenRWRecentFile(0);
            RamWatchHWnd = CreateDialog(ghInstance, MAKEINTRESOURCE(IDD_RAMWATCH), HWnd, (DLGPROC)RamWatchProc);
            DialogsOpen++;
        }
    }

    if (hwnd == HWnd && hwnd != GetActiveWindow())
        SetActiveWindow(HWnd); // restore focus to the main window if it had it before
}

int Change_VSync(HWND hWnd)
{
    int *p_vsync;

    if (Full_Screen)
    {
        End_DDraw();
        p_vsync = &FS_VSync;
    }
    else p_vsync = &W_VSync;

    *p_vsync = 1 - *p_vsync;

    if (*p_vsync) MESSAGE_L("Vertical Sync Enabled")
    else MESSAGE_L("Vertical Sync Disabled");

        Build_Main_Menu();
    if (Full_Screen) return Init_DDraw(hWnd);
    else return 1;
}

int Set_Frame_Skip(int Num)
{
    Frame_Skip = Num;

    if (Frame_Skip != -1)
        MESSAGE_NUM_L("Frame skip set to %d", Frame_Skip)
    else
        MESSAGE_L("Frame skip set to Auto");
        if (SeekFrame)
        {
            MESSAGE_L("Seek Cancelled");
        }
    SeekFrame = 0;
    Build_Main_Menu();
    return(1);
}

int Set_Latency_Compensation(int Num)
{
    if (Num == VideoLatencyCompensation)
        return 1;

    VideoLatencyCompensation = Num;

    char msg[256];
    sprintf(msg, "Set to adjust for %d frame%s of video lag", VideoLatencyCompensation, VideoLatencyCompensation == 1 ? "" : "s");
    MESSAGE_L(msg);
        disableVideoLatencyCompensationCount = 0;

    Build_Main_Menu();
    return(1);
}

int IsVideoLatencyCompensationOn()
{
    if (MainMovie.Status == MOVIE_PLAYING && !MainMovie.Recorded)
        return 0; // the option is for input responsiveness, it's useless when watching a movie

    if (disableVideoLatencyCompensationCount > 0)
        return 0;

    return VideoLatencyCompensation > 0;
}

int Set_Current_State(int Num, bool showOccupiedMessage, bool showEmptyMessage)
{
    FILE *f;

    Current_State = Num;

    if ((f = Get_State_File()) != NULL)
    {
        fclose(f);
        if (showOccupiedMessage)
        {
            MESSAGE_NUM_L("SLOT %d [OCCUPIED]", Current_State)
        }
    }
    else if (showEmptyMessage)
    {
        MESSAGE_NUM_L("SLOT %d [EMPTY]", Current_State)
    }

    MustUpdateMenu = 1;
    return 1;
}

int Change_Stretch(void)
{
    Flag_Clr_Scr = 1;

    if ((Stretch = (1 - Stretch)) != 0)
        MESSAGE_L("Stretched mode")
    else
        MESSAGE_L("Correct ratio mode");

        Build_Main_Menu();
    return(1);
}

int Change_Blit_Style(void)
{
    if ((!Full_Screen) || (Render_FS > 1)) return(0);

    Flag_Clr_Scr = 1;

    if ((Blit_Soft = (1 - Blit_Soft)) != NULL)
        MESSAGE_L("Force software blit for Full-Screen")
    else
        MESSAGE_L("Enable hardware blit for Full-Screen");

        return(1);
}

int Set_Sprite_Over(int Num)
{
    if ((Sprite_Over = Num) != 0)
        MESSAGE_L("Sprite Limit Enabled")
    else
        MESSAGE_L("Sprite Limit Disabled");

        Build_Main_Menu();
    return(1);
}

int Change_Fast_Blur()
{
    Flag_Clr_Scr = 1;

    if ((Fast_Blur = (1 - Fast_Blur)) != 0)
        MESSAGE_L("Fast Blur Enabled")
    else
        MESSAGE_L("Fast Blur Disabled");

        Build_Main_Menu();
    return(1);
}

int Change_Layer(int Num) //Nitsuja added this to allow for layer enabling and disabling.
{
    struct { char * val; char *name; } layers[] = {
        { &VScrollAl, "Scroll A Low" },
        { &VScrollBl, "Scroll B Low" },
        { &VScrollAh, "Scroll A High" },
        { &VScrollBh, "Scroll B High" },
        { &VSpritel, "Sprites Low" },
        { &VSpritel, "Sprites High" },
    };

    if (Num < 0 || Num >= sizeof(layers) / sizeof(layers[0]))
        return 0;

    char &val = *layers[Num].val;
    val = !val;
    char message[256];
    sprintf(message, "%s %sabled", layers[Num].name, val ? "en" : "dis");
    MESSAGE_L(message);

        Build_Main_Menu();
    return(1);
}

int Change_SpriteTop()
{
    Sprite_Always_Top = !Sprite_Always_Top;

    char message[256];
    sprintf(message, "Always Show Sprites %sabled", Sprite_Always_Top ? "en" : "dis");
    MESSAGE_L(message);

        Build_Main_Menu();
    return (1);
}

int Change_SpriteBoxing()
{
    Sprite_Boxing = !Sprite_Boxing;

    char message[256];
    sprintf(message, "Sprites Boxing %sabled", Sprite_Boxing ? "en" : "dis");
    MESSAGE_L(message);

        Build_Main_Menu();
    return (1);
}

int Change_LayerSwap(int num)
{
    char *Plane;
    switch (num)
    {
    case 0:
        Plane = &Swap_Scroll_PriorityA;
        break;
    case 1:
        Plane = &Swap_Scroll_PriorityB;
        break;
    case 2:
        Plane = &Swap_Sprite_Priority;
        break;
    default:
        return 1;
    }
    *Plane = !(*Plane);

    char message[256];
    sprintf(message, "Layer Swapping %sabled", *Plane ? "en" : "dis");
    MESSAGE_L(message);

        Build_Main_Menu();
    return (1);
}

int Change_Plane(int num)
{
    char *Plane;
    char Layer[9];
    switch (num)
    {
    case 0:
        Plane = &ScrollAOn;
        sprintf(Layer, "Scroll A");
        break;
    case 1:
        Plane = &ScrollBOn;
        sprintf(Layer, "Scroll B");
        break;
    case 2:
        Plane = &SpriteOn;
        sprintf(Layer, "Sprites");
        break;
    default:
        return 1;
    }
    *Plane = !(*Plane);

    char message[256];
    sprintf(message, "Plane %s %sabled", Layer, *Plane ? "en" : "dis");
    MESSAGE_L(message);

        Build_Main_Menu();
    return (1);
}

typedef void(*BlitFunc)(unsigned char*, int, int, int, int);

void Set_Rend_Int(int Num, int* Rend, BlitFunc* Blit)
{
    bool quiet = false;
    if (Num == -1)
    {
        Num = *Rend;
        quiet = true;
    }
    else
    {
        *Rend = Num;
    }

    switch (Num)
    {
    case 0:
        if (Have_MMX) *Blit = Blit_X1_MMX;
        else *Blit = Blit_X1;
        break;

    case 1:
        if (Have_MMX) *Blit = Blit_X2_MMX;
        else *Blit = Blit_X2;
        break;

    case 2:
        *Blit = CBlit_EPX;
        break;

    case 3:
        if (Bits32) *Blit = CBlit_X2_Int;
        else if (Have_MMX) *Blit = Blit_X2_Int_MMX;
        else *Blit = Blit_X2_Int;
        break;

    case 4:
        if (Bits32) *Blit = CBlit_Scanline;
        else if (Have_MMX) *Blit = Blit_Scanline_MMX;
        else *Blit = Blit_Scanline;
        break;

    case 5:
        if (!Bits32 && Have_MMX) *Blit = Blit_Scanline_50_MMX;
        else *Blit = CBlit_Scanline_50;
        break;

    case 6:
        if (!Bits32 && Have_MMX) *Blit = Blit_Scanline_25_MMX;
        else *Blit = CBlit_Scanline_25;
        break;

    case 7:
        if (Bits32) *Blit = CBlit_Scanline_Int;
        else if (Have_MMX) *Blit = Blit_Scanline_Int_MMX;
        else *Blit = Blit_Scanline_Int;
        break;

    case 8:
        if (!Bits32 && Have_MMX) *Blit = Blit_Scanline_50_Int_MMX;
        else *Blit = CBlit_Scanline_50_Int;
        break;

    case 9:
        if (!Bits32 && Have_MMX) *Blit = Blit_Scanline_25_Int_MMX;
        else *Blit = CBlit_Scanline_25_Int;
        break;

    case 10:
        if (Have_MMX) *Blit = Blit_2xSAI_MMX;
        else
        {
            *Rend = 7;
            *Blit = Blit_Scanline_Int;
        }
        break;

    case 11:
        *Blit = CBlit_EPXPlus;
        break;

    default:
        *Rend = 1;
        if (Have_MMX) *Blit = Blit_X2_MMX;
        else *Blit = Blit_X2;
        break;
    }

    if (!quiet)
    {
        switch (*Rend)
        {
        case 0: MESSAGE_L("Render selected : NORMAL"); break;
        case 1: MESSAGE_L("Render selected : DOUBLE"); break;
        case 2: MESSAGE_L("Render selected : EPX 2X SCALE"); break;
        case 3: MESSAGE_L("Render selected : INTERPOLATED"); break;
        case 4: MESSAGE_L("Render selected : FULL SCANLINE"); break;
        case 5: MESSAGE_L("Render selected : 50% SCANLINE"); break;
        case 6: MESSAGE_L("Render selected : 25% SCANLINE"); break;
        case 7: MESSAGE_L("Render selected : INTERPOLATED SCANLINE"); break;
        case 8: MESSAGE_L("Render selected : INTERPOLATED 50% SCANLINE"); break;
        case 9: MESSAGE_L("Render selected : INTERPOLATED 25% SCANLINE"); break;
        case 10: MESSAGE_L("Render selected : 2XSAI KREED'S ENGINE"); break;
        case 11: MESSAGE_L("Render selected : EPX+"); break;
        default: MESSAGE_L("Render selected : ??????"); break;
        }
    }
}

void Set_Window_Size(HWND hWnd)
{
    RECT r;

    int xRight = (int)ceilf(320 * ((Render_W == 0) ? 1 : 2) * ScaleFactor);
    int yBottom = (int)ceilf(240 * ((Render_W == 0) ? 1 : 2) * ScaleFactor);

    GetWindowRect(hWnd, &r);
    SetWindowLong(hWnd, GWL_STYLE, GetWindowLong(hWnd, GWL_STYLE) | WS_OVERLAPPEDWINDOW);
    SetRect(&r, 0, 0, xRight, yBottom);
    // don't let the menu go multi-line, since it would squash the game view because AdjustWindowRectEx doesn't take it into account
    if (r.right < Gens_Menu_Width)
        r.right = Gens_Menu_Width;
    AdjustWindowRectEx(&r, GetWindowLong(hWnd, GWL_STYLE), 1, GetWindowLong(hWnd, GWL_EXSTYLE));
    SetWindowPos(hWnd, NULL, Window_Pos.x, Window_Pos.y, r.right - r.left, r.bottom - r.top, SWP_NOZORDER | SWP_NOACTIVATE);
}

int Set_Render(HWND hWnd, int Full, int Num, int Force)
{
    Setting_Render = TRUE;

    int numAttempts = 0;
tryAgain:
    numAttempts++;

    int Old_Rend, *Rend;
    BlitFunc* Blit;

    if (Full)
    {
        Rend = &Render_FS; // Render_FS = ...
        Blit = &Blit_FS; // Blit_FS = ...
    }
    else
    {
        Rend = &Render_W; // Render_W = ...
        Blit = &Blit_W; // Blit_W = ...
    }

    Old_Rend = *Rend;
    Flag_Clr_Scr = 1;

    bool reinit = false;
    if (Full != Full_Screen || (Num != -1 && (Num < 2 || Old_Rend < 2)) || Force)
        reinit = true;
    else if (Bits32 && (Num >= 0 ? Num : *Rend) == 10) // note: this is in the else statement because Bits32 is only valid to check here if reinit is false
        Num = 11; // we don't support 2xSaI in 32-bit mode

    Set_Rend_Int(Num, Rend, Blit);

    if (reinit)
    {
        if (Sound_Initialised) Clear_Sound_Buffer();

        End_DDraw();

        if ((Full_Screen = Full) != 0)
        {
            while (ShowCursor(true) < 1);
            while (ShowCursor(false) >= 0);

            SetWindowPos(hWnd, NULL, 0, 0, 320 * ((*Rend == 0) ? 1 : 2), 240 * ((*Rend == 0) ? 1 : 2), SWP_NOZORDER | SWP_NOACTIVATE);
            SetWindowLong(hWnd, GWL_STYLE, NULL);
        }
        else
        {
            //          memset(&dm, 0, sizeof(DEVMODE));
            //          dm.dmSize = sizeof(DEVMODE);
            //          dm.dmBitsPerPel = 16;
            //          dm.dmFields = DM_BITSPERPEL;

            //          ChangeDisplaySettings(&dm, 0);

            while (ShowCursor(false) >= 0);
            while (ShowCursor(true) < 1);

            Set_Window_Size(hWnd);
        }
        DEVMODE dm;
        EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &dm);
        Bits32 = ((dm.dmBitsPerPel > 16) ? 1 : 0);

        Build_Main_Menu();
        const int retval = Init_DDraw(HWnd);
        if (retval == 0 && numAttempts < 2)
        {
            // failed to initialize, try one more time with the simplest settings before giving up
            // this way Gens can recover if the video card doesn't allow fullscreen or incorrectly supports hardware video acceleration
            Num = 0;
            Full = 0;
            Force = 1;
            goto tryAgain;
        }
        Setting_Render = FALSE;
        return retval;
    }

    InvalidateRect(hWnd, NULL, FALSE);
    Show_Genesis_Screen(hWnd);
    Build_Main_Menu();
    Setting_Render = FALSE;
    return 1;
}

int Change_Z80()
{
    if (Z80_State & 1)
    {
        Z80_State &= ~1;
        MESSAGE_L("Z80 Disabled");
    }
    else
    {
        Z80_State |= 1;
        MESSAGE_L("Z80 Enabled");
    }

    Build_Main_Menu();
    return(1);
}

int Change_Sample_Rate(HWND hWnd, int Rate)
{
    unsigned char Reg_1[0x200];

    switch (Rate)
    {
    case 0:
        Sound_Rate = 11025;
        MESSAGE_L("Sound rate set to 11025");
            break;

    case 1:
        Sound_Rate = 22050;
        MESSAGE_L("Sound rate set to 22050");
            break;

    case 2:
        Sound_Rate = 44100;
        MESSAGE_L("Sound rate set to 44100");
            break;
    }

    PSG_Save_State();
    YM2612_Save(Reg_1);

    End_Sound();

    if (CPU_Mode)
    {
      YM2612_Init(CLOCK_PAL / 7, Sound_Rate, 1);
      PSG_Init(CLOCK_PAL / 15, Sound_Rate);
    }
    else
    {
      YM2612_Init(CLOCK_NTSC / 7, Sound_Rate, 1);
      PSG_Init(CLOCK_NTSC / 15, Sound_Rate);
    }

    YM2612_Restore(Reg_1);
    PSG_Restore_State();

    if (!Init_Sound(hWnd)) return(0);

    Play_Sound();

    Build_Main_Menu();
    return(1);
}

int Change_Sound_Stereo(HWND hWnd)
{
    unsigned char Reg_1[0x200];

    if (Sound_Stereo)
    {
        Sound_Stereo = 0;
        MESSAGE_L("Mono sound");
    }
    else
    {
        Sound_Stereo = 1;
        MESSAGE_L("Stereo sound");
    }

    PSG_Save_State();
    YM2612_Save(Reg_1);

    End_Sound();

    if (CPU_Mode)
    {
      YM2612_Init(CLOCK_PAL / 7, Sound_Rate, 1);
      PSG_Init(CLOCK_PAL / 15, Sound_Rate);
    }
    else
    {
      YM2612_Init(CLOCK_NTSC / 7, Sound_Rate, 1);
      PSG_Init(CLOCK_NTSC / 15, Sound_Rate);
    }

    YM2612_Restore(Reg_1);
    PSG_Restore_State();

    if (!Init_Sound(hWnd)) return(0);

    Play_Sound();

    Build_Main_Menu();
    return(1);
}

// Modif N.
int Change_Sound_Soften(HWND hWnd)
{
    if (Sound_Soften)
    {
        Sound_Soften = 0;
        MESSAGE_L("Low pass filter off");
    }
    else
    {
        Sound_Soften = 1;

        if (Sound_Rate == 44100)
        {
            MESSAGE_L("Low pass filter on");
        }
        else
        {
            Change_Sample_Rate(hWnd, 2);
            MESSAGE_L("Low pass filter on and rate changed to 44100");
        }
    }

    Build_Main_Menu();
    return(1);
}

// Modif N.
int Change_Sound_Hog()
{
    if (Sleep_Time)
    {
        Sleep_Time = 0;
        MESSAGE_L("Maximum CPU usage");
    }
    else
    {
        Sleep_Time = 5;
        MESSAGE_L("Balanced CPU usage");
    }

    Build_Main_Menu();
    return(1);
}

int Change_Country(HWND hWnd, int Num)
{
    unsigned char Reg_1[0x200];

    Flag_Clr_Scr = 1;

    switch (Country = Num)
    {
    default:
    case -1:
        if (Genesis_Started) Detect_Country_Genesis();
        break;

    case 0:
        Game_Mode = 0;
        CPU_Mode = 0;
        break;

    case 1:
        Game_Mode = 1;
        CPU_Mode = 0;
        break;

    case 2:
        Game_Mode = 1;
        CPU_Mode = 1;
        break;

    case 3:
        Game_Mode = 0;
        CPU_Mode = 1;
        break;
    }

    if (CPU_Mode)
    {
        CPL_Z80 = Round_Double((((double)CLOCK_PAL / 15.0) / 50.0) / 312.0);
        CPL_M68K = Round_Double((((double)CLOCK_PAL / 7.0) / 50.0) / 312.0);

        VDP_Num_Lines = 312;
        VDP_Status |= 0x0001;
    }
    else
    {
        CPL_Z80 = Round_Double((((double)CLOCK_NTSC / 15.0) / 60.0) / 262.0);
        CPL_M68K = Round_Double((((double)CLOCK_NTSC / 7.0) / 60.0) / 262.0);

        VDP_Num_Lines = 262;
        VDP_Status &= 0xFFFE;
    }

    PSG_Save_State();
    YM2612_Save(Reg_1);

    End_Sound();

    if (CPU_Mode)
    {
      YM2612_Init(CLOCK_PAL / 7, Sound_Rate, 1);
      PSG_Init(CLOCK_PAL / 15, Sound_Rate);
    }
    else
    {
      YM2612_Init(CLOCK_NTSC / 7, Sound_Rate, 1);
      PSG_Init(CLOCK_NTSC / 15, Sound_Rate);
    }

    YM2612_Restore(Reg_1);
    PSG_Restore_State();

    if (!Init_Sound(hWnd)) return(0);

    Play_Sound();

    if (Game_Mode)
    {
        if (CPU_Mode) MESSAGE_L("Europe system (50 FPS)")
        else MESSAGE_L("USA system (60 FPS)");
    }
    else
    {
        if (CPU_Mode) MESSAGE_L("Japan system (50 FPS)")
        else MESSAGE_L("Japan system (60 FPS)");
    }

    if (Genesis_Started)
    {
        if ((CPU_Mode == 1) || (Game_Mode == 0))
            sprintf(Str_Tmp, GENS_NAME " - Megadrive : %s", Game->Rom_Name_W);
        else
            sprintf(Str_Tmp, GENS_NAME " - Genesis : %s", Game->Rom_Name_W);
    }

    if (Game)
    {
        // Modif N. - remove double-spaces from title bar
        for (int i = 0; i < (int)strlen(Str_Tmp) - 1; i++)
            if (Str_Tmp[i] == Str_Tmp[i + 1] && Str_Tmp[i] == ' ')
                strcpy(Str_Tmp + i, Str_Tmp + i + 1), i--;

        SetWindowText(HWnd, Str_Tmp);
    }

    Build_Main_Menu();
    return 1;
}

int Change_Country_Order(int Num)
{
    char c_str[4][4] = { "USA", "JAP", "EUR" };
    char str_w[128];
    int sav = Country_Order[Num];

    if (Num == 1) Country_Order[1] = Country_Order[0];
    else if (Num == 2)
    {
        Country_Order[2] = Country_Order[1];
        Country_Order[1] = Country_Order[0];
    }
    Country_Order[0] = sav;

    if (Country == -1) Change_Country(HWnd, -1);        // Update Country

    wsprintf(str_w, "Country detec.order : %s %s %s", c_str[Country_Order[0]], c_str[Country_Order[1]], c_str[Country_Order[2]]);
    MESSAGE_L(str_w);

        Build_Main_Menu();
    return(1);
}

#ifdef CC_SUPPORT
void CC_End_Callback(char mess[256])
{
    MessageBox(HWnd, mess, "Console Classix", MB_OK);

    if (Sound_Initialised) Clear_Sound_Buffer();
    Free_Rom(Game);
    Build_Main_Menu();
}
#endif

BOOL Init(HINSTANCE hInst, int nCmdShow)
{
    int i;

    timeBeginPeriod(1);

    Full_Screen = -1;
    VDP_Num_Vis_Lines = 224;
    Resolution = 1;
    W_VSync = 0;
    FS_VSync = 0;
    Stretch = 0;
    Sprite_Over = 1;
    VScrollAl = 1; // Modif N.
    VScrollBl = 1; // Modif N.
    VScrollAh = 1; // Modif N.
    VScrollBh = 1; // Modif N.
    VSpritel = 1; // Modif U.
    VSpriteh = 1; // Modif U.
    ScrollAOn = 1;
    ScrollBOn = 1;
    SpriteOn = 1;
    Sprite_Always_Top = 0;
    PinkBG = 0;
    Render_W = 0;
    Render_FS = 0;
    Show_Message = 1;

    Sound_Segs = 8;
    Sound_Stereo = 1;
    Sound_Initialised = 0;
    Sound_Is_Playing = 0;

    FS_Minimised = 0;
    Game = NULL;
    Genesis_Started = 0;
    CPU_Mode = 0;
    Window_Pos.x = 0;
    Window_Pos.y = 0;

    WndClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    WndClass.lpfnWndProc = WinProc;
    WndClass.cbClsExtra = 0;
    WndClass.cbWndExtra = 0;
    WndClass.hInstance = hInst;
    WndClass.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_SONIC));
    WndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    WndClass.hbrBackground = NULL;
    WndClass.lpszMenuName = NULL;
    WndClass.lpszClassName = "Gens";

    FrameCount = 0;
    LagCount = 0;
    LagCountPersistent = 0;
    frameSearchFrames = -1; frameSearchInitialized = false;

    RegisterClass(&WndClass);

    ghInstance = hInst;

    HWnd = CreateWindowEx(
        NULL,
        "Gens",
        GENS_NAME " - Idle",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        320 * 2,
        240 * 2,
        NULL,
        NULL,
        hInst,
        NULL);

    if (!HWnd) return FALSE;

    Identify_CPU();
    i = GetVersion();
    // Get major and minor version numbers of Windows
    if (((i & 0xFF) > 4) || (i & 0x80000000)) WinNT_Flag = 0;
    else WinNT_Flag = 1;

    GetCurrentDirectory(1024, Gens_Path);

    strcat(Gens_Path, "\\");

    M68K_Init();
    Z80_Init();

    YM2612_Init(CLOCK_NTSC / 7, Sound_Rate, 1);
    PSG_Init(CLOCK_NTSC / 15, Sound_Rate);

    Build_Main_Menu(); // needs to be before config is loaded so Gens_Menu_Width is valid when the render mode gets set

    strcpy(Str_Tmp, Gens_Path);
    strcat(Str_Tmp, "\\gens.cfg");
    Load_Config(Str_Tmp, NULL);

    ShowWindow(HWnd, nCmdShow);

    if (!Init_Input(hInst, HWnd))
    {
        End_Sound();
        End_DDraw();
        return FALSE;
    }

    Build_Main_Menu();

    DragAcceptFiles(HWnd, TRUE);

    SystemParametersInfo(SPI_GETSCREENSAVEACTIVE, 0, &SS_Actived, 0);
    SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, FALSE, NULL, 0);

    switch (Gens_Priority)
    {
    case 0:
        SetThreadPriority(hInst, THREAD_PRIORITY_BELOW_NORMAL);
        break;

    case 2:
        SetThreadPriority(hInst, THREAD_PRIORITY_ABOVE_NORMAL);
        break;

    case 3:
        SetThreadPriority(hInst, THREAD_PRIORITY_HIGHEST);
        break;

    case 5:
        SetThreadPriority(hInst, THREAD_PRIORITY_TIME_CRITICAL);
        break;
    }

    Put_Info("   Gens Initialized", 1); // Modif N. -- added mainly to clear out some message gunk

    Gens_Running = 1;

    return TRUE;
}

void End_All(void)
{
    Free_Rom(Game);
    End_DDraw();
    End_Input();
    YM2612_End();
    End_Sound();

    DragAcceptFiles(HWnd, FALSE);

    SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, SS_Actived, NULL, 0);
    if (MainMovie.File != NULL)
        CloseMovieFile(&MainMovie);

    timeEndPeriod(1);
}

void Handle_Gens_Messages()
{
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
    {
        if (!GetMessage(&msg, NULL, 0, 0))
        {
            Gens_Running = 0;
            StopAllLuaScripts();
        }

        if (RamSearchHWnd && IsDialogMessage(RamSearchHWnd, &msg))
            continue;
        if (RamWatchHWnd && IsDialogMessage(RamWatchHWnd, &msg))
        {
            if (msg.message == WM_KEYDOWN) // send keydown messages to the dialog (for accelerators, and also needed for the Alt key to work)
                SendMessage(RamWatchHWnd, msg.message, msg.wParam, msg.lParam);
            continue;
        }
        if (YM2612DbgHWnd && IsDialogMessage(YM2612DbgHWnd, &msg))
        {
          if (msg.message == WM_KEYDOWN) // send keydown messages to the dialog (for accelerators, and also needed for the Alt key to work)
            SendMessage(YM2612DbgHWnd, msg.message, msg.wParam, msg.lParam);
          continue;
        }
        if (VDPRamHWnd && IsDialogMessage(VDPRamHWnd, &msg))
        {
          if (msg.message == WM_KEYDOWN) // send keydown messages to the dialog (for accelerators, and also needed for the Alt key to work)
            SendMessage(VDPRamHWnd, msg.message, msg.wParam, msg.lParam);
          continue;
        }
        if (VolControlHWnd && IsDialogMessage(VolControlHWnd, &msg))
            continue;
        bool docontinue = false;
        for (UINT i = 0; i < HexEditors.size(); i++) {
            if (HexEditors[i]->Hwnd && IsDialogMessage(HexEditors[i]->Hwnd, &msg)) {
                if (msg.message == WM_CHAR)
                    SendMessage(HexEditors[i]->Hwnd, msg.message, msg.wParam, msg.lParam);
                docontinue = true;
            }
        }
        for (unsigned int i = 0; i < LuaScriptHWnds.size(); i++)
            if (IsDialogMessage(LuaScriptHWnds[i], &msg))
                docontinue = true;
        if (docontinue)
            continue;
        if (TranslateAccelerator(HWnd, hAccelTable, &msg))
            continue;

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    DontWorryLua();

    Update_Input();
}

// stripped-down single step of the main loop but with some extra control over what it does
bool Step_Gens_MainLoop(bool allowSleep, bool allowEmulate)
{
    Handle_Gens_Messages();
    Check_Misc_Key();
    if (MustUpdateMenu)
    {
        Build_Main_Menu();
        MustUpdateMenu = 0;
    }

    if (!Game)
        return true;

    bool reachedEmulate = false;

    if ((Active || BackgroundInput) && Check_Skip_Key())
    {
        // handle frame advance key
        if (Paused)
        {
            if (allowEmulate)
                Update_Emulation_One(HWnd);
            reachedEmulate = true;
            soundCleared = false;
            tgtime = timeGetTime();
            lastFrameAdvancePaused = false;
        }
        else
        {
            // if the game isn't paused yet then the first press of the frame advance key only pauses the game
            Paused = 1;
            Clear_Sound_Buffer();
            soundCleared = true;
            lastFrameAdvancePaused = true;
        }
    }
    else // normal emulation
    {
        if ((Active) && (!Paused))  // EMULATION
        {
            if (!allowEmulate || Update_Emulation(HWnd))
                reachedEmulate = true;
        }
        else        // EMULATION PAUSED
        {
            if (!soundCleared && timeGetTime() - tgtime >= 125) //eliminate stutter
            {
                Clear_Sound_Buffer();
                soundCleared = true;
            }
        }

        if (Sleep_Time && allowSleep)
        {
            if (Paused)
            {
                Sleep(1);
            }
            else
            {
                static int count = 0;
                count++;
                if (!TurboMode)
                    if ((count % ((Frame_Skip < 0 ? 0 : Frame_Skip) + 2)) == 0)
                        Sleep(1);
            }
        }
    }

    return reachedEmulate;
}

#ifdef _DEBUG
void RedirectIOToConsole()
{
    // from "Adding Console I/O to a Win32 GUI App", "Windows Developer Journal, December 1997"
    static const WORD MAX_CONSOLE_LINES = 3000;
    int hConHandle;
    long lStdHandle;
    CONSOLE_SCREEN_BUFFER_INFO coninfo;
    FILE *fp;
    // allocate a console for this app
    AllocConsole();
    // set the screen buffer to be big enough to let us scroll text
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &coninfo);
    coninfo.dwSize.Y = MAX_CONSOLE_LINES;
    SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), coninfo.dwSize);
    // redirect unbuffered STDOUT to the console
    lStdHandle = (long)GetStdHandle(STD_OUTPUT_HANDLE);
    hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);
    fp = _fdopen(hConHandle, "w");
    *stdout = *fp;
    setvbuf(stdout, NULL, _IONBF, 0);
    // redirect unbuffered STDIN to the console
    lStdHandle = (long)GetStdHandle(STD_INPUT_HANDLE);
    hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);
    fp = _fdopen(hConHandle, "r");
    *stdin = *fp;
    setvbuf(stdin, NULL, _IONBF, 0);
    // redirect unbuffered STDERR to the console
    lStdHandle = (long)GetStdHandle(STD_ERROR_HANDLE);
    hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);
    fp = _fdopen(hConHandle, "w");
    *stderr = *fp;
    setvbuf(stderr, NULL, _IONBF, 0);
    // make cout, wcout, cin, wcin, wcerr, cerr, wclog and clog
    // point to console as well
    std::ios::sync_with_stdio();
}
#endif

#ifdef DEBUG_68K
#define BREAKPOINTS_BASE 0x00D00000
extern bool IsHardwareAddressValid(unsigned int address);
extern bool WriteValueAtHardwareAddress(unsigned int address, unsigned int value, unsigned int size, bool hookless = false);
#endif

class DbgClientClient {
public:
  DbgClientClient(std::shared_ptr<Channel> channel)
    : stub_(DbgClient::NewStub(channel)) {}

  bool stop_event(const std::map<uint32_t, uint32_t>& changed) {
    Map<google::protobuf::uint32, google::protobuf::uint32> _changed(changed.cbegin(), changed.cend());

    Changed req;
    *req.mutable_changed() = _changed;

    Empty resp;

    ClientContext context;
    Status status = stub_->stop_event(&context, req, &resp);

    return status.ok();
  }

  bool pause_event(uint32_t pc, const std::map<uint32_t, uint32_t>& changed) {
    Map<google::protobuf::uint32, google::protobuf::uint32> _changed(changed.cbegin(), changed.cend());

    PauseChanged req;
    req.set_address(pc);
    *req.mutable_changed() = _changed;

    Empty resp;

    ClientContext context;
    Status status = stub_->pause_event(&context, req, &resp);

    return status.ok();
  }

  bool start_event() {
    Empty req;
    Empty resp;

    ClientContext context;
    Status status = stub_->start_event(&context, req, &resp);

    return status.ok();
  }

private:
  std::unique_ptr<DbgClient::Stub> stub_;
};

static std::unique_ptr<DbgClientClient> client;

void send_pause_event(int pc, std::map<uint32_t, uint32_t> changed) {
  if (!client) {
    return;
  }
  
  client->pause_event(pc, changed);
}

void stop_client() {
  if (!client) {
    return;
  }

#ifdef DEBUG_68K
  client->stop_event(M68kDW.changed);
  M68kDW.changed.clear();
#else
  client->stop_event(Z80DW.changed);
  Z80DW.changed.clear();
#endif
}

static void init_ida_client() {
  auto channel = grpc::CreateChannel("localhost:9091", grpc::InsecureChannelCredentials());

  while (channel->GetState(true) != GRPC_CHANNEL_READY) {
    Sleep(10);
  }

  client = std::unique_ptr<DbgClientClient>(new DbgClientClient(channel));

  atexit(stop_client);
}

static inline unsigned int mask(unsigned char bit_idx, unsigned char bits_cnt = 1) {
  return (((1 << bits_cnt) - 1) << bit_idx);
}

static void toggle_pause() {
  if (Paused) {
    Paused = 0;
  }
  else {
    Paused = 1;
    Pause_Screen();
    Clear_Sound_Buffer();
    Flip(HWnd);

    if (!client) {
      return;
    }

#ifdef DEBUG_68K
    client->pause_event(M68kDW.last_pc, M68kDW.changed);
    M68kDW.changed.clear();
#else
    client->pause_event(Z80DW.last_pc, Z80DW.changed);
    Z80DW.changed.clear();
#endif
  }
}

class DbgServerHandler final : public DbgServer::Service {
  Status add_breakpoint(ServerContext* context, const DbgBreakpoint* request, Empty* response) override {
#ifdef DEBUG_68K
    Breakpoint b((bp_type)request->type(), request->bstart() & 0xFFFFFF, request->bend() & 0xFFFFFF, true, request->is_vdp(), false);
    b.try_to_compile_condition(request->condition());
    M68kDW.Breakpoints.push_back(b);
#else
    Breakpoint b((bp_type)request->type(), request->bstart() & 0xFFFFFF, request->bend() & 0xFFFFFF, true, false);
    Z80DW.Breakpoints.push_back(b);
#endif

    return Status::OK;
  }

  Status clear_breakpoints(ServerContext* context, const Empty* request, Empty* response) override {
#ifdef DEBUG_68K
    M68kDW.Breakpoints.clear();
#else
    Z80DW.Breakpoints.clear();
#endif

    return Status::OK;
  }

  Status del_breakpoint(ServerContext* context, const DbgBreakpoint* request, Empty* response) override {
#ifdef DEBUG_68K
    for (auto i = M68kDW.Breakpoints.begin(); i != M68kDW.Breakpoints.end(); ++i) {
#else
    for (auto i = Z80DW.Breakpoints.begin(); i != Z80DW.Breakpoints.end(); ++i) {
#endif
      if (request->type() == (BpType)i->type && request->bstart() == i->start) {
#ifdef DEBUG_68K
        if (request->is_vdp() == i->is_vdp) {
          M68kDW.Breakpoints.erase(i);
          break;
        }
#else
        Z80DW.Breakpoints.erase(i);
        break;
#endif
      }
    }

    return Status::OK;
  }

#ifdef DEBUG_Z80
  Status get_sound_banks(ServerContext * context, const Empty * request, SoundBankMap * response) override {
    response->clear_range();

    for (auto i = z80_banks.cbegin(); i != z80_banks.cend(); ++i) {
      SoundBankRange bnk;
      bnk.set_bank_min(i->second.bank_min);
      bnk.set_bank_max(i->second.bank_max);

      (*response->mutable_range())[i->first] = bnk;
    }

    return Status::OK;
  }
#endif

  Status exit_emulation(ServerContext* context, const Empty* request, Empty* response) override {
    if (!client) {
      return Status::CANCELLED;
    }

#ifdef DEBUG_68K
    client->stop_event(M68kDW.changed);
#else
    client->stop_event(Z80DW.changed);
#endif

    SendMessageA(HWnd, WM_CLOSE, 0, 0);

    return Status::OK;
  }

  Status get_breakpoints(ServerContext* context, const Empty* request, DbgBreakpoints* response) override {
#ifdef DEBUG_68K
    for (auto i = M68kDW.Breakpoints.cbegin(); i != M68kDW.Breakpoints.cend(); ++i) {
#else
    for (auto i = Z80DW.Breakpoints.cbegin(); i != Z80DW.Breakpoints.cend(); ++i) {
#endif
      DbgBreakpoint* bpt = response->add_list();
      bpt->set_enabled(i->enabled);
#ifdef DEBUG_68K
      bpt->set_is_vdp(i->is_vdp);
#endif
      bpt->set_is_forbid(i->is_forbid);
      bpt->set_bstart(i->start);
      bpt->set_bend(i->end);
      bpt->set_type((BpType)i->type);
    }

    return Status::OK;
  }

  Status get_callstack(ServerContext* context, const Empty* request, Callstack* response) override {
    response->clear_callstack();
#ifdef DEBUG_68K
    for (auto i = M68kDW.callstack.cbegin(); i != M68kDW.callstack.cend(); ++i) {
#else
    for (auto i = Z80DW.callstack.cbegin(); i != Z80DW.callstack.cend(); ++i) {
#endif
      response->add_callstack(*i);
    }

    return Status::OK;
  }

#ifdef DEBUG_68K
  Status get_dma_info(ServerContext* context, const Empty* request, DmaInfo* response) override {
    response->set_len((BYTE)(VDP_Reg.regs[VdpRegsEnum::V13]) | ((BYTE)(VDP_Reg.regs[VdpRegsEnum::V14]) << 8));
    
    uint32_t src = (BYTE)(VDP_Reg.regs[VdpRegsEnum::V15]) | ((BYTE)(VDP_Reg.regs[VdpRegsEnum::V16]) << 8);

    UINT16 dma_high = VDP_Reg.regs[VdpRegsEnum::V17];
    if (!(dma_high & 0x80)) {
      src |= ((BYTE)(VDP_Reg.regs[VdpRegsEnum::V17] & mask(0, 7)) << 16);
    } else {
      src |= ((BYTE)(VDP_Reg.regs[VdpRegsEnum::V17] & mask(0, 6)) << 16);
    }
    src <<= 1;
    src &= 0xFFFFFF;

    response->set_src(src);

    uint32_t dst = BREAKPOINTS_BASE;
    switch (Ctrl.Access) {
    case 0x09: // VRAM
    case 0x0A: // CRAM
    case 0x0B: // VSRAM
      dst = (BREAKPOINTS_BASE + 0x10000 * (Ctrl.Access - 0x09)) + (Ctrl.Address & 0xFFFF);
      break;
    }

    response->set_dst(dst);

    return Status::OK;
  }
#endif

  Status get_gp_reg(ServerContext* context, const GpReg* request, AnyRegValue* response) override {
#ifdef DEBUG_68K
    if (request->reg() >= GpRegsEnum::D0 && request->reg() <= GpRegsEnum::D7) { // Dx
      response->set_value(main68k_context.dreg[(int)request->reg()]);
      return Status::OK;
    }
    else if (request->reg() >= GpRegsEnum::A0 && request->reg() <= GpRegsEnum::A7) { // Ax
      response->set_value(main68k_context.areg[request->reg()-GpRegsEnum::A0]);
      return Status::OK;
    }
    else {
      switch (request->reg()) {
      case GpRegsEnum::PC: {
        response->set_value(main68k_context.pc & 0xFFFFFF);
        return Status::OK;
      }
      case GpRegsEnum::SR: {
        response->set_value(main68k_context.sr);
        return Status::OK;
      }
      case GpRegsEnum::SP: {
        response->set_value(main68k_context.areg[GpRegsEnum::A7-GpRegsEnum::A0]);
        return Status::OK;
      }
      }
    }
#else
    switch (request->reg()) {
    case GpRegsEnum::AF: {
      response->set_value((M_Z80.AF.b.A << 8) | (M_Z80.AF.b.F));
      return Status::OK;
    }
    case GpRegsEnum::BC: {
      response->set_value(M_Z80.BC.w.BC);
      return Status::OK;
    }
    case GpRegsEnum::DE: {
      response->set_value(M_Z80.DE.w.DE);
      return Status::OK;
    }
    case GpRegsEnum::HL: {
      response->set_value(M_Z80.HL.w.HL);
      return Status::OK;
    }
    case GpRegsEnum::IX: {
      response->set_value(M_Z80.IX.w.IX);
      return Status::OK;
    }
    case GpRegsEnum::IY: {
      response->set_value(M_Z80.IY.w.IY);
      return Status::OK;
    }

    case GpRegsEnum::A: {
      response->set_value(M_Z80.AF.b.A);
      return Status::OK;
    }
    case GpRegsEnum::B: {
      response->set_value(M_Z80.BC.b.B);
      return Status::OK;
    }
    case GpRegsEnum::C: {
      response->set_value(M_Z80.BC.b.C);
      return Status::OK;
    }
    case GpRegsEnum::D: {
      response->set_value(M_Z80.DE.b.D);
      return Status::OK;
    }
    case GpRegsEnum::E: {
      response->set_value(M_Z80.DE.b.E);
      return Status::OK;
    }
    case GpRegsEnum::H: {
      response->set_value(M_Z80.HL.b.H);
      return Status::OK;
    }
    case GpRegsEnum::L: {
      response->set_value(M_Z80.HL.b.L);
      return Status::OK;
    }

    case GpRegsEnum::IXH: {
      response->set_value(M_Z80.IX.b.IXH);
      return Status::OK;
    }
    case GpRegsEnum::IXL: {
      response->set_value(M_Z80.IX.b.IXL);
      return Status::OK;
    }

    case GpRegsEnum::IYH: {
      response->set_value(M_Z80.IY.b.IYH);
      return Status::OK;
    }
    case GpRegsEnum::IYL: {
      response->set_value(M_Z80.IY.b.IYL);
      return Status::OK;
    }

    case GpRegsEnum::AF2: {
      response->set_value((M_Z80.AF2.b.A2 << 8) | (M_Z80.AF2.b.F2));
      return Status::OK;
    }
    case GpRegsEnum::BC2: {
      response->set_value(M_Z80.BC2.w.BC2);
      return Status::OK;
    }
    case GpRegsEnum::DE2: {
      response->set_value(M_Z80.DE2.w.DE2);
      return Status::OK;
    }
    case GpRegsEnum::HL2: {
      response->set_value(M_Z80.HL2.w.HL2);
      return Status::OK;
    }

    case GpRegsEnum::I: {
      response->set_value(M_Z80.I);
      return Status::OK;
    }
    case GpRegsEnum::R: {
      response->set_value(M_Z80.R.w.R);
      return Status::OK;
    }

    case GpRegsEnum::SP: {
      response->set_value(M_Z80.SP.w.SP);
      return Status::OK;
    }
    case GpRegsEnum::IP: {
      response->set_value(Z80DW.last_pc);
      return Status::OK;
    }

    case GpRegsEnum::BANK: {
      response->set_value(Bank_Z80);
      return Status::OK;
    }
    }
#endif

    return Status::OK;
  }

  Status get_gp_regs(ServerContext* context, const Empty* request, GpRegs* response) override {
#ifdef DEBUG_68K
    response->set_d0(main68k_context.dreg[0]);
    response->set_d1(main68k_context.dreg[1]);
    response->set_d2(main68k_context.dreg[2]);
    response->set_d3(main68k_context.dreg[3]);
    response->set_d4(main68k_context.dreg[4]);
    response->set_d5(main68k_context.dreg[5]);
    response->set_d6(main68k_context.dreg[6]);
    response->set_d7(main68k_context.dreg[7]);

    response->set_a0(main68k_context.areg[0]);
    response->set_a1(main68k_context.areg[1]);
    response->set_a2(main68k_context.areg[2]);
    response->set_a3(main68k_context.areg[3]);
    response->set_a4(main68k_context.areg[4]);
    response->set_a5(main68k_context.areg[5]);
    response->set_a6(main68k_context.areg[6]);
    response->set_a7(main68k_context.areg[7]);
    response->set_sp(main68k_context.areg[7]);

    response->set_pc(M68kDW.last_pc & 0xFFFFFF);
    response->set_sr(main68k_context.sr);
#else
    response->set_af((M_Z80.AF.b.A << 8) | M_Z80.AF.b.F);
    response->set_bc(M_Z80.BC.w.BC);
    response->set_de(M_Z80.DE.w.DE);
    response->set_hl(M_Z80.HL.w.HL);

    response->set_ix(M_Z80.IX.w.IX);
    response->set_iy(M_Z80.IY.w.IY);

    response->set_a(M_Z80.AF.b.A);
    response->set_b(M_Z80.BC.b.B);
    response->set_c(M_Z80.BC.b.C);
    response->set_d(M_Z80.DE.b.D);
    response->set_e(M_Z80.DE.b.E);
    response->set_h(M_Z80.HL.b.H);
    response->set_l(M_Z80.HL.b.L);

    response->set_ixh(M_Z80.IX.b.IXH);
    response->set_ixl(M_Z80.IX.b.IXL);
    response->set_iyh(M_Z80.IY.b.IYH);
    response->set_iyl(M_Z80.IY.b.IYL);


    response->set_af2((M_Z80.AF2.b.A2 << 8) | M_Z80.AF2.b.F2);
    response->set_bc2(M_Z80.BC2.w.BC2);
    response->set_de2(M_Z80.DE2.w.DE2);
    response->set_hl2(M_Z80.HL2.w.HL2);

    response->set_i(M_Z80.I);
    response->set_r(M_Z80.R.w.R);

    response->set_sp(M_Z80.SP.w.SP);
    response->set_ip(Z80DW.last_pc);

    response->set_bank(Bank_Z80);
#endif

    return Status::OK;
  }

#ifdef DEBUG_68K
  Status get_vdp_reg(ServerContext* context, const VdpReg* request, AnyRegValue* response) override {
    if (request->reg() >= VdpRegsEnum::V00 && request->reg() <= VdpRegsEnum::V17) {
      response->set_value(VDP_Reg.regs[(int)request->reg()]);
      return Status::OK;
    }

    return Status::OK;
  }

  Status get_vdp_regs(ServerContext* context, const Empty* request, VdpRegs* response) override {
    response->set_v00(VDP_Reg.regs[0]);
    response->set_v01(VDP_Reg.regs[1]);
    response->set_v02(VDP_Reg.regs[2]);
    response->set_v03(VDP_Reg.regs[3]);
    response->set_v04(VDP_Reg.regs[4]);
    response->set_v05(VDP_Reg.regs[5]);
    response->set_v06(VDP_Reg.regs[6]);
    response->set_v07(VDP_Reg.regs[7]);
    response->set_v08(VDP_Reg.regs[8]);
    response->set_v09(VDP_Reg.regs[9]);
    response->set_v0a(VDP_Reg.regs[10]);
    response->set_v0b(VDP_Reg.regs[11]);
    response->set_v0c(VDP_Reg.regs[12]);
    response->set_v0d(VDP_Reg.regs[13]);
    response->set_v0e(VDP_Reg.regs[14]);
    response->set_v0f(VDP_Reg.regs[15]);
    response->set_v10(VDP_Reg.regs[16]);
    response->set_v11(VDP_Reg.regs[17]);
    response->set_v12(VDP_Reg.regs[18]);
    response->set_v13(VDP_Reg.regs[19]);
    response->set_v14(VDP_Reg.regs[20]);
    response->set_v15(VDP_Reg.regs[21]);
    response->set_v16(VDP_Reg.regs[22]);
    response->set_v17(VDP_Reg.regs[23]);

    return Status::OK;
  }
#endif

  Status pause(ServerContext* context, const Empty* request, Empty* response) override {
#ifdef DEBUG_68K
    M68kDW.DebugStop = true;
#else
    Z80DW.DebugStop = true;
#endif

    if (Paused) {
      return Status::OK;
    }

    toggle_pause();

    return Status::OK;
  }

  Status read_memory(ServerContext* context, const MemoryAS* request, MemData* response) override {
    std::string* _return = response->mutable_data();
    _return->clear();

    for (uint32_t i = 0; i < request->size(); ++i) {
#ifdef DEBUG_68K
      if ((request->address() + i >= 0xA00000 && request->address() + i < 0xA10000) && IsHardwareAddressValid((uint32)(request->address() + i))) {
        // Z80
        unsigned char value = (unsigned char)(ReadValueAtHardwareAddress((uint32)((request->address() + i) ^ 1), 1) & 0xFF);
        _return->push_back(value);
      }
      else if (IsHardwareAddressValid((uint32)(request->address() + i))) {
        unsigned char value = (unsigned char)(ReadValueAtHardwareAddress((uint32)(request->address() + i), 1) & 0xFF);
        _return->push_back(value);
      }
      else if (request->address() + i >= (BREAKPOINTS_BASE + 0x00000) && request->address() + i < (BREAKPOINTS_BASE + 0x10000)) {
        // VRAM
        unsigned int addr = request->address() + i - (BREAKPOINTS_BASE + 0x00000);
        _return->push_back(VRam[(addr ^ 1) & 0xFFFF]);
      }
      else if (request->address() + i >= (BREAKPOINTS_BASE + 0x10000) && request->address() + i < (BREAKPOINTS_BASE + 0x20000)) {
        // CRAM
        unsigned int addr = request->address() + i - (BREAKPOINTS_BASE + 0x10000);
        _return->push_back(((UINT8*)CRam)[(addr ^ 1) & 0x1FF]);
      }
      else if (request->address() + i >= (BREAKPOINTS_BASE + 0x20000) && request->address() + i < (BREAKPOINTS_BASE + 0x30000)) {
        // VSRAM
        unsigned int addr = request->address() + i - (BREAKPOINTS_BASE + 0x20000);
        _return->push_back(((UINT8*)VSRam)[(addr ^ 1) & 0xFF]);
      }
      else { // else leave the value nil
        _return->push_back('\x00');
      }
#else
      _return->push_back(Ram_Z80[request->address() + i]);
#endif
    }

    return Status::OK;
  }

  Status resume(ServerContext* context, const Empty* request, Empty* response) override {
#ifdef DEBUG_68K
    M68kDW.DebugStop = false;
#else
    Z80DW.DebugStop = false;
#endif

    if (!Paused) {
      return Status::OK;
    }

    toggle_pause();

    return Status::OK;
  }

  Status set_gp_reg(ServerContext* context, const GpRegValue* request, Empty* response) override {
#ifdef DEBUG_68K
    if (request->index() >= GpRegsEnum::D0 && request->index() <= GpRegsEnum::D7) { // Dx
      main68k_context.dreg[(int)request->index()] = request->value();
    }
    else if (request->index() >= GpRegsEnum::A0 && request->index() <= GpRegsEnum::A7) { // Ax
      switch (request->index()) {
      case GpRegsEnum::A0:
        main68k_context.areg[0] = request->value();
        break;
      case GpRegsEnum::A1:
        main68k_context.areg[1] = request->value();
        break;
      case GpRegsEnum::A2:
        main68k_context.areg[2] = request->value();
        break;
      case GpRegsEnum::A3:
        main68k_context.areg[3] = request->value();
        break;
      case GpRegsEnum::A4:
        main68k_context.areg[4] = request->value();
        break;
      case GpRegsEnum::A5:
        main68k_context.areg[5] = request->value();
        break;
      case GpRegsEnum::A6:
        main68k_context.areg[6] = request->value();
        break;
      case GpRegsEnum::A7:
        main68k_context.areg[7] = request->value();
        break;
      }
    }
    else {
      switch (request->index()) {
      case GpRegsEnum::SR:
        main68k_context.sr = request->value() & 0xFFFF;
        break;
      case GpRegsEnum::SP:
        main68k_context.areg[7] = request->value();
        break;
      }
    }
#else
    switch (request->index()) {
    case GpRegsEnum::AF: {
      M_Z80.AF.b.A = (request->value() >> 8) & 0xFF;
      M_Z80.AF.b.F = (request->value() >> 0) & 0xFF;
    } break;
    case GpRegsEnum::BC: M_Z80.BC.w.BC = request->value(); break;
    case GpRegsEnum::DE: M_Z80.DE.w.DE = request->value(); break;
    case GpRegsEnum::HL: M_Z80.HL.w.HL = request->value(); break;

    case GpRegsEnum::IX: M_Z80.IX.w.IX = request->value(); break;
    case GpRegsEnum::IY: M_Z80.IY.w.IY = request->value(); break;

    case GpRegsEnum::A: M_Z80.AF.b.A = request->value(); break;
    case GpRegsEnum::B: M_Z80.BC.b.B = request->value(); break;
    case GpRegsEnum::C: M_Z80.BC.b.C = request->value(); break;
    case GpRegsEnum::D: M_Z80.DE.b.D = request->value(); break;
    case GpRegsEnum::E: M_Z80.DE.b.E = request->value(); break;
    case GpRegsEnum::H: M_Z80.HL.b.H = request->value(); break;
    case GpRegsEnum::L: M_Z80.HL.b.L = request->value(); break;

    case GpRegsEnum::IXH: M_Z80.IX.b.IXH = request->value(); break;
    case GpRegsEnum::IXL: M_Z80.IX.b.IXL = request->value(); break;
    case GpRegsEnum::IYH: M_Z80.IY.b.IYH = request->value(); break;
    case GpRegsEnum::IYL: M_Z80.IY.b.IYL = request->value(); break;


    case GpRegsEnum::AF2: {
      M_Z80.AF2.b.A2 = (request->value() >> 8) & 0xFF;
      M_Z80.AF2.b.F2 = (request->value() >> 0) & 0xFF;
    } break;
    case GpRegsEnum::BC2: M_Z80.BC2.w.BC2 = request->value(); break;
    case GpRegsEnum::DE2: M_Z80.DE2.w.DE2 = request->value(); break;
    case GpRegsEnum::HL2: M_Z80.HL2.w.HL2 = request->value(); break;

    case GpRegsEnum::I: M_Z80.I = request->value(); break;
    case GpRegsEnum::R: M_Z80.R.w.R = request->value(); break;

    case GpRegsEnum::SP: M_Z80.SP.w.SP = request->value(); break;
    case GpRegsEnum::IP: Z80DW.last_pc = request->value(); break;

    case GpRegsEnum::BANK: Bank_Z80 = request->value(); break;
    }
#endif

    return Status::OK;
  }

#ifdef DEBUG_68K
  Status set_vdp_reg(ServerContext* context, const VdpRegValue* request, Empty* response) override {
    if (request->index() >= VdpRegsEnum::V00 && request->index() <= VdpRegsEnum::V17) {
      VDP_Reg.regs[request->index()] = request->value();
    }

    return Status::OK;
  }
#endif

  Status start_emulation(ServerContext* context, const Empty* request, Empty* response) override {
    init_ida_client();

    if (!client) {
      return Status::CANCELLED;
    }

    client->start_event();
#ifdef DEBUG_68K
    M68kDW.changed.clear();
    client->pause_event(main68k_context.pc, M68kDW.changed);
#else
    Z80DW.changed.clear();
    client->pause_event(M_Z80.PC.w.PC, Z80DW.changed);
#endif

    return Status::OK;
  }

  Status step_into(ServerContext* context, const Empty* request, Empty* response) override {
    Paused = 0;
#ifdef DEBUG_68K
    M68kDW.StepInto = 1;
    M68kDW.DebugStop = false;
#else
    Z80DW.StepInto = 1;
    Z80DW.DebugStop = false;
#endif

    return Status::OK;
  }

  Status step_over(ServerContext* context, const Empty* request, Empty* response) override {
    Paused = 0;
#ifdef DEBUG_68K
    M68kDW.DoStepOver();
    M68kDW.DebugStop = false;
#else
    Z80DW.DoStepOver();
    Z80DW.DebugStop = false;
#endif

    return Status::OK;
  }

  Status toggle_breakpoint(ServerContext* context, const DbgBreakpoint* request, Empty* response) override {
#ifdef DEBUG_68K
    for (auto i = M68kDW.Breakpoints.begin(); i != M68kDW.Breakpoints.end(); ++i) {
#else
    for (auto i = Z80DW.Breakpoints.begin(); i != Z80DW.Breakpoints.end(); ++i) {
#endif
      if (request->type() == (BpType)i->type && request->bstart() == i->start) {
#ifdef DEBUG_68K
        if (request->is_vdp() == i->is_vdp) {
#endif
          i->enabled = !i->enabled;
          break;
#ifdef DEBUG_68K
      }
#endif
      }
    }

    return Status::OK;
  }

  Status update_breakpoint(ServerContext* context, const DbgBreakpoint* request, Empty* response) override {
#ifdef DEBUG_68K
    for (auto i = M68kDW.Breakpoints.begin(); i != M68kDW.Breakpoints.end(); ++i) {
#else
    for (auto i = Z80DW.Breakpoints.begin(); i != Z80DW.Breakpoints.end(); ++i) {
#endif
      if (request->type() == (BpType)i->type && request->bstart() == i->start) {
#ifdef DEBUG_68K
        if (request->is_vdp() == i->is_vdp) {
#endif
          i->enabled = request->enabled();
          i->is_forbid = request->is_forbid();
          i->try_to_compile_condition(request->condition());
          break;
#ifdef DEBUG_68K
      }
#endif
      }
    }

    return Status::OK;
  }

  Status write_memory(ServerContext* context, const MemoryAD* request, Empty* response) override {
    for (size_t i = 0; i < request->data().size(); ++i) {
#ifdef DEBUG_68K
      if ((request->address() + i >= 0xA00000 && request->address() + i < 0xA10000) && IsHardwareAddressValid((uint32)(request->address() + i))) { // Z80
        WriteValueAtHardwareAddress((uint32)((request->address() + i) ^ 1), request->data()[i] & 0xFF, true);
      }
      else if (IsHardwareAddressValid((uint32)(request->address() + i))) {
        WriteValueAtHardwareAddress((uint32)(request->address() + i), request->data()[i] & 0xFF, true);
      }
      else if (request->address() + i >= (BREAKPOINTS_BASE + 0x00000) && request->address() + i < (BREAKPOINTS_BASE + 0x10000)) { // VRAM
        unsigned int addr = request->address() + i - (BREAKPOINTS_BASE + 0x00000);
        VRam[(addr ^ 1) & 0xFFFF] = request->data()[i] & 0xFF;
      }
      else if (request->address() + i >= (BREAKPOINTS_BASE + 0x10000) && request->address() + i < (BREAKPOINTS_BASE + 0x20000)) { // CRAM
        unsigned int addr = request->address() + i - (BREAKPOINTS_BASE + 0x10000);
        ((UINT8*)CRam)[(addr ^ 1) & 0x1FF] = request->data()[i] & 0xFF;
      }
      else if (request->address() + i >= (BREAKPOINTS_BASE + 0x20000) && request->address() + i < (BREAKPOINTS_BASE + 0x30000)) { // VSRAM
        unsigned int addr = request->address() + i - (BREAKPOINTS_BASE + 0x20000);
        ((UINT8*)VSRam)[(addr ^ 1) & 0xFF] = request->data()[i] & 0xFF;
      }
#else
      Ram_Z80[request->address() + i] = request->data()[i] & 0xFF;
#endif
    }

    return Status::OK;
  }
};

static std::unique_ptr<Server> server;

static void stop_server() {
  server->Shutdown(std::chrono::system_clock::now() + std::chrono::milliseconds(100));
}

static void DbgServerFunc() {
  std::string server_address("0.0.0.0:9090");
  DbgServerHandler service;

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  server = builder.BuildAndStart();

  server->Wait();
}

static void init_dbg_server() {
  std::thread t1(DbgServerFunc);
  t1.detach();

  atexit(stop_server);

#ifdef DEBUG_68K
  M68kDW.Breakpoints.clear();
  Breakpoint b(bp_type::BP_PC, main68k_context.pc & 0xFFFFFF, main68k_context.pc & 0xFFFFFF, true, false, false);
  M68kDW.Breakpoints.push_back(b);
#else
  Z80DW.Breakpoints.clear();
  Breakpoint b(bp_type::BP_PC, 0, 0, true, false);
  Z80DW.Breakpoints.push_back(b);
#endif

  //Paused = 1;
  //Pause_Screen();
  //Clear_Sound_Buffer();
  //Flip(HWnd);

  MESSAGE_L("Waiting for IDA connection");
}

int PASCAL WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nCmdShow) //note: nCmdShow is always one indicating that lpCmdLine contains something (even when it doesn't)
{
    ///////////////////////////////////////////////////

    SetErrorMode(SEM_FAILCRITICALERRORS); // Modif N. -- prevents "There is no disk in the drive." error spam if a file on a removed disk is in the recent ROMs list

#ifdef _DEBUG
    // make it possible to see stdout/stderr output
    // so we can do things like use printf to debug,
    // see the mp3 decoder's error messages, etc.
    // ...I'd rather redirect stdout/stderr to OutputDebugString() though
    // but I can't figure out how to do that so this is the next best thing
    //RedirectIOToConsole();
#endif

    ///////////////////////////////////////////////////

    MSG msg;

    InitMovie(&MainMovie);

    Init(hInst, nCmdShow);

    // Have to do it *before* load by command line
    Init_Genesis_Bios();

    if (lpCmdLine[0])ParseCmdLine(lpCmdLine, HWnd);

    DestroyWindow(RamSearchHWnd); RamSearchHWnd = NULL; // modeless dialog
    DestroyWindow(RamWatchHWnd); RamWatchHWnd = NULL; // modeless dialog
    DestroyWindow(PlaneExplorerHWnd); PlaneExplorerHWnd = NULL; // modeless dialog
    DestroyWindow(VDPRamHWnd); VDPRamHWnd = NULL; // modeless dialog
#ifdef DEBUG_Z80
    DestroyWindow(YM2612DbgHWnd); YM2612DbgHWnd = NULL; // modeless dialog
#endif
    DestroyWindow(VDPSpritesHWnd); VDPSpritesHWnd = NULL; // modeless dialog
    DestroyWindow(RamCheatHWnd); RamCheatHWnd = NULL; // modeless dialog

    for (size_t i = 0; i < LuaScriptHWnds.size(); ++i)
    {
        DestroyWindow(LuaScriptHWnds[i]);
        LuaScriptHWnds[i] = NULL;
    }
    LuaScriptHWnds.clear();

    DestroyWindow(VolControlHWnd); VolControlHWnd = NULL;

    while (Gens_Running)
    {
        Handle_Gens_Messages();

        if (Game)
        {
            Check_Misc_Key();
            //Update_Input(); // disabled because we just called it in Handle_Gens_Messages

            if (MustUpdateMenu)
            {
                Build_Main_Menu();
                MustUpdateMenu = 0;
            }

            int frameAdvanceKeyJustReleased = ((GetActiveWindow() == HWnd) || BackgroundInput) ? Check_Skip_Key_Released() : 0;
            static int frameAdvanceKeyWasJustPressed = 0;
            bool skipOptionEnabled = frameadvSkipLag;
            bool frameadvSkipLag = skipOptionEnabled && !frameadvSkipLagForceDisable;
            TurboMode = TurboToggle || (FastForwardKeyDown && (GetActiveWindow() == HWnd || BackgroundInput));

            if (!(frameadvSkipLag && skipLagNow && !Paused))
            {
                skipLagNow = false;

                if (frameadvSkipLag && (frameAdvanceKeyJustReleased || frameAdvanceKeyWasJustPressed) && !lastFrameAdvancePaused && frameadvSkipLag_Rewind_State_Buffer_Valid)
                {
                    // activate lag skip checking
                    skipLagNow = true;
                    Paused = 0; // so the pause button can re-pause while skipping lag frames
                }
                else if (((GetActiveWindow() == HWnd) || BackgroundInput) && Check_Skip_Key())
                {
                    // handle frame advance key
                    if (Paused)
                    {
                        if (frameadvSkipLag)
                        {
                            frameadvSkipLag_Rewind_Input_Buffer[frameadvSkipLag_Rewind_State_Buffer_Index % 2] = GetLastInputCondensed();
                            Save_State_To_Buffer(frameadvSkipLag_Rewind_State_Buffer[frameadvSkipLag_Rewind_State_Buffer_Index++ % 2]);
                            frameadvSkipLag_Rewind_State_Buffer_Valid = true;
                        }

                        Update_Emulation_One(HWnd);
                        soundCleared = false;
                        tgtime = timeGetTime();
                        lastFrameAdvancePaused = false;
                    }
                    else
                    {
                        // if the game isn't paused yet then the first press of the frame advance key only pauses the game
                        Paused = 1;
                        Clear_Sound_Buffer();
                        soundCleared = true;
                        lastFrameAdvancePaused = true;
                    }
                }
                else // normal emulation
                {
                    if ((Active) && (!Paused))  // EMULATION
                    {
                        Update_Emulation(HWnd);
                    }
                    else        // EMULATION PAUSED
                    {
                        if (!soundCleared && timeGetTime() - tgtime >= 125) //eliminate stutter
                        {
                            Clear_Sound_Buffer();
                            soundCleared = true;
                        }
                    }

                    //Modif N - don't hog 100% of CPU power:
                    // (this is in addition to a sleep of Sleep_Time elsewhere, because sometimes that code is not running)
                    if (Sleep_Time)
                    {
                        if (Paused)
                        {
                            Sleep(1);
                        }
                        else
                        {
                            static int count = 0;
                            count++;
                            if (!TurboMode)
                                if ((count % ((Frame_Skip < 0 ? 0 : Frame_Skip) + 2)) == 0)
                                    Sleep(1);
                        }
                    }
                }
            }
            else //if(frameadvSkipLag && skipLagNow && !Paused)
            {
                if (FrameCount == 0 || !frameadvSkipLag_Rewind_State_Buffer_Valid)
                {
                    // this case is to interrupt the auto-frame skipping if the game or movie is reset or reloaded
                    Paused = 1;
                    skipLagNow = false;
                    frameadvSkipLag_Rewind_State_Buffer_Valid = false;
                }
                else
                {
                    // perform lag skip checking

                    frameadvSkipLag_Rewind_Input_Buffer[frameadvSkipLag_Rewind_State_Buffer_Index % 2] = GetLastInputCondensed();
                    SetNextInputCondensed(frameadvSkipLag_Rewind_Input_Buffer[frameadvSkipLag_Rewind_State_Buffer_Index % 2]); // to reduce user confusion, this line prevents the input display from changing more than once per frame advance by applying the initially accepted input to all following auto-skipped lag frames
                    Save_State_To_Buffer(frameadvSkipLag_Rewind_State_Buffer[frameadvSkipLag_Rewind_State_Buffer_Index++ % 2]);
                    frameadvSkipLag_Rewind_State_Buffer_Valid = true;

                    // update the graphics in case they're changing during non-input frames
                    int Temp_Frame_Skip = Frame_Skip;
                    if (TurboMode)
                        Temp_Frame_Skip = 8;
                    if (Lag_Frame && Frame_Number + 1 >= Temp_Frame_Skip)
                        Do_VDP_Refresh(); // better than nothing for showing skipped frames

                    Update_Emulation_One_Before(HWnd);
                    Update_Frame_Fast();
                    soundCleared = false;
                    tgtime = timeGetTime();
                    if (Lag_Frame)
                    {
                        // keep going, because the frame ignored user input
                        Update_Emulation_After_Fast(HWnd);
                    }
                    else
                    {
                        // stop skipping, the next frame will accept user input
#if 0
                        // works most of the time but certain cases won't look right
                        Do_VDP_Only();
#else
                        // re-run the last frame to generate its graphics properly
                        Load_State_From_Buffer(frameadvSkipLag_Rewind_State_Buffer[frameadvSkipLag_Rewind_State_Buffer_Index % 2]);
                        SetNextInputCondensed(frameadvSkipLag_Rewind_Input_Buffer[(frameadvSkipLag_Rewind_State_Buffer_Index + 1) % 2]); // being careful to re-run the last frame with the same input as before
                        Update_Emulation_One(HWnd);
#endif
                        Paused = 1;
                        skipLagNow = false;
                        frameadvSkipLag_Rewind_State_Buffer_Valid = false;
                    }
                }
            }

            frameAdvanceKeyWasJustPressed = ((GetActiveWindow() == HWnd) || BackgroundInput) ? Check_Skip_Key_Pressed() : 0;
        }
        else if (Intro_Style == 1)      // GENS LOGO EFFECT
        {
            Update_Gens_Logo(HWnd);
            Sleep(20);
        }
        else if (Intro_Style == 2)      // STRANGE EFFECT
        {
            Update_Crazy_Effect(HWnd);
            Sleep(20);
        }
        else if (Intro_Style == 3)      // GENESIS BIOS
        {
            Do_Genesis_Frame();
            Flip(HWnd);
            Sleep(20);
        }
        else                            // BLANK SCREEN (MAX IDLE)
        {
            // Modif N. -- reduced sleep time without increasing clear frequency
            // to make non-accelerator hotkeys more responsive when no game is running
            static int clearCounter = 0;
            if (++clearCounter % 10 == 0)
            {
                Clear_Back_Screen(HWnd);
                Flip(HWnd);
            }
            Sleep(20);
        }
    }

    End_Sound(); //Modif N - making sure sound doesn't stutter upon exit

    strcpy(Str_Tmp, Gens_Path);
    strcat(Str_Tmp, "Gens.cfg");
    Save_Config(Str_Tmp);

    End_All(); //Modif N

    ChangeDisplaySettings(NULL, 0);

    DestroyWindow(HWnd);

    while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
    {
        if (!GetMessage(&msg, NULL, 0, 0)) return msg.wParam;
    }

    //TerminateProcess(GetCurrentProcess(), 0);

    return 0;
}

void BeginMoviePlayback()
{
    if (OpenMovieFile(&MainMovie) == 0)
    {
        MESSAGE_L("Error opening file");
            MainMovie.Status = 0;
        return;
    }

    struct Temp { Temp() { EnableStopAllLuaScripts(false); } ~Temp() { EnableStopAllLuaScripts(true); } } dontStopScriptsHere;

    FrameCount = 0;
    LagCount = 0;
    LagCountPersistent = 0;
    frameSearchFrames = -1; frameSearchInitialized = false;
    if (MainMovie.UseState)
    {
        if (MainMovie.Status == MOVIE_PLAYING)
        {
            int t = MainMovie.ReadOnly;
            MainMovie.ReadOnly = 1;
            Load_State(MainMovie.StateName);
            MainMovie.ReadOnly = t;
        }
        else
        {
            Load_State(MainMovie.StateName);
        }
        if (MainMovie.Status == MOVIE_RECORDING)
        {
            Str_Tmp[0] = 0;
            Get_State_File_Name(Str_Tmp);
            Save_State(Str_Tmp);
        }
    }
    else
    {
        // Modif N. reload currently-loaded ROM (more thorough than a Reset, apparently necessary) and clear out SRAM and BRAM
        int wasPaused = Paused;
        if (Genesis_Started)
        {
            Pre_Load_Rom(HWnd, Recent_Rom[0]);
            MESSAGE_L("Genesis reset");
                if (MainMovie.ClearSRAM) memset(SRAM, 0, sizeof(SRAM));
        }
        Paused = wasPaused;
    }
    if (MainMovie.Status == MOVIE_PLAYING && MainMovie.UseState != 0)
    {
        if (CPU_Mode)
            sprintf(Str_Tmp, "Playing from savestate. %d frames. %d rerecords. %d min %2.2f s", MainMovie.LastFrame, MainMovie.NbRerecords, MainMovie.LastFrame / 50 / 60, (MainMovie.LastFrame % 3000) / 60.0);
        else
            sprintf(Str_Tmp, "Playing from savestate. %d frames. %d rerecords. %d min %2.2f s", MainMovie.LastFrame, MainMovie.NbRerecords, MainMovie.LastFrame / 60 / 60, (MainMovie.LastFrame % 3600) / 60.0);
    }
    else if (MainMovie.Status == MOVIE_PLAYING && MainMovie.UseState == 0)
    {
        if (CPU_Mode)
            sprintf(Str_Tmp, "Playing from reset. %d frames. %d rerecords. %d min %2.2f s", MainMovie.LastFrame, MainMovie.NbRerecords, MainMovie.LastFrame / 50 / 60, (MainMovie.LastFrame % 3000) / 60.0);
        else
            sprintf(Str_Tmp, "Playing from reset. %d frames. %d rerecords. %d min %2.2f s", MainMovie.LastFrame, MainMovie.NbRerecords, MainMovie.LastFrame / 60 / 60, (MainMovie.LastFrame % 3600) / 60.0);
    }
    else
        wsprintf(Str_Tmp, "Resuming recording from savestate. Frame %d.", FrameCount);
    Put_Info(Str_Tmp);
    Build_Main_Menu();
    CallRegisteredLuaFunctions(LUACALL_ONSTART);
}

void PlaySubMovie();
int LoadSubMovie(char* filename);
void PutSubMovieErrorInStr_Tmp(int gmiRV, const char* filename, char* header);

const char* GensPlayMovie(const char* filename, bool silent)
{
    //  if(MainMovie.Status==MOVIE_RECORDING) //Modif N - disabled; if the user chose playback, they meant it!
    //      return 0;
    //  if(MainMovie.Status==MOVIE_PLAYING || MainMovie.Status==MOVIE_FINISHED)
    //      CloseMovieFile(&MainMovie);
    if (!(Game))
        SendMessage(HWnd, WM_COMMAND, ID_FILES_OPENROM, 0); // Modif N. -- prompt once to load ROM if it's not already loaded

    // Modif N. -- added so that a movie that's currently being recorded doesn't show up with bogus info in the movie play dialog
    if (MainMovie.Status == MOVIE_RECORDING && MainMovie.File)
        WriteMovieHeader(&MainMovie);

    if (filename == NULL)
    {
        // if no filename was passed in then
        // bring up the movie dialog to choose which movie to play
        MINIMIZE
            DialogsOpen++;
        DialogBox(ghInstance, MAKEINTRESOURCE(IDD_PLAY_MOVIE), HWnd, (DLGPROC)PlayMovieProc);
        if (PlayMovieCanceled)
            return "user cancelled";
    }
    else if (Game)
    {
        char tempfilename[1024];
        strncpy(tempfilename, filename, 1024);
        tempfilename[1023] = 0;

        int gmiRV = LoadSubMovie(tempfilename);
        if (SubMovie.Ok != 0 && (SubMovie.UseState == 0 || SubMovie.StateOk != 0) && (SubMovie.StateRequired == 0 || SubMovie.StateOk != 0))
        {
            SubMovie.ClearSRAM = true;
            SubMovie.ReadOnly = 1;
            PlaySubMovie();
        }
        else
        {
            char header[16];
            memcpy(header, SubMovie.Header, 16);
            header[15] = 0;

            PutSubMovieErrorInStr_Tmp(gmiRV, filename, header);
            return Str_Tmp;
        }
    }

    BeginMoviePlayback();

    return NULL; // success
}
void GensReplayMovie()
{
    if (Game && MainMovie.File)
    {
        FlushMovieFile(&MainMovie); // important if we were recording
        MainMovie.Status = MOVIE_PLAYING;
        BeginMoviePlayback();
    }
}

int GensLoadRom(const char* filename)
{
    Clear_Sound_Buffer();

    int loaded;
    if (!filename)
    {
        MINIMIZE
            loaded = Get_Rom(HWnd);
    }
    else
    {
        loaded = Pre_Load_Rom(HWnd, MakeRomPathAbsolute(filename));
    }

    if (loaded == 0)
        return loaded; // 0 == cancelled safely, so return before changing anything else

    if (MainMovie.File)
        CloseMovieFile(&MainMovie);
    FrameCount = 0;
    LagCount = 0;
    LagCountPersistent = 0;
    frameSearchFrames = -1;
    frameSearchInitialized = false;

    if (loaded < 0)
        return loaded; // negative == failed to load, and unloaded previous ROM

    ReopenRamWindows();

    if (!no_debug) {
      init_dbg_server();
    }

    return loaded; // positive = success
}

// some extensions that might commonly be near emulation-related files that we almost certainly can't open, or at least not directly.
static const char* s_dropIgnoreExtensions[] = { "txt", "nfo", "htm", "html", "jpg", "jpeg", "png", "bmp", "gif", "mp3", "wav", "lnk", "exe", "bat", "luasav", "sav" };

enum GensFileType
{
    FILETYPE_UNKNOWN,
    FILETYPE_MOVIE,
    FILETYPE_ROM,
    FILETYPE_SAVESTATE,
    FILETYPE_SRAM,
    FILETYPE_SCRIPT,
    FILETYPE_WATCH,
    FILETYPE_CONFIG,
};
GensFileType GuessFileType(const char* filename, const char* extension)
{
    GensFileType rv = FILETYPE_UNKNOWN;

    // first decide what we can based on the file contents
    FILE* file = fopen(filename, "rb");
    int filesize = 0;
    if (file)
    {
        unsigned char sig[11] = { 0 };
        fread(sig, 1, 10, file);
        sig[10] = 0;

        if (/*sig[0] == 'G' &&*/ sig[1] == 'S' && sig[2] == 'T' && sig[3] == 0x40 && sig[4] == 0xE0) // some previous versions of Gens had a problem writing the 'G' in "GST"...
            rv = FILETYPE_SAVESTATE;
        else if (!strncmp((const char*)sig, "Gens Movie", 10))
            rv = FILETYPE_MOVIE;
        else if (!strncmp((const char*)sig, "\033Lua", 4))
            rv = FILETYPE_SCRIPT;

        fseek(file, 0, SEEK_END);
        filesize = ftell(file);

        fclose(file);
    }

    // now decide what's leftover based on the filename extension
    if (rv == FILETYPE_UNKNOWN)
    {
        if (!stricmp(extension, "wch") || !stricmp(extension, "watch"))
            rv = FILETYPE_WATCH;
        else if (!stricmp(extension, "cfg") || !stricmp(extension, "config"))
            rv = FILETYPE_CONFIG;
        else if (!stricmp(extension, "lua"))
            rv = FILETYPE_SCRIPT;
        else if (!stricmp(extension, "srm") || !stricmp(extension, "sram"))
            rv = FILETYPE_SRAM;
        else if (!stricmp(extension, "gmv") || !stricmp(extension, "gm2"))
            rv = FILETYPE_MOVIE;
        else if (tolower(extension[0]) == 'g' && tolower(extension[1]) == 's' && (extension[2] == 't' || extension[2] == '-' || isdigit(extension[2])))
            rv = FILETYPE_SAVESTATE;
        else if (!stricmp(extension, "smd") || !stricmp(extension, "gen") || !stricmp(extension, "bin"))
            rv = FILETYPE_ROM;
        else if (filesize >= 1024) // we don't have a reliable way to tell the difference between a ROM/image and other junk, do we?
            rv = FILETYPE_ROM;
    }

    return rv;
}

long PASCAL WinProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    RECT r;
    static char Str_Tmp[1024]; // necessary to shadow this here, or things like ID_FILES_LOADSTATE will break in subtle ways...

    switch (message)
    {
    case WM_ACTIVATE:
        if (Gens_Running == 0) break;

        if (LOWORD(wParam) != WA_INACTIVE)
        {
            Active = 1;

            if (FS_Minimised && !(DialogsOpen))
            {
                FS_Minimised = 0;
                Set_Render(hWnd, 1, -1, true);
            }
        }
        else
        {
            if ((Full_Screen) && ((BOOL)HIWORD(wParam)) && (Active))
            {
                Set_Render(hWnd, 0, -1, false);
                FS_Minimised = 1;
            }

            if (Auto_Pause && Active)
            {
                Active = 0;

                if (!Paused) Pause_Screen();
                Clear_Sound_Buffer();
            }
        }
        break;

    case WM_MENUSELECT:
    case WM_ENTERSIZEMOVE:
        Clear_Sound_Buffer();
        break;

    case WM_MOVE:
        if (!Full_Screen)
        {
            GetWindowRect(HWnd, &r);
            Window_Pos.x = r.left;
            Window_Pos.y = r.top;
        }
        break;

    case WM_SIZING:
        if (!Full_Screen)
        {
            GetWindowRect(HWnd, &r);
            Window_Pos.x = r.left;
            Window_Pos.y = r.top;
            ScaleFactor = std::min(
                (float)(r.right - r.left) / 320 / ((Render_W == 0) ? 1 : 2),
                (float)(r.bottom - r.top) / 240 / ((Render_W == 0) ? 1 : 2)
            );
        }
        break;

    case WM_EXITSIZEMOVE:
        Build_Main_Menu();
        break;

    case WM_CLOSE:
        if (AskSave())
        {
            if (Sound_Initialised)
                Clear_Sound_Buffer(); //Modif N - making sure sound doesn't stutter on exit
            for (int i = (int)LuaScriptHWnds.size() - 1; i >= 0; i--)
                SendMessage(LuaScriptHWnds[i], WM_CLOSE, 0, 0);
            if (MainMovie.File != NULL)
                CloseMovieFile(&MainMovie);
            if (HexEditors.size() > 0)
                for (int i = (int)HexEditors.size() - 1; i >= 0; i--)
                    HexDestroyDialog(HexEditors[i]);
            Gens_Running = 0;
        }
        return 0;

    case WM_RBUTTONDOWN:
        POINT point;
        GetCursorPos(&point);
        if (Full_Screen)
        {
            Clear_Sound_Buffer();
            //SetCursorPos(40, 30);
            while (ShowCursor(false) >= 0);
            while (ShowCursor(true) < 0);
            SendMessage(hWnd, WM_PAINT, 0, 0);
            Restore_Primary();
            PaintsEnabled = false;
            TrackPopupMenu(Gens_Menu, TPM_LEFTALIGN | TPM_TOPALIGN, point.x, point.y, NULL, hWnd, NULL);
            PaintsEnabled = true;
            while (ShowCursor(true) < 0);
            while (ShowCursor(false) >= 0);
        }
        else
        {
            Clear_Sound_Buffer();
            Build_Context_Menu();
            TrackPopupMenu(Context_Menu, TPM_LEFTALIGN | TPM_TOPALIGN, point.x, point.y, NULL, hWnd, NULL);
        }
        break;

    case WM_CREATE:
        Active = 1;
        break;

    case WM_PAINT:
    {
        HDC         hDC;
        PAINTSTRUCT ps;

        hDC = BeginPaint(hWnd, &ps);

        if (PaintsEnabled && (!Full_Screen || DialogsOpen <= 0 || GetActiveWindow() == hWnd))
        {
            Clear_Primary_Screen(HWnd);
            Flip(hWnd);
        }

        EndPaint(hWnd, &ps);
    } break;

    // drag and drop support
    case WM_DROPFILES:
    {
        HDROP hDrop = (HDROP)wParam;
        int numDropped = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);
        // TODO: defer gmv loads to happen later than any rom loads that might be in the set of dropped files
        for (int i = 0; i < numDropped; i++)
        {
            DragQueryFile(hDrop, i, Str_Tmp, 1024);
            GensOpenFile(Str_Tmp);
        }
        DragFinish(hDrop);
        return true;
    }   break;

    case WM_COMMAND:
    {
        int command = LOWORD(wParam);

        if (command >= ID_FILES_OPENRECENTROM0 &&
            command <= ID_FILES_OPENRECENTROMMAX &&
            command - ID_FILES_OPENRECENTROM0 < MAX_RECENT_ROMS)
        {
            GensLoadRom(Recent_Rom[command - ID_FILES_OPENRECENTROM0]);
            return 0;
        }

        if (command >= ID_TOOLS_OPENRECENTMOVIE0 &&
            command <= ID_TOOLS_OPENRECENTMOVIEMAX &&
            command - ID_TOOLS_OPENRECENTMOVIE0 < MAX_RECENT_MOVIES)
        {
            GensPlayMovie(Recent_Movie[command - ID_TOOLS_OPENRECENTMOVIE0]);
            return 0;
        }

        if (command >= ID_LUA_OPENRECENTSCRIPT0 &&
            command <= ID_LUA_OPENRECENTSCRIPTMAX &&
            command - ID_LUA_OPENRECENTSCRIPT0 < MAX_RECENT_SCRIPTS)
        {
            if (LuaScriptHWnds.size() < 16)
            {
                char temp[1024];
                strcpy(temp, Recent_Scripts[command - ID_LUA_OPENRECENTSCRIPT0]);
                HWND IsScriptFileOpen(const char* Path);
                if (!IsScriptFileOpen(temp))
                {
                    HWND hDlg = CreateDialog(ghInstance, MAKEINTRESOURCE(IDD_LUA), hWnd, (DLGPROC)LuaScriptProc);
                    SendDlgItemMessage(hDlg, IDC_EDIT_LUAPATH, WM_SETTEXT, 0, (LPARAM)temp);
                    DialogsOpen++;
                }
            }
        }

        switch (command)
        {
        case ID_GRAPHICS_NEVER_SKIP_FRAME:
            Never_Skip_Frame = !Never_Skip_Frame;
            Build_Main_Menu();
            return 0;
        case ID_CHANGE_256RATIO:
            Correct_256_Aspect_Ratio = !Correct_256_Aspect_Ratio;
            InvalidateRect(hWnd, NULL, FALSE);
            Build_Main_Menu();
            return 0;
        case ID_SLOW_SPEED_1:
            SlowDownSpeed = 1;
            Build_Main_Menu();
            return 0;
        case ID_SLOW_SPEED_2:
            SlowDownSpeed = 2;
            Build_Main_Menu();
            return 0;
        case ID_SLOW_SPEED_3:
            SlowDownSpeed = 3;
            Build_Main_Menu();
            return 0;
        case ID_SLOW_SPEED_4:
            SlowDownSpeed = 4;
            Build_Main_Menu();
            return 0;
        case ID_SLOW_SPEED_5:
            SlowDownSpeed = 5;
            Build_Main_Menu();
            return 0;
        case ID_SLOW_SPEED_9:
            SlowDownSpeed = 9;
            Build_Main_Menu();
            return 0;
        case ID_SLOW_SPEED_15:
            SlowDownSpeed = 15;
            Build_Main_Menu();
            return 0;
        case ID_SLOW_SPEED_31:
            SlowDownSpeed = 31;
            Build_Main_Menu();
            return 0;
        case ID_SLOW_MODE:
            if (SlowDownMode == 1)
                SlowDownMode = 0;
            else
                SlowDownMode = 1;
            Build_Main_Menu();
            return 0;
        case ID_TOGGLE_TURBO:
            TurboToggle = !TurboToggle;
            return 0;
        case ID_SLOW_SPEED_PLUS: //Modif N - for new "speed up" key:
            if (SlowDownSpeed == 1 || SlowDownMode == 0)
                SlowDownMode = 0;
            else
            {
                SlowDownMode = 1;
                if (SlowDownSpeed <= 5)
                    SlowDownSpeed--;
                else if (SlowDownSpeed == 9)
                    SlowDownSpeed = 5;
                else if (SlowDownSpeed == 15)
                    SlowDownSpeed = 9;
                else if (SlowDownSpeed == 31)
                    SlowDownSpeed = 15;
            }
            Str_Tmp[0] = '\0';
            if (SlowDownMode == 0)
                strcpy(Str_Tmp, "100%");
            else switch (SlowDownSpeed)
            {
            case 1: strcpy(Str_Tmp, "50%"); break;
            case 2: strcpy(Str_Tmp, "25%"); break;
            case 3: strcpy(Str_Tmp, "20%"); break;
            case 4: strcpy(Str_Tmp, "16%"); break;
            case 5: strcpy(Str_Tmp, "10%"); break;
            case 9: strcpy(Str_Tmp, "6%"); break;
            case 15: strcpy(Str_Tmp, "3%"); break;
            }
            if (Str_Tmp[0])
                Put_Info(Str_Tmp);

            Build_Main_Menu();
            return 0;
        case ID_SLOW_SPEED_MINUS: //Modif N - for new "speed down" key:
            if (SlowDownMode != 0)
            {
                if (SlowDownSpeed < 5)
                    SlowDownSpeed++;
                else if (SlowDownSpeed == 5)
                    SlowDownSpeed = 9;
                else if (SlowDownSpeed == 9)
                    SlowDownSpeed = 15;
                else if (SlowDownSpeed == 15)
                    SlowDownSpeed = 31;
            }
            SlowDownMode = 1;

            Str_Tmp[0] = '\0';
            switch (SlowDownSpeed)
            {
            case 1: strcpy(Str_Tmp, "50%"); break;
            case 2: strcpy(Str_Tmp, "25%"); break;
            case 3: strcpy(Str_Tmp, "20%"); break;
            case 4: strcpy(Str_Tmp, "16%"); break;
            case 5: strcpy(Str_Tmp, "10%"); break;
            case 9: strcpy(Str_Tmp, "6%"); break;
            case 15: strcpy(Str_Tmp, "3%"); break;
            }
            if (Str_Tmp[0])
                Put_Info(Str_Tmp);

            Build_Main_Menu();
            return 0;
        case ID_TOGGLE_MOVIE_READONLY: //Modif N - for new toggle readonly key:
            if (MainMovie.File)
            {
                if (MainMovie.ReadOnly && (GetFileAttributes(MainMovie.PhysicalFileName) & FILE_ATTRIBUTE_READONLY))
                {
                    Put_Info("Can't toggle read-only; write permission denied.");
                }
                else
                {
                    MainMovie.ReadOnly = !MainMovie.ReadOnly;
                    if (MainMovie.ReadOnly)
                        Put_Info("Movie is now read-only.");
                    else
                        Put_Info("Movie is now editable.");
                }
            }
            else
            {
                Put_Info("Can't toggle read-only; no movie is active.");
            }
            break;

            // increase frameSearchFrames, then
            // starting at initial frame (frameSearch_Start_State_Buffer)
            // run game for (frameSearchFrames)
            // with input = initial input (frameSearchInitialInput)
            // then unpause and run game normally (with new input)
            // TODO: add support for doing this during read-only mode meaning it switches to playback and the movie replaces frameSearchInitialInput for frameSearchFrames steps, then it switches to recording before unpausing
        case ID_FRAME_SEARCH_NEXT:
        {
            if (!Game || (MainMovie.File && MainMovie.Status == MOVIE_PLAYING))
                break;
            frameSearchFrames++;
            MESSAGE_NUM_L("%d frame search", frameSearchFrames);
            if (frameSearchFrames == 0)
            {   // setup initial frame
                frameSearchInitialInput = GetLastInputCondensed();
                Save_State_To_Buffer(frameSearch_Start_State_Buffer);
            }
            else
            {
                Load_State_From_Buffer(frameSearch_End_State_Buffer);
                SetNextInputCondensed(frameSearchInitialInput);
                Update_Emulation_One(HWnd);
            }
            Save_State_To_Buffer(frameSearch_End_State_Buffer);
            Update_Emulation_One(HWnd);
            soundCleared = false;
            frameSearchFinalInput = GetLastInputCondensed();
            frameSearchInitialized = true;
            Paused = 0;
        }   break;

        // decrease frameSearchFrames, then
        // starting at initial frame (frameSearch_Start_State_Buffer)
        // run game for (frameSearchFrames)
        // with input = initial input (frameSearchInitialInput)
        // then unpause and run game normally (with new input)
        // TODO: optimize this to be O(1) amortized when key is held down instead of current slow O(n) where n = frameSearchFrames
        case ID_FRAME_SEARCH_PREV:
        {
            if (frameSearchFrames <= 0 || !Game || (MainMovie.File && MainMovie.Status == MOVIE_PLAYING))
                break;
            frameSearchFrames--;
            MESSAGE_NUM_L("%d frame search", frameSearchFrames);
            Load_State_From_Buffer(frameSearch_Start_State_Buffer);
            if (MainMovie.File && MainMovie.Status == MOVIE_RECORDING)
                MainMovie.NbRerecords++;
            for (int i = 0; i < frameSearchFrames; i++)
            {
                SetNextInputCondensed(frameSearchInitialInput);
                Update_Emulation_One_Before(HWnd);
                Update_Frame_Fast();
                Update_Emulation_After_Fast(HWnd);
            }
            Save_State_To_Buffer(frameSearch_End_State_Buffer);
            Update_Emulation_One(HWnd);
            soundCleared = false;
            frameSearchFinalInput = GetLastInputCondensed();
            Paused = 0;
        }   break;

        // starting at initial frame (frameSearch_Start_State_Buffer)
        // run game for (frameSearchFrames)
        // with input = initial input (frameSearchInitialInput)
        // then run one frame with the last new input (frameSearchFinalInput)
        // then pause and exit frame search mode
        case ID_FRAME_SEARCH_END:
            if (!frameSearchInitialized || !Game || (MainMovie.File && MainMovie.Status == MOVIE_PLAYING))
                break;
            MESSAGE_L("Frame search result");
            Load_State_From_Buffer(frameSearch_End_State_Buffer);
            if (MainMovie.File && MainMovie.Status == MOVIE_RECORDING)
                MainMovie.NbRerecords++;
            SetNextInputCondensed(frameSearchFinalInput);
            Update_Emulation_One(HWnd);
            soundCleared = false;
            Paused = 1;
            frameSearchFrames = -1;
            break;

        case ID_RAM_SEARCH:
            if (!RamSearchHWnd)
            {
                RamSearchHWnd = CreateDialog(ghInstance, MAKEINTRESOURCE(IDD_RAMSEARCH), hWnd, (DLGPROC)RamSearchProc);
                DialogsOpen++;
            }
            else
                SetForegroundWindow(RamSearchHWnd);
            break;

        case ID_RAM_WATCH:
            if (!RamWatchHWnd)
            {
                RamWatchHWnd = CreateDialog(ghInstance, MAKEINTRESOURCE(IDD_RAMWATCH), hWnd, (DLGPROC)RamWatchProc);
                DialogsOpen++;
            }
            else
                SetForegroundWindow(RamWatchHWnd);
            break;

        case ID_HEX_EDITOR:
            HexCreateDialog();
            break;

        case ID_PLANE_EXPLORER:
            if (!PlaneExplorerHWnd)
            {
                PlaneExplorerHWnd = CreateDialog(ghInstance, MAKEINTRESOURCE(IDD_PLANEEXPLORER), hWnd, (DLGPROC)PlaneExplorerDialogProc);
                DialogsOpen++;
            }
            else
                SetForegroundWindow(PlaneExplorerHWnd);
            break;

        case ID_VDP_RAM:
            if (!VDPRamHWnd)
            {
                VDPRamHWnd = CreateDialog(ghInstance, MAKEINTRESOURCE(IDD_VDPRAM), hWnd, (DLGPROC)VDPRamProc);
                DialogsOpen++;
            }
            else
                SetForegroundWindow(VDPRamHWnd);
            break;

#ifdef DEBUG_Z80
        case ID_YM2612_DEBUGGER:
            if (!YM2612DbgHWnd)
            {
              YM2612DbgHWnd = CreateDialog(ghInstance, MAKEINTRESOURCE(IDD_YM2612_DEBUGGER), hWnd, (DLGPROC)YM2612WndProcDialog);
              DialogsOpen++;
            }
            else
                SetForegroundWindow(YM2612DbgHWnd);
            break;
#endif

        case ID_VDP_SPRITES:
            if (!VDPSpritesHWnd)
            {
                VDPSpritesHWnd = CreateDialog(ghInstance, MAKEINTRESOURCE(IDD_VDP_SPRITES), hWnd, (DLGPROC)VDPSpritesProc);
                DialogsOpen++;
            }
            else
                SetForegroundWindow(VDPSpritesHWnd);
            break;

        case IDC_NEW_LUA_SCRIPT:
            if (LuaScriptHWnds.size() < 16)
            {
                CreateDialog(ghInstance, MAKEINTRESOURCE(IDD_LUA), hWnd, (DLGPROC)LuaScriptProc);
                DialogsOpen++;
            }
            break;

        case IDC_CLOSE_LUA_SCRIPTS:
            for (int i = (int)LuaScriptHWnds.size() - 1; i >= 0; i--)
                SendMessage(LuaScriptHWnds[i], WM_CLOSE, 0, 0);
            break;

        case IDC_LUA_SCRIPT_0:
        case IDC_LUA_SCRIPT_1:
        case IDC_LUA_SCRIPT_2:
        case IDC_LUA_SCRIPT_3:
        case IDC_LUA_SCRIPT_4:
        case IDC_LUA_SCRIPT_5:
        case IDC_LUA_SCRIPT_6:
        case IDC_LUA_SCRIPT_7:
        case IDC_LUA_SCRIPT_8:
        case IDC_LUA_SCRIPT_9:
        case IDC_LUA_SCRIPT_10:
        case IDC_LUA_SCRIPT_11:
        case IDC_LUA_SCRIPT_12:
        case IDC_LUA_SCRIPT_13:
        case IDC_LUA_SCRIPT_14:
        case IDC_LUA_SCRIPT_15:
        {
            unsigned int index = command - IDC_LUA_SCRIPT_0;
            if (LuaScriptHWnds.size() > index)
                SetForegroundWindow(LuaScriptHWnds[index]);
        }   break;

        case IDC_LUA_SCRIPT_HOTKEY_1:
        case IDC_LUA_SCRIPT_HOTKEY_2:
        case IDC_LUA_SCRIPT_HOTKEY_3:
        case IDC_LUA_SCRIPT_HOTKEY_4:
        case IDC_LUA_SCRIPT_HOTKEY_5:
        case IDC_LUA_SCRIPT_HOTKEY_6:
        case IDC_LUA_SCRIPT_HOTKEY_7:
        case IDC_LUA_SCRIPT_HOTKEY_8:
        case IDC_LUA_SCRIPT_HOTKEY_9:
        case IDC_LUA_SCRIPT_HOTKEY_10:
        case IDC_LUA_SCRIPT_HOTKEY_11:
        case IDC_LUA_SCRIPT_HOTKEY_12:
        case IDC_LUA_SCRIPT_HOTKEY_13:
        case IDC_LUA_SCRIPT_HOTKEY_14:
        case IDC_LUA_SCRIPT_HOTKEY_15:
        case IDC_LUA_SCRIPT_HOTKEY_16:
        {
            unsigned int index = command - IDC_LUA_SCRIPT_HOTKEY_1;
            CallRegisteredLuaFunctions((LuaCallID)(LUACALL_SCRIPT_HOTKEY_1 + index));
        }   break;

        case ID_PLAY_FROM_START:
            GensReplayMovie();
            return 0;
        case ID_RESUME_RECORD:
            if (!(Game))
                return 0;
            if (MainMovie.Status != MOVIE_PLAYING)
            {
                MESSAGE_L("Error: no movie is playing");
                return 0;
            }
            if (MainMovie.ReadOnly)
            {
                MESSAGE_L("Error: movie is read only");
                return 0;
            }
            strncpy(Str_Tmp, MainMovie.FileName, 512);
            if (AutoBackupEnabled)
            {
                strcat(MainMovie.FileName, ".gmv");
                for (int i = strlen(MainMovie.FileName); i >= 0; i--) if (MainMovie.FileName[i] == '|') MainMovie.FileName[i] = '_';
                MainMovie.FileName[strlen(MainMovie.FileName) - 7] = 'b'; // ".bak"
                MainMovie.FileName[strlen(MainMovie.FileName) - 6] = 'a';
                MainMovie.FileName[strlen(MainMovie.FileName) - 5] = 'k';
                BackupMovieFile(&MainMovie);
                strncpy(MainMovie.FileName, Str_Tmp, 512);
            }
            MainMovie.Status = MOVIE_RECORDING;
            MainMovie.NbRerecords++;
            if (MainMovie.TriplePlayerHack)
                MainMovie.LastFrame = std::max(std::max(std::max(Track1_FrameCount, Track2_FrameCount), Track3_FrameCount), FrameCount);
            else
                MainMovie.LastFrame = std::max(std::max(Track1_FrameCount, Track2_FrameCount), FrameCount);
            MESSAGE_L("Recording from current frame");
            Build_Main_Menu();
            return 0;
        case ID_STOP_MOVIE:
            if (!(Game))
                return 0;
            if (MainMovie.Status == MOVIE_RECORDING)
                MovieRecordingStuff();
            if (MainMovie.File != NULL)
                CloseMovieFile(&MainMovie);
            MainMovie.Status = 0;
            MESSAGE_L("Recording/Playing stop");
            Build_Main_Menu();
            return 0;
        case ID_RECORD_MOVIE:   //Modif
            if (!(Game))
                if (SendMessage(hWnd, WM_COMMAND, ID_FILES_OPENROM, 0) <= 0) // Modif N. -- prompt once to load ROM if it's not already loaded
                    return 0;
            //                  if(MainMovie.File!=NULL || MainMovie.Status==MOVIE_FINISHED) //Modif N - disabled; if the user chose record, they meant it!
            //                      return 0;
            MINIMIZE
                dialogAgain : //Nitsuja added this
            DialogsOpen++;
            DialogBox(ghInstance, MAKEINTRESOURCE(IDD_RECORD_A_MOVIE), hWnd, (DLGPROC)RecordMovieProc);
            if (RecordMovieCanceled)
                return 0;
            if (MainMovie.StateRequired)
            {
                FrameCount = 0;
                LagCount = 0;
                LagCountPersistent = 0;
                frameSearchFrames = -1; frameSearchInitialized = false;
                strncpy(Str_Tmp, Rom_Name, 507);
                strcat(Str_Tmp, "[GMV]");
                strcat(Str_Tmp, ".gst");
                Change_File_S(Str_Tmp, Movie_Dir, "Save state", "State Files\0*.gs?\0All Files\0*.*\0\0", "", hWnd);
                if (Save_State(Str_Tmp) == 0)
                    return 0;
            }
            MainMovie.File = fopen(MainMovie.FileName, "wb");

            if (!MainMovie.File)
            {
                char pathError[256];
                char* slash = strrchr(MainMovie.FileName, '\\');
                char* slash2 = strrchr(MainMovie.FileName, '/');
                if (slash > slash2) *(slash + 1) = 0;
                if (slash2 > slash) *(slash2 + 1) = 0;
                sprintf(pathError, "Invalid path: %s", MainMovie.FileName);
                MessageBox(hWnd, pathError, "Error", MB_OK | MB_ICONERROR);
                MainMovie.Status = 0;
                goto dialogAgain;
                //char* div = strrchr(MainMovie.FileName, '\\');
                //if(!div) div = strrchr(MainMovie.FileName, '/');
                //if(div)
                //{
                //  memmove(MainMovie.FileName, div+1, strlen(div));
                //  MainMovie.File=fopen(MainMovie.FileName,"wb");
                //}
            }

            if (!MainMovie.File)
            {
                MessageBox(hWnd, MainMovie.FileName, "Error", MB_OK);

                MainMovie.Status = 0;
                MESSAGE_L("File error");
                    return 0;
            }
            MainMovie.Ok = 1;
            fseek(MainMovie.File, 0, SEEK_SET);
            fwrite(MainMovie.Header, 16, 1, MainMovie.File);
            fseek(MainMovie.File, 24, SEEK_SET);
            fwrite(MainMovie.Note, 40, 1, MainMovie.File);
            fclose(MainMovie.File);
            MainMovie.File = NULL;
            if (OpenMovieFile(&MainMovie) == 0)
            {
                MainMovie.Status = 0;
                MESSAGE_L("File error");
                    return 0;
            }
            FrameCount = 0;
            LagCount = 0;
            LagCountPersistent = 0;
            frameSearchFrames = -1; frameSearchInitialized = false;
            MainMovie.NbRerecords = 0;
            MainMovie.LastFrame = 0;
            if (MainMovie.StateRequired)
            {
                MESSAGE_L("Recording from now");
            }
            else
            {
                if (Genesis_Started)
                {
                    Pre_Load_Rom(HWnd, Recent_Rom[0]);
                    MESSAGE_L("Genesis reset");
                        memset(SRAM, 0, sizeof(SRAM));
                }
                
                MESSAGE_L("Recording from start");
            }
            Build_Main_Menu();
            CallRegisteredLuaFunctions(LUACALL_ONSTART);
            return 0;
        case ID_PLAY_MOVIE:
            GensPlayMovie(NULL, false);
            return 0;

        case ID_SPLICE:
            if (SpliceFrame)
            {
                DoMovieSplice();
                return 0;
            }
            else if (MainMovie.File != NULL)
            {
                DialogsOpen++;
                DialogBox(ghInstance, MAKEINTRESOURCE(IDD_PROMPT), hWnd, (DLGPROC)PromptSpliceFrameProc);
                return 0;
            }
            else
                return 1;

        case IDC_SEEK_FRAME:
            if (SeekFrame)
            {
                SeekFrame = 0;
                MustUpdateMenu = 1;
                MESSAGE_L("Seek Cancelled");
                return 0;
            }
            else
            {
                DialogsOpen++;
                DialogBox(ghInstance, MAKEINTRESOURCE(IDD_PROMPT), hWnd, (DLGPROC)PromptSeekFrameProc);
                return 0;
            }

        case ID_FILES_QUIT:
            PostMessage(hWnd, WM_CLOSE, 0, 0);
            return 0;

        case ID_FILES_OPENROM:
        {
            GensLoadRom(NULL);
            return 0;
        }

        case ID_FILES_CLOSEROM:
            if (MainMovie.File != NULL)
                CloseMovieFile(&MainMovie);
            if (Sound_Initialised) Clear_Sound_Buffer();

            Free_Rom(Game);
            Build_Main_Menu();
            FrameCount = 0;
            LagCount = 0;
            LagCountPersistent = 0;
            frameSearchFrames = -1; frameSearchInitialized = false;
            return 0;

        case ID_FILES_GAMEGENIE:
            MINIMIZE
                DialogsOpen++;
            DialogBox(ghInstance, MAKEINTRESOURCE(IDD_GAMEGENIE), hWnd, (DLGPROC)GGenieProc);
            Build_Main_Menu();
            return 0;

        case ID_FILES_LOADSTATE:
            Str_Tmp[0] = 0;
            Get_State_File_Name(Str_Tmp);
            Load_State(Str_Tmp);
            return 0;

        case ID_FILES_LOADSTATEAS:
            Str_Tmp[0] = 0;
            DialogsOpen++;
            Change_File_L(Str_Tmp, State_Dir, "Load state", "State Files\0*.gs?\0All Files\0*.*\0\0", "", hWnd);
            DialogsOpen--;
            Load_State(Str_Tmp);
            return 0;

        case ID_FILES_SAVESTATE:
            Str_Tmp[0] = 0;
            Get_State_File_Name(Str_Tmp);
            Save_State(Str_Tmp);
            return 0;

        case ID_FILES_SAVESTATEAS:
            DialogsOpen++;
            Change_File_S(Str_Tmp, State_Dir, "Save state", "State Files\0*.gs?\0All Files\0*.*\0\0", "", hWnd);
            DialogsOpen--;
            Save_State(Str_Tmp);
            return 0;

        case ID_FILES_PREVIOUSSTATE:
            Set_Current_State((Current_State + 9) % 10, true, true);
            return 0;

        case ID_FILES_NEXTSTATE:
            Set_Current_State((Current_State + 1) % 10, true, true);
            return 0;

        case ID_GRAPHICS_VSYNC:
            Change_VSync(hWnd);
            return 0;

        case ID_GRAPHICS_SWITCH_MODE:
            if (Full_Screen) Set_Render(hWnd, 0, -1, true);
            else Set_Render(hWnd, 1, Render_FS, true);
            return 0;

        case ID_GRAPHICS_FS_SAME_RES: //Upth-Add - toggle the same-res fullscreen flag
            FS_No_Res_Change = !(FS_No_Res_Change);
            Build_Main_Menu();
            if (Full_Screen) Set_Render(hWnd, 1, Render_FS, true); // Modif N. -- if already in fullscreen, take effect immediately
            return 0;

        case ID_GRAPHICS_COLOR_ADJUST:
            MINIMIZE
                DialogsOpen++;
            DialogBox(ghInstance, MAKEINTRESOURCE(IDD_COLOR), hWnd, (DLGPROC)ColorProc);
            return 0;

        case ID_GRAPHICS_RENDER_NORMAL:
            Set_Render(hWnd, Full_Screen, 0, false);
            return 0;

        case ID_GRAPHICS_RENDER_DOUBLE:
            Set_Render(hWnd, Full_Screen, 1, false);
            return 0;

        case ID_GRAPHICS_RENDER_EPX:
            Set_Render(hWnd, Full_Screen, 2, false);
            return 0;
        case ID_GRAPHICS_RENDER_EPXPLUS:
            Set_Render(hWnd, Full_Screen, 11, false);
            return 0;

        case ID_GRAPHICS_RENDER_DOUBLE_INT:
            Set_Render(hWnd, Full_Screen, 3, false);
            return 0;

        case ID_GRAPHICS_RENDER_FULLSCANLINE:
            Set_Render(hWnd, Full_Screen, 4, false);
            return 0;

        case ID_GRAPHICS_SIZE_1X:
            if (ScaleFactor != 1.0)
            {
                ScaleFactor = 1.0;
                Set_Window_Size(hWnd);
                Build_Main_Menu();
            }
            return 0;

        case ID_GRAPHICS_SIZE_2X:
            if (ScaleFactor != 2.0)
            {
                ScaleFactor = 2.0;
                Set_Window_Size(hWnd);
                Build_Main_Menu();
            }
            return 0;

        case ID_GRAPHICS_SIZE_3X:
            if (ScaleFactor != 3.0)
            {
                ScaleFactor = 3.0;
                Set_Window_Size(hWnd);
                Build_Main_Menu();
            }
            return 0;

        case ID_GRAPHICS_SIZE_4X:
            if (ScaleFactor != 4.0)
            {
                ScaleFactor = 4.0;
                Set_Window_Size(hWnd);
                Build_Main_Menu();
            }
            return 0;

        case ID_GRAPHICS_LAYER0: //Nitsuja added these
        case ID_GRAPHICS_LAYER1:
        case ID_GRAPHICS_LAYER2:
        case ID_GRAPHICS_LAYER3:
        case ID_GRAPHICS_LAYERSPRITE:
        case ID_GRAPHICS_LAYERSPRITEHIGH:
            Change_Layer(command - ID_GRAPHICS_LAYER0);
            return 0;

        case ID_GRAPHICS_SPRITEALWAYS:
            Change_SpriteTop();
            return 0;

        case ID_GRAPHICS_SPRITEBOXING:
            Change_SpriteBoxing();
            return 0;

        case ID_GRAPHICS_LAYERSWAPA:
        case ID_GRAPHICS_LAYERSWAPB:
        case ID_GRAPHICS_LAYERSWAPS:
            Change_LayerSwap(command - ID_GRAPHICS_LAYERSWAPA);
            return 0;

        case ID_GRAPHICS_TOGGLEA:
        case ID_GRAPHICS_TOGGLEB:
        case ID_GRAPHICS_TOGGLES:
            Change_Plane(command - ID_GRAPHICS_TOGGLEA);
            return 0;

        case ID_GRAPHICS_PINKBG:
        {
            PinkBG = !PinkBG;
            Build_Main_Menu();
            Recalculate_Palettes();
            Show_Genesis_Screen(hWnd);
            char message[256];
            sprintf(message, "Pink background %sabled", PinkBG ? "en" : "dis");
            MESSAGE_L(message);

                return 0;
        }

        case ID_GRAPHICS_RENDER_50SCANLINE:
            Set_Render(hWnd, Full_Screen, 5, false);
            return 0;

        case ID_GRAPHICS_RENDER_25SCANLINE:
            Set_Render(hWnd, Full_Screen, 6, false);
            return 0;

        case ID_GRAPHICS_RENDER_INTESCANLINE:
            Set_Render(hWnd, Full_Screen, 7, false);
            return 0;

        case ID_GRAPHICS_RENDER_INT50SCANLIN:
            Set_Render(hWnd, Full_Screen, 8, false);
            return 0;

        case ID_GRAPHICS_RENDER_INT25SCANLIN:
            Set_Render(hWnd, Full_Screen, 9, false);
            return 0;

        case ID_GRAPHICS_RENDER_2XSAI:
            Set_Render(hWnd, Full_Screen, 10, false);
            return 0;

        case ID_GRAPHICS_PREVIOUS_RENDER:
        case ID_GRAPHICS_NEXT_RENDER:
        {
            int& Rend = Full_Screen ? Render_FS : Render_W;
            int RendOrder[] = { -1, 0, 1, 2, 11, Bits32 ? -2 : 10, 3, 4, 5, 6, 7, 8, 9, -1, -1 };
            int index = 1; for (; RendOrder[index] != Rend && index < sizeof(RendOrder) / sizeof(*RendOrder) - 1; index++);
            do { index += (command == ID_GRAPHICS_PREVIOUS_RENDER) ? -1 : 1; } while (RendOrder[index] == -2);
            Set_Render(hWnd, Full_Screen, RendOrder[index], false);
        }
        return 0;

        case ID_GRAPHICS_STRETCH:
            Change_Stretch();
            return 0;

        case ID_GRAPHICS_FORCESOFT:
            Change_Blit_Style();
            return 0;

        case ID_GRAPHICS_FRAMESKIP_AUTO:
            Set_Frame_Skip(-1);
            return 0;

        case ID_GRAPHICS_FRAMESKIP_0:
        case ID_GRAPHICS_FRAMESKIP_1:
        case ID_GRAPHICS_FRAMESKIP_2:
        case ID_GRAPHICS_FRAMESKIP_3:
        case ID_GRAPHICS_FRAMESKIP_4:
        case ID_GRAPHICS_FRAMESKIP_5:
        case ID_GRAPHICS_FRAMESKIP_6:
        case ID_GRAPHICS_FRAMESKIP_7:
        case ID_GRAPHICS_FRAMESKIP_8:
            Set_Frame_Skip(command - ID_GRAPHICS_FRAMESKIP_0);
            return 0;

        case ID_LATENCY_COMPENSATION_0:
        case ID_LATENCY_COMPENSATION_1:
        case ID_LATENCY_COMPENSATION_2:
        case ID_LATENCY_COMPENSATION_3:
        case ID_LATENCY_COMPENSATION_4:
        case ID_LATENCY_COMPENSATION_5:
            Set_Latency_Compensation(command - ID_LATENCY_COMPENSATION_0);
            return 0;

        case ID_GRAPHICS_FRAMESKIP_DECREASE:
            if (Frame_Skip <= -1)
                Set_Frame_Skip(8);
            else
                Set_Frame_Skip(Frame_Skip - 1);
            return 0;

        case ID_GRAPHICS_FRAMESKIP_INCREASE:
            if (Frame_Skip >= 8)
                Set_Frame_Skip(-1);
            else
                Set_Frame_Skip(Frame_Skip + 1);
            return 0;

        case ID_GRAPHICS_SPRITEOVER:
            Set_Sprite_Over(Sprite_Over ^ 1);
            return 0;

        case ID_FILES_SAVESTATE_1:
        case ID_FILES_SAVESTATE_2:
        case ID_FILES_SAVESTATE_3:
        case ID_FILES_SAVESTATE_4:
        case ID_FILES_SAVESTATE_5:
        case ID_FILES_SAVESTATE_6:
        case ID_FILES_SAVESTATE_7:
        case ID_FILES_SAVESTATE_8:
        case ID_FILES_SAVESTATE_9:
        case ID_FILES_SAVESTATE_0:
        {
            Set_Current_State((command - ID_FILES_SAVESTATE_1 + 1) % 10, false, false);
            char Name[1024] = { 0 };
            Get_State_File_Name(Name);
            Save_State(Name);
            return 0;
        }
        case ID_FILES_SAVESTATE_10:
        {
            Set_Current_State(10, false, false);
            char Name[1024] = { 0 };
            Get_State_File_Name(Name);
            Save_State(Name);
            return 0;
        }
        case ID_FILES_LOADSTATE_1:
        case ID_FILES_LOADSTATE_2:
        case ID_FILES_LOADSTATE_3:
        case ID_FILES_LOADSTATE_4:
        case ID_FILES_LOADSTATE_5:
        case ID_FILES_LOADSTATE_6:
        case ID_FILES_LOADSTATE_7:
        case ID_FILES_LOADSTATE_8:
        case ID_FILES_LOADSTATE_9:
        case ID_FILES_LOADSTATE_0:
        {
            Set_Current_State((command - ID_FILES_LOADSTATE_1 + 1) % 10, false, true);
            char Name[1024] = { 0 };
            Get_State_File_Name(Name);
            Load_State(Name);
            return 0;
        }
        case ID_FILES_SETSTATE_1:
        case ID_FILES_SETSTATE_2:
        case ID_FILES_SETSTATE_3:
        case ID_FILES_SETSTATE_4:
        case ID_FILES_SETSTATE_5:
        case ID_FILES_SETSTATE_6:
        case ID_FILES_SETSTATE_7:
        case ID_FILES_SETSTATE_8:
        case ID_FILES_SETSTATE_9:
        case ID_FILES_SETSTATE_0:
            Set_Current_State((command - ID_FILES_SETSTATE_1 + 1) % 10, true, true);
            return 0;

        case ID_MOVIE_CHANGETRACK_ALL:
            track = 1 | 2 | 4;
            Put_Info("Recording all tracks");
            if (!MainMovie.TriplePlayerHack) track &= 3;
            return 0;
        case ID_MOVIE_CHANGETRACK_1:
        case ID_MOVIE_CHANGETRACK_2:
        case ID_MOVIE_CHANGETRACK_3:
        {
            int chgtrack = command - ID_MOVIE_CHANGETRACK_ALL;
            track ^= chgtrack;
            sprintf(Str_Tmp, "Recording player %d %sed", std::min(chgtrack, 3), (track & chgtrack) ? "start" : "end");
            Put_Info(Str_Tmp);
            if (!MainMovie.TriplePlayerHack) track &= 3;
            return 0;
        }
        case ID_PREV_TRACK:
        {
            int maxtrack = TRACK1 | TRACK2;
            if (MainMovie.TriplePlayerHack) maxtrack |= TRACK3;
            track &= maxtrack;
            if (track == maxtrack)
                track = 2;
            else
            {
                track >>= 1;
                if (!track) track = maxtrack;
            }
            if (track == maxtrack) sprintf(Str_Tmp, "Recording all players.");
            else sprintf(Str_Tmp, "Recording player %d.", std::min((int)track, 3));
            Put_Info(Str_Tmp);
        }
        break;
        case ID_NEXT_TRACK:
        {
            int maxtrack = TRACK1 | TRACK2;
            if (MainMovie.TriplePlayerHack) maxtrack |= TRACK3;
            track &= maxtrack;
            if (track == maxtrack)
                track = 1;
            else
            {
                track <<= 1;
                track &= maxtrack;
                if (!track) track = maxtrack;
            }
            if (track == maxtrack) sprintf(Str_Tmp, "Recording all players.");
            else sprintf(Str_Tmp, "Recording player %d.", std::min((int)track, 3));
            Put_Info(Str_Tmp);
        }
        break;

        case ID_LAG_RESET:
            LagCount = 0;
            // note: the whole point of LagCountPersistent is that it doesn't get reset here
            return 0;

        case ID_CPU_RESET:
            if (!(Game))
                return 0;
            if ((MainMovie.File != NULL) && (AutoCloseMovie)) //Upth-Modif - So movie close on reset is optional
            {
                CloseMovieFile(&MainMovie);
                MainMovie.Status = 0;
            }
            if ((MainMovie.Status == MOVIE_RECORDING) && (!(AutoCloseMovie)) && (MainMovie.ReadOnly)) //Upth-Add - on reset, switch movie from recording
                MainMovie.Status = MOVIE_PLAYING; //Upth-Add - To playing, if read only has been toggled on

            if (Genesis_Started)
                Reset_Genesis();

            FrameCount = 0;
            LagCount = 0;
            LagCountPersistent = 0;
            frameSearchFrames = -1; frameSearchInitialized = false;

            if (Genesis_Started)
                MESSAGE_L("Genesis reset")

                CallRegisteredLuaFunctions(LUACALL_ONSTART);

            return 0;

        case ID_CPU_RESET68K:
            if (!(Game))
                return 0;
            if (MainMovie.File != NULL)
                CloseMovieFile(&MainMovie);
            MainMovie.Status = 0;

            if (Game)
            {
                Paused = 0;
                main68k_reset();
                if (Genesis_Started) MESSAGE_L("68000 CPU reseted")
            }
            FrameCount = 0;
            LagCount = 0;
            LagCountPersistent = 0;
            frameSearchFrames = -1; frameSearchInitialized = false;
            return 0;

        case ID_CPU_RESETZ80:
            if (!(Game))
                return 0;
            if (MainMovie.File != NULL)
                CloseMovieFile(&MainMovie);
            MainMovie.Status = 0;

            if (Game)
            {
                z80_Reset(&M_Z80);
                MESSAGE_L("CPU Z80 reseted");
            }
            FrameCount = 0;
            LagCount = 0;
            LagCountPersistent = 0;
            frameSearchFrames = -1; frameSearchInitialized = false;
            return 0;

        case ID_CPU_COUNTRY_AUTO:
            Change_Country(hWnd, -1);
            return 0;

        case ID_CPU_COUNTRY_JAPAN:
            Change_Country(hWnd, 0);
            return 0;

        case ID_CPU_COUNTRY_USA:
            Change_Country(hWnd, 1);
            return 0;

        case ID_CPU_COUNTRY_EUROPE:
            Change_Country(hWnd, 2);
            return 0;

        case ID_CPU_COUNTRY_MISC:
            Change_Country(hWnd, 3);
            return 0;

        case ID_CPU_COUNTRY_ORDER + 0:
        case ID_CPU_COUNTRY_ORDER + 1:
        case ID_CPU_COUNTRY_ORDER + 2:
            Change_Country_Order(command - ID_CPU_COUNTRY_ORDER);
            return 0;

        case ID_SOUND_RATE_11000:
            Change_Sample_Rate(hWnd, 0);
            return 0;

        case ID_SOUND_RATE_22000:
            Change_Sample_Rate(hWnd, 1);
            return 0;

        case ID_SOUND_RATE_44000:
            Change_Sample_Rate(hWnd, 2);
            return 0;

        case ID_SOUND_STEREO:
            Change_Sound_Stereo(hWnd);
            return 0;

        case ID_SOUND_SOFTEN: //Nitsuja added this
            Change_Sound_Soften(hWnd);
            return 0;

        case ID_SOUND_HOG: //Nitsuja added this
            Change_Sound_Hog();
            return 0;

        case ID_OPTIONS_FASTBLUR:
            Change_Fast_Blur();
            return 0;

        case ID_OPTIONS_SHOWFPS:
            if (Show_FPS) Show_FPS = 0;
            else Show_FPS = 1;
            return 0;

        case ID_OPTIONS_GENERAL:
            MINIMIZE
                DialogsOpen++;
            DialogBox(ghInstance, MAKEINTRESOURCE(IDD_OPTION), hWnd, (DLGPROC)OptionProc);
            Build_Main_Menu();
            return 0;

        case ID_OPTIONS_JOYPADSETTING:
            MINIMIZE
                End_Input();
            DialogsOpen++;
            DialogBox(ghInstance, MAKEINTRESOURCE(IDD_CONTROLLER), hWnd, (DLGPROC)ControllerProc);
            if (!Init_Input(ghInstance, HWnd)) return false;
            Build_Main_Menu();
            return 0;

        case ID_OPTIONS_CHANGEDIR:
            MINIMIZE
                DialogsOpen++;
            DialogBox(ghInstance, MAKEINTRESOURCE(IDD_DIRECTORIES), hWnd, (DLGPROC)DirectoriesProc);
            Build_Main_Menu();
            return 0;

        case ID_OPTIONS_CHANGEFILES:
            MINIMIZE
                DialogsOpen++;
            DialogBox(ghInstance, MAKEINTRESOURCE(IDD_FILES), hWnd, (DLGPROC)FilesProc);
            Build_Main_Menu();
            return 0;

        case ID_TOGGLE_SHOWFRAMEANDLAGCOUNT:
            FrameCounterEnabled = !FrameCounterEnabled;
            LagCounterEnabled = FrameCounterEnabled;
            if (FrameCounterEnabled) Flip(hWnd); else Show_Genesis_Screen(hWnd);
            break;
        case ID_TOGGLE_TIMEUNIT:
            FrameCounterFrames = !FrameCounterFrames;
            LagCounterFrames = FrameCounterFrames;
            Show_Genesis_Screen(hWnd);
            break;
        case ID_TOGGLE_SHOWFRAMECOUNT:
            FrameCounterEnabled = !FrameCounterEnabled;
            if (FrameCounterEnabled) Flip(hWnd); else Show_Genesis_Screen(hWnd);
            break;
        case ID_TOGGLE_SHOWLAGCOUNT:
            LagCounterEnabled = !LagCounterEnabled;
            if (LagCounterEnabled) Flip(hWnd); else Show_Genesis_Screen(hWnd);
            break;
        case ID_TOGGLE_SHOWINPUT:
            ShowInputEnabled = !ShowInputEnabled;
            if (ShowInputEnabled) Flip(hWnd); else Show_Genesis_Screen(hWnd);
            break;
        case ID_TOGGLE_SHOWFPS:
            Show_FPS = !Show_FPS;
            if (Show_FPS) Flip(hWnd); else Show_Genesis_Screen(hWnd);
            break;

        case ID_OPTION_SRAMON:
            SRAM_ON = !SRAM_ON;
            Build_Main_Menu();
            return 0;

        case ID_OPTIONS_SAVECONFIG:
            strcpy(Str_Tmp, Gens_Path);
            strcat(Str_Tmp, "Gens.cfg");
            Save_Config(Str_Tmp);
            return 0;

        case ID_OPTIONS_LOADCONFIG:
            MINIMIZE
                DialogsOpen++;
            Load_As_Config(hWnd, Game);
            DialogsOpen--;
            return 0;

        case ID_OPTIONS_SAVEASCONFIG:
            MINIMIZE
                DialogsOpen++;
            Save_As_Config(hWnd);
            DialogsOpen--;
            return 0;

        case ID_HELP_ABOUT:
            Clear_Sound_Buffer();
            DialogsOpen++;
            DialogBox(ghInstance, MAKEINTRESOURCE(ABOUTDIAL), hWnd, (DLGPROC)AboutProc);
            return 0;

        case ID_CHANGE_PALLOCK:
        {
            PalLock = !PalLock;
            for (int i = 0; i < 0x40; ++i)
                LockedPalette[i] = CRam[i];
            Build_Main_Menu();
            char message[256];
            sprintf(message, "Palette %sed", PalLock ? "lock" : "unlock");
            MESSAGE_L(message);

                return 0;
        }

        case ID_EMULATION_PAUSED:
            if (Paused)
            {
                Paused = 0;
            }
            else
            {
                Paused = 1;
                Pause_Screen();
                Clear_Sound_Buffer();
                Flip(HWnd);

                if (!client) {
                  return 0;
                }

#ifdef DEBUG_68K
                client->pause_event(M68kDW.last_pc, M68kDW.changed);
                M68kDW.changed.clear();
#else
                client->pause_event(Z80DW.last_pc, Z80DW.changed);
                Z80DW.changed.clear();
#endif
            }
            return 0;
        }
    }
    break;

    case WM_KNUX:
        MESSAGE_L("Communicating ...");

            switch (wParam)
            {
            case 0:
                switch (lParam)
                {
                case 0:
                    return 4;

                case 1:
                    GetWindowText(HWnd, Str_Tmp, 1024);
                    return (long)(char *)Str_Tmp;

                case 2:
                    return 5;

                case 3:
                    return GENS_VERSION_H;

                default:
                    return -1;
                }

            case 1:
                switch (lParam)
                {
                case 0:
                    return((long)(unsigned short *)&Ram_68k[0]);
                case 1:
                    return(64 * 1024);
                case 2:
                    return(1);
                default:
                    return(-1);
                }

            case 2:
                switch (lParam)
                {
                case 0:
                    return((long)(unsigned char *)&Ram_Z80[0]);
                case 1:
                    return(8 * 1024);
                case 2:
                    return(0);
                default:
                    return(-1);
                }

            case 3:
                switch (lParam)
                {
                case 0:
                    return((long)(char *)&Rom_Data[0]);
                case 1:
                    return(0);
                case 2:
                    return(Rom_Size);
                default:
                    return(-1);
                }

            case 4:
                switch (lParam)
                {
                case 0:
                    return(0);
                case 1:
                    return((Game != NULL) ? 1 : 0);
                case 2:
                    return(0);
                default:
                    return(-1);
                }

            default:
                return(-1);
            }
        break;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

HMENU Build_Context_Menu(void)
{
    DestroyMenu(Context_Menu);

    HMENU ContextMenu = CreatePopupMenu();
    HMENU GraphicsSize = CreatePopupMenu();
    int i = 0;
    unsigned int Flags = MF_BYPOSITION | MF_STRING;

    if (!Game)
    {
        MENU_L(ContextMenu, i++, Flags, ID_FILES_OPENRECENTROM0, "&Load Last ROM");
        MENU_L(ContextMenu, i++, Flags, ID_FILES_OPENROM, "&Open ROM...");
        MENU_L(ContextMenu, i++, Flags | MF_POPUP, (UINT)GraphicsSize, "&Window Size");
    }
    else
    {
        if (!MainMovie.Status)
        {
            MENU_L(ContextMenu, i++, Flags, ID_TOOLS_OPENRECENTMOVIE0, "&Load Last Movie");
            MENU_L(ContextMenu, i++, Flags, ID_PLAY_MOVIE, "&Open Movie...");
            MENU_L(ContextMenu, i++, Flags, ID_RECORD_MOVIE, "Record &New Movie...");

            InsertMenu(ContextMenu, i++, MF_SEPARATOR, NULL, NULL);

            MENU_L(ContextMenu, i++, Flags, ID_FILES_OPENROM, "&Open ROM...");
        }
        else
        {
            MENU_L(ContextMenu, i++, Flags, ID_PLAY_FROM_START, "&Watch Movie From Beginning");
            MENU_L(ContextMenu, i++, Flags, ID_RESUME_RECORD, "&Resume Record from Now");
            MENU_L(ContextMenu, i++, Flags | ((MainMovie.File != NULL) ? MF_ENABLED : MF_DISABLED | MF_GRAYED), ID_STOP_MOVIE, "&Stop Movie");
            MENU_L(ContextMenu, i++, Flags | ((MainMovie.Status == MOVIE_RECORDING) ? MF_CHECKED : MF_UNCHECKED), ID_RECORD_MOVIE, "Record &New Movie...");

            InsertMenu(ContextMenu, i++, MF_SEPARATOR, NULL, NULL);

            MENU_L(ContextMenu, i++, Flags, ID_FILES_OPENROM, "&Open ROM...");
            MENU_L(ContextMenu, i++, Flags, ID_PLAY_MOVIE, "&Open Movie...");
        }

        InsertMenu(ContextMenu, i++, MF_SEPARATOR, NULL, NULL);

        MENU_L(ContextMenu, i++, Flags, ID_LUA_OPENRECENTSCRIPT0, "&Load Last Lua");
        MENU_L(ContextMenu, i++, Flags, IDC_NEW_LUA_SCRIPT, "&Open Lua...");

        InsertMenu(ContextMenu, i++, MF_SEPARATOR, NULL, NULL);

        MENU_L(ContextMenu, i++, Flags, ID_EMULATION_PAUSED, Paused ? "&Unpause Emulation" : "&Pause Emulation");
        MENU_L(ContextMenu, i++, Flags | MF_POPUP, (UINT)GraphicsSize, "&Window Size");
        MENU_L(ContextMenu, i++, Flags, ID_CPU_RESET, "&Hard Reset\t\tCtrl+Shift+R");
    }

    // SIZE //

    i = 0;

    MENU_L(GraphicsSize, i++, MF_BYPOSITION | ((ScaleFactor == 1.0) ? MF_CHECKED : MF_UNCHECKED), ID_GRAPHICS_SIZE_1X, "&1x");
    MENU_L(GraphicsSize, i++, MF_BYPOSITION | ((ScaleFactor == 2.0) ? MF_CHECKED : MF_UNCHECKED), ID_GRAPHICS_SIZE_2X, "&2x");
    MENU_L(GraphicsSize, i++, MF_BYPOSITION | ((ScaleFactor == 3.0) ? MF_CHECKED : MF_UNCHECKED), ID_GRAPHICS_SIZE_3X, "&3x");
    MENU_L(GraphicsSize, i++, MF_BYPOSITION | ((ScaleFactor == 4.0) ? MF_CHECKED : MF_UNCHECKED), ID_GRAPHICS_SIZE_4X, "4x");

    Context_Menu = ContextMenu;
    return (Context_Menu);
}

HMENU Build_Main_Menu(void)
{
    unsigned int Flags;
    int i, j, Rend;

    HMENU MainMenu;
    HMENU Files;
    HMENU Graphics;
    HMENU CPU;
    HMENU Sound;
    HMENU TAS_Tools; //Upth-Add - For the new menu which contains all the TAS stuff
    HMENU Options;
    HMENU Help;
    HMENU FilesChangeState;
    HMENU FilesSaveState;
    HMENU FilesLoadState;
    HMENU FilesHistory;
    HMENU GraphicsRender;
    HMENU GraphicsSize;
    HMENU GraphicsLayers; //Nitsuja added this
    HMENU GraphicsLayersA;
    HMENU GraphicsLayersB;
    HMENU GraphicsLayersX;
    HMENU GraphicsLayersS;
    HMENU GraphicsFrameSkip;
    HMENU GraphicsLatencyCompensation;

    HMENU CPUCountry;
    HMENU CPUCountryOrder;
    HMENU CPUSlowDownSpeed;
    HMENU SoundRate;
    HMENU Tools_Movies; //Upth-Add - Submenu of TAS_Tools
    HMENU Movies_Tracks; //Upth-Add - submenu of Tas_Tools -> Tools_Movies
    HMENU MoviesHistory;
    HMENU Lua_Script;

    DestroyMenu(Gens_Menu);

    if (Full_Screen)
    {
        MainMenu = CreatePopupMenu();
        Rend = Render_FS;
    }
    else
    {
        MainMenu = CreateMenu();
        Rend = Render_W;
    }

    Files = CreatePopupMenu();
    Graphics = CreatePopupMenu();
    CPU = CreatePopupMenu();
    Sound = CreatePopupMenu();
    Options = CreatePopupMenu();
    TAS_Tools = CreatePopupMenu(); //Upth-Add - Initialize my new menus
    Help = CreatePopupMenu();
    FilesChangeState = CreatePopupMenu();
    FilesSaveState = CreatePopupMenu();
    FilesLoadState = CreatePopupMenu();
    FilesHistory = CreatePopupMenu();
    MoviesHistory = CreatePopupMenu();
    GraphicsRender = CreatePopupMenu();
    GraphicsSize = CreatePopupMenu();
    GraphicsLayers = CreatePopupMenu(); //Nitsuja added this
    GraphicsLayersA = CreatePopupMenu();
    GraphicsLayersB = CreatePopupMenu();
    GraphicsLayersX = CreatePopupMenu();
    GraphicsLayersS = CreatePopupMenu();
    GraphicsFrameSkip = CreatePopupMenu();
    GraphicsLatencyCompensation = CreatePopupMenu();

    CPUCountry = CreatePopupMenu();
    CPUCountryOrder = CreatePopupMenu();
    CPUSlowDownSpeed = CreatePopupMenu();
    SoundRate = CreatePopupMenu();
    Tools_Movies = CreatePopupMenu(); //Upth-Add - Initialize my new menus
    Movies_Tracks = CreatePopupMenu(); //Upth-Add - Initialize new menu
    Lua_Script = CreatePopupMenu();

    /////////////////////////////////////////////
    //                  BEGIN                  //
    /////////////////////////////////////////////

    Flags = MF_BYPOSITION | MF_POPUP | MF_STRING;

    //smenu, pos, flags, id, str, suffixe, def
    MENU_L(MainMenu, 0, Flags, (UINT)Files, "&File");
    MENU_L(MainMenu, 1, Flags, (UINT)Graphics, "&Graphics");
    MENU_L(MainMenu, 2, Flags, (UINT)CPU, "&CPU");
    MENU_L(MainMenu, 3, Flags, (UINT)Sound, "&Sound");
    MENU_L(MainMenu, 4, Flags, (UINT)TAS_Tools, "&Tools"); //Upth-Add - Put the new menu in between sound and options // Nitsuja: changed TAS Tools to Tools to prevent extra-wide menu in normal render mode, and because spaces in menu titles can be confusing
    MENU_L(MainMenu, 5, Flags, (UINT)Options, "&Options"); //Upth-Modif - this now goes in one later
    MENU_L(MainMenu, 6, Flags, (UINT)Help, "&Help"); //Upth-Modif - this now goes in one later

    //////////////////////////////////////////////////
    //                  FILES MENU                  //
    //////////////////////////////////////////////////

    Flags = MF_BYPOSITION | MF_STRING;

    MENU_L(Files, 0, Flags, ID_FILES_OPENROM, "&Open ROM\t\tCtrl+O");
    MENU_L(Files, 1, Flags, ID_FILES_CLOSEROM, "&Close ROM\t\tCtrl+C");

    i = 2;

    MENU_L(Files, i++, Flags, ID_FILES_GAMEGENIE, "&Game Genie");

    InsertMenu(Files, i++, MF_SEPARATOR, NULL, NULL);

    MENU_L(Files, i++, MF_BYPOSITION | MF_POPUP | MF_STRING, (UINT)FilesSaveState, "Save State");
    MENU_L(Files, i++, MF_BYPOSITION | MF_POPUP | MF_STRING, (UINT)FilesLoadState, "Load State");
    MENU_L(Files, i++, MF_BYPOSITION | MF_POPUP | MF_STRING, (UINT)FilesChangeState, "C&hange State");

    InsertMenu(Files, i++, MF_SEPARATOR, NULL, NULL);

    if (strcmp(Recent_Rom[0], ""))
    {
        MENU_L(Files, i++, MF_BYPOSITION | MF_POPUP | MF_STRING, (UINT)FilesHistory, "&ROM History");

        InsertMenu(Files, i++, MF_SEPARATOR, NULL, NULL);
    }

    MENU_L(Files, i++, Flags, ID_FILES_QUIT, "&Quit\tAlt F4");

    // CHANGE STATE //

    MENU_L(FilesChangeState, i++, Flags, ID_FILES_PREVIOUSSTATE, "Previous State");
    MENU_L(FilesChangeState, i++, Flags, ID_FILES_NEXTSTATE, "Next State");

    InsertMenu(FilesChangeState, i++, MF_SEPARATOR, NULL, NULL);

    for (j = 0; j < 10; j++)
    {
        wsprintf(Str_Tmp, "Set &%d", (j + 1) % 10);
        MENU_L(FilesChangeState, i++, Flags | (Current_State == ((j + 1) % 10) ? MF_CHECKED : MF_UNCHECKED), ID_FILES_SETSTATE_1 + j, Str_Tmp);
    }

    MENU_L(FilesSaveState, i++, Flags, ID_FILES_SAVESTATE, "Quick &Save\t\tF5");
    MENU_L(FilesSaveState, i++, Flags, ID_FILES_SAVESTATEAS, "&Save State as...\t\tShift+F5");

    InsertMenu(FilesSaveState, i++, MF_SEPARATOR, NULL, NULL);

    for (j = 0; j < 10; j++)
    {
        wsprintf(Str_Tmp, "Save &%d", (j + 1) % 10);
        MENU_L(FilesSaveState, i++, Flags, ID_FILES_SAVESTATE_1 + j, Str_Tmp);
    }

    MENU_L(FilesLoadState, i++, Flags, ID_FILES_LOADSTATE, "Quick &Load\t\tF8");
    MENU_L(FilesLoadState, i++, Flags, ID_FILES_LOADSTATEAS, "&Load State...\t\tShift+F8");

    InsertMenu(FilesLoadState, i++, MF_SEPARATOR, NULL, NULL);

    for (j = 0; j < 10; j++)
    {
        wsprintf(Str_Tmp, "Load &%d", (j + 1) % 10);
        MENU_L(FilesLoadState, i++, Flags, ID_FILES_LOADSTATE_1 + j, Str_Tmp);
    }

    // HISTORY //

    for (i = 0; i < MAX_RECENT_ROMS; i++)
    {
        if (strcmp(Recent_Rom[i], ""))
        {
            char tmp[1024];
            switch (Detect_Format(Recent_Rom[i]) >> 1)
            {
            default:
                strcpy(tmp, "[---]    - "); // does not exist anymore
                break;

            case GENESIS_ROM >> 1:
                strcpy(tmp, "[MD]   - ");
                break;

            case COMPRESSED_IMAGE >> 1:
                strcpy(tmp, "[ZIP]  - ");
                break;
            }
            Get_Name_From_Path(Recent_Rom[i], Str_Tmp);
            strcat(tmp, Str_Tmp);
            // & is an escape sequence in windows menu names, so replace & with &&
            int len = strlen(tmp);
            for (int j = 0; j < len && len < 1023; j++)
                if (tmp[j] == '&')
                    memmove(tmp + j + 1, tmp + j, strlen(tmp + j) + 1), ++len, ++j;

            MENU_L(FilesHistory, i, Flags, ID_FILES_OPENRECENTROM0 + i, tmp);
        }
        else break;
    }

    /////////////////////////////////////////////////////
    //                  GRAPHICS MENU                  //
    /////////////////////////////////////////////////////

    Flags = MF_BYPOSITION | MF_STRING;
    i = 0; //In this next section Nitsuja and I simplified the menu generation code greatly through consistent use of "i" and the trinary operator.

    if (Full_Screen)
        MENU_L(Graphics, i++, Flags, ID_GRAPHICS_SWITCH_MODE, "&Windowed");
    else
        MENU_L(Graphics, i++, Flags, ID_GRAPHICS_SWITCH_MODE, "&Full Screen");

    MENU_L(Graphics, i++, Flags | (((Full_Screen && FS_VSync) || (!Full_Screen && W_VSync)) ? MF_CHECKED : MF_UNCHECKED), ID_GRAPHICS_VSYNC, "&VSync\t\tShift+F3");
    MENU_L(Graphics, i++, Flags | (Stretch ? MF_CHECKED : MF_UNCHECKED), ID_GRAPHICS_STRETCH, "&Stretch\t\tShift+F2");
    MENU_L(Graphics, i++, Flags | (FS_No_Res_Change ? MF_CHECKED : MF_UNCHECKED), ID_GRAPHICS_FS_SAME_RES, "&Windowed Fullscreen"); // UpthAdd
    MENU_L(Graphics, i++, Flags | (Correct_256_Aspect_Ratio ? MF_CHECKED : MF_UNCHECKED), ID_CHANGE_256RATIO, "Proper Aspect Ratio in low-res mode");
    MENU_L(Graphics, i++, Flags, ID_GRAPHICS_COLOR_ADJUST, "&Color Adjust...");
    MENU_L(Graphics, i++, Flags | MF_POPUP, (UINT)GraphicsRender, "&Render");
    MENU_L(Graphics, i++, Flags | MF_POPUP, (UINT)GraphicsSize, "&Window Size");

    InsertMenu(Graphics, i++, MF_SEPARATOR, NULL, NULL);

    MENU_L(Graphics, i++, Flags | MF_POPUP, (UINT)GraphicsLayers, "&Layers");
    MENU_L(Graphics, i++, Flags | (PalLock ? MF_CHECKED : MF_UNCHECKED), ID_CHANGE_PALLOCK, "Lock &Palette");
    MENU_L(Graphics, i++, Flags | (Sprite_Over ? MF_CHECKED : MF_UNCHECKED), ID_GRAPHICS_SPRITEOVER, "&Sprite Limit");
    MENU_L(Graphics, i++, Flags | (PinkBG ? MF_CHECKED : MF_UNCHECKED), ID_GRAPHICS_PINKBG, "&Pink Background");

    InsertMenu(Graphics, i++, MF_SEPARATOR, NULL, NULL);

    MENU_L(Graphics, i++, MF_BYPOSITION | MF_POPUP | MF_STRING, (UINT)GraphicsLatencyCompensation, "L&atency Compensation");
    MENU_L(Graphics, i++, MF_BYPOSITION | MF_POPUP | MF_STRING, (UINT)GraphicsFrameSkip, "&Frame Skip");
    MENU_L(Graphics, i++, Flags | (Never_Skip_Frame ? MF_CHECKED : MF_UNCHECKED), ID_GRAPHICS_NEVER_SKIP_FRAME, "&Never skip frame with auto frameskip");

    //  InsertMenu(Graphics, 12, MF_SEPARATOR, NULL, NULL);

    // RENDER //

    i = 0;

    MENU_L(GraphicsRender, i++, MF_BYPOSITION | MF_STRING | ((Rend == 0) ? MF_CHECKED : MF_UNCHECKED), ID_GRAPHICS_RENDER_NORMAL, "&Normal");
    MENU_L(GraphicsRender, i++, MF_BYPOSITION | MF_STRING | ((Rend == 1) ? MF_CHECKED : MF_UNCHECKED), ID_GRAPHICS_RENDER_DOUBLE, "&Double");
    MENU_L(GraphicsRender, i++, MF_BYPOSITION | ((Rend == 2) ? MF_CHECKED : MF_UNCHECKED), ID_GRAPHICS_RENDER_EPX, "&EPX"); //Modif N.
    MENU_L(GraphicsRender, i++, MF_BYPOSITION | ((Rend == 11) ? MF_CHECKED : MF_UNCHECKED), ID_GRAPHICS_RENDER_EPXPLUS, "EP&X+"); //Modif N.

    if (Have_MMX && !Bits32)
        MENU_L(GraphicsRender, i++, MF_BYPOSITION | (Bits32 ? MF_DISABLED | MF_GRAYED | MF_UNCHECKED : ((Rend == 10) ? MF_CHECKED : MF_UNCHECKED)), ID_GRAPHICS_RENDER_2XSAI, "2xSAI (&Kreed)");

    MENU_L(GraphicsRender, i++, MF_BYPOSITION | (((Rend == 3) ? MF_CHECKED : MF_UNCHECKED)), ID_GRAPHICS_RENDER_DOUBLE_INT, "&Interpolated");
    MENU_L(GraphicsRender, i++, MF_BYPOSITION | (((Rend == 4) ? MF_CHECKED : MF_UNCHECKED)), ID_GRAPHICS_RENDER_FULLSCANLINE, "&Scanline");

    if (Have_MMX)
    {
        MENU_L(GraphicsRender, i++, MF_BYPOSITION | MF_STRING | (((Rend == 5) ? MF_CHECKED : MF_UNCHECKED)), ID_GRAPHICS_RENDER_50SCANLINE, "&50% Scanline");
        MENU_L(GraphicsRender, i++, MF_BYPOSITION | (((Rend == 6) ? MF_CHECKED : MF_UNCHECKED)), ID_GRAPHICS_RENDER_25SCANLINE, "&25% Scanline");
    }

    MENU_L(GraphicsRender, i++, MF_BYPOSITION | MF_STRING | (((Rend == 7) ? MF_CHECKED : MF_UNCHECKED)), ID_GRAPHICS_RENDER_INTESCANLINE, "Interpolated Scanline");

    if (Have_MMX)
    {
        MENU_L(GraphicsRender, i++, MF_BYPOSITION | MF_STRING | (((Rend == 8) ? MF_CHECKED : MF_UNCHECKED)), ID_GRAPHICS_RENDER_INT50SCANLIN, "Interpolated 50% Scanline");
        MENU_L(GraphicsRender, i++, MF_BYPOSITION | (((Rend == 9) ? MF_CHECKED : MF_UNCHECKED)), ID_GRAPHICS_RENDER_INT25SCANLIN, "Interpolated 25% Scanline");
    }

    InsertMenu(GraphicsRender, i++, MF_SEPARATOR, NULL, NULL);

    MENU_L(GraphicsRender, i++, MF_BYPOSITION | ((Rend > 0) ? MF_ENABLED : MF_DISABLED | MF_GRAYED), ID_GRAPHICS_PREVIOUS_RENDER, "Previous Render Mode");
    MENU_L(GraphicsRender, i++, MF_BYPOSITION | ((Rend != 9) ? MF_ENABLED : MF_DISABLED | MF_GRAYED), ID_GRAPHICS_NEXT_RENDER, "Next Render Mode");

    // SIZE //

    i = 0;

    MENU_L(GraphicsSize, i++, MF_BYPOSITION | ((ScaleFactor == 1.0) ? MF_CHECKED : MF_UNCHECKED), ID_GRAPHICS_SIZE_1X, "&1x");
    MENU_L(GraphicsSize, i++, MF_BYPOSITION | ((ScaleFactor == 2.0) ? MF_CHECKED : MF_UNCHECKED), ID_GRAPHICS_SIZE_2X, "&2x");
    MENU_L(GraphicsSize, i++, MF_BYPOSITION | ((ScaleFactor == 3.0) ? MF_CHECKED : MF_UNCHECKED), ID_GRAPHICS_SIZE_3X, "&3x");
    MENU_L(GraphicsSize, i++, MF_BYPOSITION | ((ScaleFactor == 4.0) ? MF_CHECKED : MF_UNCHECKED), ID_GRAPHICS_SIZE_4X, "4x");

    // LAYERS //

    // Nitsuja Added this

    i = 0;

    MENU_L(GraphicsLayers, i++, Flags | MF_POPUP, (UINT)GraphicsLayersA, "Scroll &A");
    MENU_L(GraphicsLayers, i++, Flags | MF_POPUP, (UINT)GraphicsLayersB, "Scroll &B");
    MENU_L(GraphicsLayers, i++, Flags | MF_POPUP, (UINT)GraphicsLayersS, "&Sprites");

    // LAYERS SUBMENUS //

    i = 0;

    MENU_L(GraphicsLayersA, i, MF_BYPOSITION | (VScrollAl ? MF_CHECKED : MF_UNCHECKED), ID_GRAPHICS_LAYER0, "Scroll A &Low");
    MENU_L(GraphicsLayersB, i, MF_BYPOSITION | (VScrollBl ? MF_CHECKED : MF_UNCHECKED), ID_GRAPHICS_LAYER1, "Scroll B &Low");
    MENU_L(GraphicsLayersS, i++, MF_BYPOSITION | (VSpritel ? MF_CHECKED : MF_UNCHECKED), ID_GRAPHICS_LAYERSPRITE, "Sprites &Low");

    MENU_L(GraphicsLayersA, i, MF_BYPOSITION | (VScrollAh ? MF_CHECKED : MF_UNCHECKED), ID_GRAPHICS_LAYER2, "Scroll A &High");
    MENU_L(GraphicsLayersB, i, MF_BYPOSITION | (VScrollBh ? MF_CHECKED : MF_UNCHECKED), ID_GRAPHICS_LAYER3, "Scroll B &High");
    MENU_L(GraphicsLayersS, i++, MF_BYPOSITION | (VSpriteh ? MF_CHECKED : MF_UNCHECKED), ID_GRAPHICS_LAYERSPRITEHIGH, "Sprites &High");

    MENU_L(GraphicsLayersA, i, MF_BYPOSITION | (Swap_Scroll_PriorityA ? MF_CHECKED : MF_UNCHECKED), ID_GRAPHICS_LAYERSWAPA, "&Swap Scroll Layers");
    MENU_L(GraphicsLayersB, i, MF_BYPOSITION | (Swap_Scroll_PriorityB ? MF_CHECKED : MF_UNCHECKED), ID_GRAPHICS_LAYERSWAPB, "&Swap Scroll Layers");
    MENU_L(GraphicsLayersS, i++, MF_BYPOSITION | (Swap_Sprite_Priority ? MF_CHECKED : MF_UNCHECKED), ID_GRAPHICS_LAYERSWAPS, "&Swap Sprite Layers");

    MENU_L(GraphicsLayersA, i, MF_BYPOSITION | (ScrollAOn ? MF_CHECKED : MF_UNCHECKED), ID_GRAPHICS_TOGGLEA, "&Enable");
    MENU_L(GraphicsLayersB, i, MF_BYPOSITION | (ScrollBOn ? MF_CHECKED : MF_UNCHECKED), ID_GRAPHICS_TOGGLEB, "&Enable");
    MENU_L(GraphicsLayersS, i++, MF_BYPOSITION | (SpriteOn ? MF_CHECKED : MF_UNCHECKED), ID_GRAPHICS_TOGGLES, "&Enable");

    MENU_L(GraphicsLayersS, i++, MF_BYPOSITION | (Sprite_Always_Top ? MF_CHECKED : MF_UNCHECKED), ID_GRAPHICS_SPRITEALWAYS, "Sprites Always On &Top");
    MENU_L(GraphicsLayersS, i++, MF_BYPOSITION | (Sprite_Boxing ? MF_CHECKED : MF_UNCHECKED), ID_GRAPHICS_SPRITEBOXING, "Sprites Boxing");

    // LATENCY COMPENSATION //

    i = 0;
    Flags = MF_BYPOSITION | MF_STRING;

    MENU_L(GraphicsLatencyCompensation, i++, Flags | ((VideoLatencyCompensation <= 0) ? MF_CHECKED : MF_UNCHECKED), ID_LATENCY_COMPENSATION_0, "&0 (lightest/cheap/default)");
    MENU_L(GraphicsLatencyCompensation, i++, Flags | ((VideoLatencyCompensation == 1) ? MF_CHECKED : MF_UNCHECKED), ID_LATENCY_COMPENSATION_1, "&1 (best Lua GUI sync in some games)");
    MENU_L(GraphicsLatencyCompensation, i++, Flags | ((VideoLatencyCompensation == 2) ? MF_CHECKED : MF_UNCHECKED), ID_LATENCY_COMPENSATION_2, "&2 (responsive, recommended)");
    MENU_L(GraphicsLatencyCompensation, i++, Flags | ((VideoLatencyCompensation == 3) ? MF_CHECKED : MF_UNCHECKED), ID_LATENCY_COMPENSATION_3, "&3 (over-responsive)");
    MENU_L(GraphicsLatencyCompensation, i++, Flags | ((VideoLatencyCompensation == 4) ? MF_CHECKED : MF_UNCHECKED), ID_LATENCY_COMPENSATION_4, "&4 (heaviest/expensive)");
    /*  MENU_L(GraphicsLatencyCompensation, i++, Flags | ((VideoLatencyCompensation == 5) ? MF_CHECKED : MF_UNCHECKED), ID_LATENCY_COMPENSATION_5, "&5"); */

            // FRAME SKIP //

    Flags = MF_BYPOSITION | MF_STRING;
    MENU_L(GraphicsFrameSkip, 0, Flags | ((Frame_Skip == -1) ? MF_CHECKED : MF_UNCHECKED), ID_GRAPHICS_FRAMESKIP_AUTO, "&Auto");

    for (i = 0; i < 9; i++)
    {
        wsprintf(Str_Tmp, "&%d", i);
        InsertMenu(GraphicsFrameSkip, i + 1, Flags | ((Frame_Skip == i) ? MF_CHECKED : MF_UNCHECKED),
            ID_GRAPHICS_FRAMESKIP_0 + i, Str_Tmp);
    }

    ////////////////////////////////////////////////
    //                  CPU MENU                  //
    ////////////////////////////////////////////////

    i = 0;

    MENU_L(CPU, i++, Flags | MF_POPUP, (UINT)CPUCountry, "&Country");

    InsertMenu(CPU, i++, MF_SEPARATOR, NULL, NULL);

    MENU_L(CPU, i++, Flags, ID_CPU_RESET, "&Hard Reset\t\tCtrl+Shift+R");

    MENU_L(CPU, i++, Flags, ID_CPU_RESET68K, "Reset &68000");

    MENU_L(CPU, i++, Flags, ID_CPU_RESETZ80, "Reset &Z80");

    // COUNTRY //

    Flags = MF_BYPOSITION | MF_STRING;

    MENU_L(CPUCountry, 0, Flags | (Country == -1 ? MF_CHECKED : MF_UNCHECKED), ID_CPU_COUNTRY_AUTO, "&Auto detect");
    MENU_L(CPUCountry, 1, Flags | (Country == 0 ? MF_CHECKED : MF_UNCHECKED), ID_CPU_COUNTRY_JAPAN, "&Japan (NTSC)");
    MENU_L(CPUCountry, 2, Flags | (Country == 1 ? MF_CHECKED : MF_UNCHECKED), ID_CPU_COUNTRY_USA, "&USA (NTSC)");
    MENU_L(CPUCountry, 3, Flags | (Country == 2 ? MF_CHECKED : MF_UNCHECKED), ID_CPU_COUNTRY_EUROPE, "&Europe (PAL)");
    MENU_L(CPUCountry, 4, Flags | (Country == 3 ? MF_CHECKED : MF_UNCHECKED), ID_CPU_COUNTRY_MISC, "Japan (PAL)");

    InsertMenu(CPUCountry, 5, MF_SEPARATOR, NULL, NULL);

    MENU_L(CPUCountry, 6, Flags | MF_POPUP, (UINT)CPUCountryOrder, "&Auto detection order");

    // PREFERED COUNTRY //

    for (i = 0; i < 3; i++)
    {
        if (Country_Order[i] == 0)
            MENU_L(CPUCountryOrder, i, Flags, ID_CPU_COUNTRY_ORDER + i, "&USA (NTSC)");
        else if (Country_Order[i] == 1)
            MENU_L(CPUCountryOrder, i, Flags, ID_CPU_COUNTRY_ORDER + i, "&Japan (NTSC)");
        else
            MENU_L(CPUCountryOrder, i, Flags, ID_CPU_COUNTRY_ORDER + i, "&Europe (PAL)");
    }

    //////////////////////////////////////////////////
    //                  SOUND MENU                  //
    //////////////////////////////////////////////////

    i = 0;

    MENU_L(Sound, i++, Flags | MF_POPUP, (UINT)SoundRate, "&Rate");
    MENU_L(Sound, i++, Flags | (Sound_Stereo ? MF_CHECKED : MF_UNCHECKED), ID_SOUND_STEREO, "&Stereo");
    MENU_L(Sound, i++, Flags | (Sound_Soften ? MF_CHECKED : MF_UNCHECKED), ID_SOUND_SOFTEN, "Soften &Filter"); // Modif N.
    MENU_L(Sound, i++, Flags | (!Sleep_Time ? MF_CHECKED : MF_UNCHECKED), ID_SOUND_HOG, "&Hog CPU"); // Modif N.

    // RATE //

    InsertMenu(SoundRate, 0, Flags | (Sound_Rate == 11025 ? MF_CHECKED : MF_UNCHECKED),
        ID_SOUND_RATE_11000, "&11025");
    InsertMenu(SoundRate, 1, Flags | (Sound_Rate == 22050 ? MF_CHECKED : MF_UNCHECKED),
        ID_SOUND_RATE_22000, "&22050");
    InsertMenu(SoundRate, 2, Flags | (Sound_Rate == 44100 ? MF_CHECKED : MF_UNCHECKED),
        ID_SOUND_RATE_44000, "&44100");

    //////////////////////////////////////////////////
    //                  TOOLS MENU                  //
    //////////////////////////////////////////////////

    // TAS_Tools menus by Upthorn

    MENU_L(TAS_Tools, i++, MF_BYPOSITION | MF_POPUP | MF_STRING, (UINT)Tools_Movies, "&Movie");

    InsertMenu(TAS_Tools, i++, MF_SEPARATOR, NULL, NULL);

    MENU_L(TAS_Tools, i++, MF_BYPOSITION | MF_POPUP | MF_STRING, (UINT)Lua_Script, "&Lua Scripting");

    InsertMenu(TAS_Tools, i++, MF_SEPARATOR, NULL, NULL);

    MENU_L(TAS_Tools, i++, MF_BYPOSITION | MF_POPUP | MF_STRING, (UINT)CPUSlowDownSpeed, "S&low Mode");

    InsertMenu(TAS_Tools, i++, MF_SEPARATOR, NULL, NULL);

    MENU_L(TAS_Tools, i++, Flags, ID_RAM_WATCH, "RAM &Watch");   //Modif U.
    MENU_L(TAS_Tools, i++, Flags, ID_RAM_SEARCH, "&RAM Search"); //Modif N.
    MENU_L(TAS_Tools, i++, Flags, ID_HEX_EDITOR, "&Hex Editor");
#ifdef DEBUG_68K
    MENU_L(TAS_Tools, i++, Flags, ID_VDP_RAM, "VDP RAM");
    MENU_L(TAS_Tools, i++, Flags, ID_VDP_SPRITES, "VDP Sprites");
    MENU_L(TAS_Tools, i++, Flags, ID_PLANE_EXPLORER, "Plane Explorer");
#else
    MENU_L(TAS_Tools, i++, Flags, ID_YM2612_DEBUGGER, "&YM2612 && PSG View");
#endif

    // MOVIES //

    i = 0;

    if (strcmp(Recent_Movie[0], ""))
    {
        MENU_L(Tools_Movies, i++, MF_BYPOSITION | MF_POPUP | MF_STRING, (UINT)MoviesHistory, "&Movie History");

        InsertMenu(Tools_Movies, i++, MF_SEPARATOR, NULL, NULL);
    }

    MENU_L(Tools_Movies, i++, Flags | ((MainMovie.Status == MOVIE_PLAYING) ? MF_CHECKED : MF_UNCHECKED), ID_PLAY_MOVIE, "&Play Movie" /*" or Resume record from savestate"*/); //Modif
    MENU_L(Tools_Movies, i++, Flags | ((MainMovie.Status) ? MF_ENABLED : MF_DISABLED | MF_GRAYED), ID_PLAY_FROM_START, "&Watch From Beginning"); //Modif N.

    InsertMenu(Tools_Movies, i++, MF_SEPARATOR, NULL, NULL);

    MENU_L(Tools_Movies, i++, Flags | ((MainMovie.Status == MOVIE_RECORDING) ? MF_CHECKED : MF_UNCHECKED), ID_RECORD_MOVIE, "Record &New Movie"); //Modif
    MENU_L(Tools_Movies, i++, Flags | ((MainMovie.Status) ? MF_ENABLED : MF_DISABLED | MF_GRAYED), ID_RESUME_RECORD, "&Resume Record from Now"); //Modif

    InsertMenu(Tools_Movies, i++, MF_SEPARATOR, NULL, NULL);

    MENU_L(Tools_Movies, i++, Flags | ((SpliceFrame) ? MF_CHECKED : MF_UNCHECKED) | ((MainMovie.File) ? MF_ENABLED : MF_DISABLED | MF_GRAYED), ID_SPLICE, "&Input Splice\t\tShift-S"); //Modif
    MENU_L(Tools_Movies, i++, Flags | ((SeekFrame) ? MF_CHECKED : MF_UNCHECKED) | ((MainMovie.File) ? MF_ENABLED : MF_DISABLED | MF_GRAYED), IDC_SEEK_FRAME, "Seek to &Frame"); //Modif
    MENU_L(Tools_Movies, i++, MF_BYPOSITION | MF_POPUP | MF_STRING | (MainMovie.Status ? MF_ENABLED : (MF_DISABLED | MF_GRAYED)), (UINT)Movies_Tracks, "&Tracks"); //Modif

    InsertMenu(Tools_Movies, i++, MF_SEPARATOR, NULL, NULL);

    MENU_L(Tools_Movies, i++, ((MainMovie.File != NULL) ? Flags : (Flags | MF_DISABLED | MF_GRAYED)), ID_STOP_MOVIE, "&Stop Movie");

    // HISTORY //

    for (i = 0; i < MAX_RECENT_MOVIES; i++)
    {
        if (strcmp(Recent_Movie[i], ""))
        {
            char tmp1[1024];
            Get_Name_From_Path(Recent_Movie[i], Str_Tmp);
            strcpy(tmp1, Str_Tmp);
            // & is an escape sequence in windows menu names, so replace & with &&
            int len = strlen(tmp1);
            for (int j = 0; j < len && len < 1023; j++)
                if (tmp1[j] == '&')
                    memmove(tmp1 + j + 1, tmp1 + j, strlen(tmp1 + j) + 1), ++len, ++j;

            MENU_L(MoviesHistory, i, Flags, ID_TOOLS_OPENRECENTMOVIE0 + i, tmp1);
        }
        else break;
    }

    // TRACKS //

    i = 0;

    MENU_L(Movies_Tracks, i++, Flags, ID_MOVIE_CHANGETRACK_ALL, "&All Players\t\tCtrl-Shift-0"); //Modif
    MENU_L(Movies_Tracks, i++, Flags | ((track & TRACK1) ? MF_CHECKED : MF_UNCHECKED), ID_MOVIE_CHANGETRACK_1, "Players &1\t\tCtrl-Shift-1"); //Modif
    MENU_L(Movies_Tracks, i++, Flags | ((track & TRACK2) ? MF_CHECKED : MF_UNCHECKED), ID_MOVIE_CHANGETRACK_2, "Players &2\t\tCtrl-Shift-2"); //Modif
    MENU_L(Movies_Tracks, i++, Flags | (MainMovie.TriplePlayerHack ? MF_ENABLED : MF_DISABLED | MF_GRAYED) | ((track & TRACK3) ? MF_CHECKED : MF_UNCHECKED), ID_MOVIE_CHANGETRACK_3, "Players &3\t\tCtrl-Shift-3"); //Modif

    // LUA SCRIPT //

    i = 0;

    MENU_L(Lua_Script, i++, Flags, IDC_NEW_LUA_SCRIPT, "&New Lua Script Window...");
    MENU_L(Lua_Script, i++, Flags | (!LuaScriptHWnds.empty() ? MF_ENABLED : MF_DISABLED | MF_GRAYED), IDC_CLOSE_LUA_SCRIPTS, "&Close All Lua Windows");

    if (!LuaScriptHWnds.empty())
    {
        InsertMenu(Lua_Script, i++, MF_SEPARATOR, NULL, NULL);

        for (unsigned int j = 0; j < LuaScriptHWnds.size(); j++)
        {
            GetWindowText(LuaScriptHWnds[j], Str_Tmp, 1024);
            MENU_L(Lua_Script, i++, Flags, IDC_LUA_SCRIPT_0 + j, Str_Tmp);
        }
    }
    {
        int dividerI = i;
        for (unsigned int j = 0; j < MAX_RECENT_SCRIPTS; j++)
        {
            const char* pathPtr = Recent_Scripts[j];
            if (!*pathPtr) continue;
            HWND IsScriptFileOpen(const char* Path);
            if (IsScriptFileOpen(pathPtr)) continue;
            // only show some of the path
            const char* pathPtrSearch;
            int slashesLeft = 2;
            for (pathPtrSearch = pathPtr + strlen(pathPtr) - 1;
                pathPtrSearch != pathPtr && slashesLeft >= 0;
                pathPtrSearch--)
            {
                char c = *pathPtrSearch;
                if (c == '\\' || c == '/')
                    slashesLeft--;
            }
            if (slashesLeft < 0)
                pathPtr = pathPtrSearch + 2;
            strcpy(Str_Tmp, pathPtr);
            if (i == dividerI)
                InsertMenu(Lua_Script, i++, MF_SEPARATOR, NULL, NULL);
            MENU_L(Lua_Script, i++, Flags, ID_LUA_OPENRECENTSCRIPT0 + j, Str_Tmp);
        }
    }

    // CPU SLOW DOWN SPEED //

    // Upth-Modif - Slow Mode Selection -- now a submenu of TAS_Tools

    i = 0;

    MENU_L(CPUSlowDownSpeed, i++, Flags | ((SlowDownMode == 1) ? MF_ENABLED : MF_DISABLED | MF_GRAYED), ID_SLOW_SPEED_PLUS, "Speed &Up"); //Modif N.
    MENU_L(CPUSlowDownSpeed, i++, Flags | ((SlowDownMode == 0 || SlowDownSpeed < 31) ? MF_ENABLED : MF_DISABLED | MF_GRAYED), ID_SLOW_SPEED_MINUS, "Slow &Down"); //Modif N.

    InsertMenu(CPUSlowDownSpeed, i++, MF_SEPARATOR, NULL, NULL);

    MENU_L(CPUSlowDownSpeed, i++, Flags | ((SlowDownMode == 1) ? MF_CHECKED : MF_UNCHECKED), ID_SLOW_MODE, "&Slow Mode Enabled"); //Modif
    MENU_L(CPUSlowDownSpeed, i++, Flags | ((SlowDownSpeed == 1) ? MF_CHECKED : MF_UNCHECKED), ID_SLOW_SPEED_1, "50%");
    MENU_L(CPUSlowDownSpeed, i++, Flags | ((SlowDownSpeed == 2) ? MF_CHECKED : MF_UNCHECKED), ID_SLOW_SPEED_2, "33%");
    MENU_L(CPUSlowDownSpeed, i++, Flags | ((SlowDownSpeed == 3) ? MF_CHECKED : MF_UNCHECKED), ID_SLOW_SPEED_3, "25%");
    MENU_L(CPUSlowDownSpeed, i++, Flags | ((SlowDownSpeed == 4) ? MF_CHECKED : MF_UNCHECKED), ID_SLOW_SPEED_4, "20%");
    MENU_L(CPUSlowDownSpeed, i++, Flags | ((SlowDownSpeed == 5) ? MF_CHECKED : MF_UNCHECKED), ID_SLOW_SPEED_5, "16%");
    MENU_L(CPUSlowDownSpeed, i++, Flags | ((SlowDownSpeed == 9) ? MF_CHECKED : MF_UNCHECKED), ID_SLOW_SPEED_9, "10%");
    MENU_L(CPUSlowDownSpeed, i++, Flags | ((SlowDownSpeed == 15) ? MF_CHECKED : MF_UNCHECKED), ID_SLOW_SPEED_15, " 6%");
    MENU_L(CPUSlowDownSpeed, i, Flags | ((SlowDownSpeed == 31) ? MF_CHECKED : MF_UNCHECKED), ID_SLOW_SPEED_31, " 3%");

    ////////////////////////////////////////////////////
    //                  OPTIONS MENU                  //
    ////////////////////////////////////////////////////

    i = 0;

    MENU_L(Options, i++, Flags, ID_OPTIONS_JOYPADSETTING, "&Input...");
    MENU_L(Options, i++, MF_BYPOSITION | MF_STRING, ID_OPTIONS_GENERAL, "&General..."); // Modif N: changed Misc... to General...
    MENU_L(Options, i++, Flags, ID_OPTIONS_CHANGEDIR, "&Directories...");
    MENU_L(Options, i++, Flags, ID_OPTIONS_CHANGEFILES, "Bios/Misc &Files...");

    InsertMenu(Options, i++, MF_SEPARATOR, NULL, NULL);

    MENU_L(Options, i++, Flags, ID_OPTIONS_LOADCONFIG, "&Load Config...");
    MENU_L(Options, i++, Flags, ID_OPTIONS_SAVEASCONFIG, "&Save Config As...");

    /////////////////////////////////////////////////
    //                  HELP MENU                  //
    /////////////////////////////////////////////////

    i = 0;

    MENU_L(Help, i++, Flags, ID_HELP_ABOUT, "&About");

    ///////////////////////////////////////////
    //                  END                  //
    ///////////////////////////////////////////

    Gens_Menu = MainMenu;
    if (Full_Screen) SetMenu(HWnd, NULL);
    else SetMenu(HWnd, Gens_Menu);
    MustUpdateMenu = 0;

    // measure the desired width of the menu in pixels,
    // such that the menu will only be 1 line tall if the surrounding window is at least this wide
    // (you would think this would be a simple thing to do, but no...)
    NONCLIENTMETRICS ncm = { 0 };
    ncm.cbSize = sizeof(NONCLIENTMETRICS);
    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    HDC hdc = GetDC(HWnd);
    HFONT menuFont = CreateFontIndirect(&ncm.lfMenuFont);
    HFONT oldFont = (HFONT)SelectObject(hdc, (HGDIOBJ)menuFont);
    SIZE size = { 0 };
    GetTextExtentPoint32(hdc, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 52, &size);
    int extraSpace = (size.cx / 26 + 1) / 2;
    Gens_Menu_Width = extraSpace;
    int numItems = GetMenuItemCount(Gens_Menu);
    for (int i = 0; i < numItems; i++)
    {
        char str[256];
        GetMenuString(Gens_Menu, i, str, sizeof(str), MF_BYPOSITION);
        for (int i = 0; i < (int)strlen(str); i++) // the & symbol is an escape sequence for underline, so remove & (and convert && to &) before measuring
        {
            if (str[i] == str[i + 1])
                i++;
            if (str[i] == '&')
                strcpy(str + i, str + i + 1), i--;
        }
        if (GetTextExtentPoint32(hdc, str, strlen(str), &size))
            Gens_Menu_Width += size.cx + (2 * extraSpace);
    }
    SelectObject(hdc, (HGDIOBJ)oldFont);
    DeleteObject((HGDIOBJ)menuFont);
    return(Gens_Menu);
}

LRESULT CALLBACK GGenieProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    RECT r;
    RECT r2;
    int dx1, dy1, dx2, dy2, i, value;
    char tmp[1024];

    switch (uMsg)
    {
    case WM_INITDIALOG:
        GetWindowRect(HWnd, &r);
        dx1 = (r.right - r.left) / 2;
        dy1 = (r.bottom - r.top) / 2;

        GetWindowRect(hDlg, &r2);
        dx2 = (r2.right - r2.left) / 2;
        dy2 = (r2.bottom - r2.top) / 2;

        SetWindowPos(hDlg, NULL, std::max(0L, r.left + (dx1 - dx2)), std::max(0L, r.top + (dy1 - dy2)), NULL, NULL, SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);

        WORD_L(IDC_INFO_GG, "Informations about GG/Patch codes");
        WORD_L(IDC_GGINFO1, "Both Game Genie code and Patch code are supported.");
        WORD_L(IDC_GGINFO2, "Highlight a code to activate it.");
        WORD_L(IDC_GGINFO3, "Syntax for Game Genie code :  XXXX-XXXX");
        WORD_L(IDC_GGINFO4, "Syntax for Patch code :  XXXXXX:YYYY    (address:data)");

        WORD_L(ID_GGADD, "Add &code");
        WORD_L(ID_GGREMOVE, "&Remove selected codes");
        WORD_L(ID_GGDEACTIVATE, "&Deactivate all codes");
        WORD_L(ID_OK, "&OK");
        WORD_L(ID_CANCEL, "&Cancel");

        for (i = 0; i < 256; i++)
        {
            if (Liste_GG[i].code[0] != 0)
            {
                strcpy(Str_Tmp, Liste_GG[i].code);
                while (strlen(Str_Tmp) < 20) strcat(Str_Tmp, " ");
                strcat(Str_Tmp, Liste_GG[i].name);

                SendDlgItemMessage(hDlg, IDC_LIST1, LB_ADDSTRING, (WPARAM)0, (LONG)(LPTSTR)Str_Tmp);

                if (Liste_GG[i].active)
                    SendDlgItemMessage(hDlg, IDC_LIST1, LB_SETSEL, (WPARAM)1, (LONG)i);
                else
                    SendDlgItemMessage(hDlg, IDC_LIST1, LB_SETSEL, (WPARAM)0, (LONG)i);

                if ((Liste_GG[i].restore != 0xFFFFFFFF) && (Liste_GG[i].addr < Rom_Size) && (Genesis_Started))
                {
                    Rom_Data[Liste_GG[i].addr] = (unsigned char)(Liste_GG[i].restore & 0xFF);
                    Rom_Data[Liste_GG[i].addr + 1] = (unsigned char)((Liste_GG[i].restore & 0xFF00) >> 8);
                }
            }
        }
        return true;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case ID_GGADD:
            if (GetDlgItemText(hDlg, IDC_EDIT1, Str_Tmp, 14))
            {
                size_t Str_Tmp_len = strlen(Str_Tmp);
                if ((Str_Tmp_len == 9) || (Str_Tmp_len == 11))
                {
                    strupr(Str_Tmp);
                    while (strlen(Str_Tmp) < 20) strcat(Str_Tmp, " ");

                    GetDlgItemText(hDlg, IDC_EDIT2, (char *)(Str_Tmp + strlen(Str_Tmp)), 240);

                    SendDlgItemMessage(hDlg, IDC_LIST1, LB_ADDSTRING, (WPARAM)0, (LONG)(LPTSTR)Str_Tmp);

                    SetDlgItemText(hDlg, IDC_EDIT1, "");
                    SetDlgItemText(hDlg, IDC_EDIT2, "");
                }
            }
            return true;

        case ID_GGREMOVE:
            value = SendDlgItemMessage(hDlg, IDC_LIST1, LB_GETCOUNT, (WPARAM)0, (LPARAM)0);
            if (value == LB_ERR) value = 0;

            for (i = value - 1; i >= 0; i--)
            {
                if (SendDlgItemMessage(hDlg, IDC_LIST1, LB_GETSEL, (WPARAM)i, NULL) > 0)
                    SendDlgItemMessage(hDlg, IDC_LIST1, LB_DELETESTRING, (WPARAM)i, (LPARAM)0);
            }
            return true;

        case ID_GGDEACTIVATE:
            value = SendDlgItemMessage(hDlg, IDC_LIST1, LB_GETCOUNT, (WPARAM)0, (LPARAM)0);
            if (value == LB_ERR) value = 0;

            for (i = value - 1; i >= 0; i--)
            {
                SendDlgItemMessage(hDlg, IDC_LIST1, LB_SETSEL, (WPARAM)0, (LPARAM)i);
            }
            return true;

        case ID_OK:
            value = SendDlgItemMessage(hDlg, IDC_LIST1, LB_GETCOUNT, (WPARAM)0, (LPARAM)0);
            if (value == LB_ERR) value = 0;

            for (i = 0; i < 256; i++)
            {
                Liste_GG[i].code[0] = 0;
                Liste_GG[i].name[0] = 0;
                Liste_GG[i].active = 0;
                Liste_GG[i].addr = 0xFFFFFFFF;
                Liste_GG[i].data = 0;
                Liste_GG[i].restore = 0xFFFFFFFF;
            }
            List_GG_Max_Active_Index = 0;

            if (value > 256) value = 256;
            for (i = 0; i < value; i++)
            {
                if (SendDlgItemMessage(hDlg, IDC_LIST1, LB_GETTEXT, (WPARAM)i, (LONG)(LPTSTR)tmp) != LB_ERR)
                {
                    dx1 = 0;

                    while ((tmp[dx1] != ' ') && (tmp[dx1] != 0)) dx1++;

                    memcpy(Liste_GG[i].code, tmp, dx1);
                    Liste_GG[i].code[dx1] = 0;

                    while ((tmp[dx1] == ' ') && (tmp[dx1] != 0)) dx1++;

                    strcpy(Liste_GG[i].name, (char *)(tmp + dx1));

                    if (SendDlgItemMessage(hDlg, IDC_LIST1, LB_GETSEL, (WPARAM)i, NULL) > 0)
                    {
                        Liste_GG[i].active = 1;
                        List_GG_Max_Active_Index = i + 1;
                    }
                    else Liste_GG[i].active = 0;
                }
            }

            for (i = 0; i < value; i++)
            {
                if ((Liste_GG[i].code[0] != 0) && (Liste_GG[i].addr == 0xFFFFFFFF) && (Liste_GG[i].data == 0))
                {
                    decode(Liste_GG[i].code, (patch *)(&(Liste_GG[i].addr)));

                    if ((Liste_GG[i].restore == 0xFFFFFFFF) && (Liste_GG[i].addr < Rom_Size) && (Genesis_Started))
                    {
                        Liste_GG[i].restore = (unsigned int)(Rom_Data[Liste_GG[i].addr] & 0xFF);
                        Liste_GG[i].restore += (unsigned int)((Rom_Data[Liste_GG[i].addr + 1] & 0xFF) << 8);
                    }
                }
            }

        case ID_CANCEL:
        case IDCANCEL:
            DialogsOpen--;
            EndDialog(hDlg, true);
            return true;
        }
        break;

    case WM_CLOSE:
        DialogsOpen--;
        EndDialog(hDlg, true);
        return true;
    }

    return false;
}

LRESULT CALLBACK DirectoriesProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    char Str_Tmp2[1024];

    switch (uMsg)
    {
    case WM_INITDIALOG:
        RECT r;
        RECT r2;
        int dx1, dy1, dx2, dy2;

        GetWindowRect(HWnd, &r);
        dx1 = (r.right - r.left) / 2;
        dy1 = (r.bottom - r.top) / 2;

        GetWindowRect(hDlg, &r2);
        dx2 = (r2.right - r2.left) / 2;
        dy2 = (r2.bottom - r2.top) / 2;

        SetWindowPos(hDlg, NULL, std::max(0L, r.left + (dx1 - dx2)), std::max(0L, r.top + (dy1 - dy2)), NULL, NULL, SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);

        WORD_L(IDD_DIRECTORIES, "Directories configuration");
        WORD_L(IDC_DIRECTORIES, "Configure directories");

        WORD_L(ID_CANCEL, "&Cancel");
        WORD_L(ID_OK, "&OK");

        WORD_L(ID_CHANGE_SAVE, "Change");
        WORD_L(ID_CHANGE_SRAM, "Change");
        WORD_L(ID_CHANGE_PATCH, "Change");
        WORD_L(ID_CHANGE_IPS, "Change");
        WORD_L(ID_CHANGE_MOVIE, "Change");
        WORD_L(ID_CHANGE_LUA, "Change");

        WORD_L(IDC_STATIC_SAVE, "SAVE STATE");
        WORD_L(IDC_STATIC_SRAM, "SRAM BACKUP");
        WORD_L(IDC_STATIC_PATCH, "PAT PATCH");
        WORD_L(IDC_STATIC_IPS, "IPS PATCH");
        WORD_L(IDC_STATIC_LUA, "LUA SCRIPT");

        SetDlgItemText(hDlg, IDC_EDIT_SAVE, State_Dir);
        SetDlgItemText(hDlg, IDC_EDIT_SRAM, SRAM_Dir);
        SetDlgItemText(hDlg, IDC_EDIT_PATCH, Patch_Dir);
        SetDlgItemText(hDlg, IDC_EDIT_IPS, IPS_Dir);
        SetDlgItemText(hDlg, IDC_EDIT_MOVIE, Movie_Dir);
        SetDlgItemText(hDlg, IDC_EDIT_WATCH, Watch_Dir);
        SetDlgItemText(hDlg, IDC_EDIT_LUA, Lua_Dir);

        return true;
        break;

    case WM_COMMAND:
        switch (wParam)
        {
        case ID_CHANGE_SAVE:
            GetDlgItemText(hDlg, IDC_EDIT_SAVE, Str_Tmp2, 1024);
            if (Change_Dir(Str_Tmp, Str_Tmp2, "Save state directory", "Save state files\0*.gs?\0\0", "gs0", hDlg))
                SetDlgItemText(hDlg, IDC_EDIT_SAVE, Str_Tmp);
            break;

        case ID_CHANGE_SRAM:
            GetDlgItemText(hDlg, IDC_EDIT_SRAM, Str_Tmp2, 1024);
            if (Change_Dir(Str_Tmp, Str_Tmp2, "SRAM backup directory", "SRAM backup files\0*.srm\0\0", "srm", hDlg))
                SetDlgItemText(hDlg, IDC_EDIT_SRAM, Str_Tmp);
            break;

        case ID_CHANGE_PATCH:
            GetDlgItemText(hDlg, IDC_EDIT_PATCH, Str_Tmp2, 1024);
            if (Change_Dir(Str_Tmp, Str_Tmp2, "PAT Patch directory", "PAT Patch files\0*.pat\0\0", "pat", hDlg))
                SetDlgItemText(hDlg, IDC_EDIT_PATCH, Str_Tmp);
            break;

        case ID_CHANGE_IPS:
            GetDlgItemText(hDlg, IDC_EDIT_IPS, Str_Tmp2, 1024);
            if (Change_Dir(Str_Tmp, Str_Tmp2, "IPS Patch directory", "IPS Patch files\0*.ips\0\0", "ips", hDlg))
                SetDlgItemText(hDlg, IDC_EDIT_IPS, Str_Tmp);
            break;
        case ID_CHANGE_LUA:
            GetDlgItemText(hDlg, IDC_EDIT_LUA, Str_Tmp2, 1024);
            if (Change_Dir(Str_Tmp, Str_Tmp2, "Lua script directory", "Lua script files\0*.lua\0\0", "lua", hDlg))
                SetDlgItemText(hDlg, IDC_EDIT_LUA, Str_Tmp);
            break;
        case ID_CHANGE_MOVIE:
            GetDlgItemText(hDlg, IDC_EDIT_MOVIE, Str_Tmp2, 1024);
            if (Change_Dir(Str_Tmp, Str_Tmp2, "Movie directory", "Gens Movie files\0*.gmv\0\0", "gmv", hDlg))
                SetDlgItemText(hDlg, IDC_EDIT_MOVIE, Str_Tmp);
            break;
        case ID_CHANGE_WATCH:
            GetDlgItemText(hDlg, IDC_EDIT_WATCH, Str_Tmp2, 1024);
            if (Change_Dir(Str_Tmp, Str_Tmp2, "Ram Watch directory", "Gens WATCH files\0*.wch\0\0", "wch", hDlg))
                SetDlgItemText(hDlg, IDC_EDIT_WATCH, Str_Tmp);
            break;
        case ID_OK:
            GetDlgItemText(hDlg, IDC_EDIT_SAVE, State_Dir, 1024);
            GetDlgItemText(hDlg, IDC_EDIT_SRAM, SRAM_Dir, 1024);
            GetDlgItemText(hDlg, IDC_EDIT_PATCH, Patch_Dir, 1024);
            GetDlgItemText(hDlg, IDC_EDIT_IPS, IPS_Dir, 1024);
            GetDlgItemText(hDlg, IDC_EDIT_MOVIE, Movie_Dir, 1024);
            GetDlgItemText(hDlg, IDC_EDIT_WATCH, Watch_Dir, 1024);
            GetDlgItemText(hDlg, IDC_EDIT_LUA, Lua_Dir, 1024);
        case ID_CANCEL:
        case IDCANCEL:
            DialogsOpen--;
            EndDialog(hDlg, true);
            return true;
        }
        break;

    case WM_CLOSE:
        DialogsOpen--;
        EndDialog(hDlg, true);
        return true;
    }

    return false;
}

LRESULT CALLBACK FilesProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    char Str_Tmp2[1024];
    switch (uMsg)
    {
    case WM_INITDIALOG:
        RECT r;
        RECT r2;
        int dx1, dy1, dx2, dy2;

        GetWindowRect(HWnd, &r);
        dx1 = (r.right - r.left) / 2;
        dy1 = (r.bottom - r.top) / 2;

        GetWindowRect(hDlg, &r2);
        dx2 = (r2.right - r2.left) / 2;
        dy2 = (r2.bottom - r2.top) / 2;

        SetWindowPos(hDlg, NULL, std::max(0L, r.left + (dx1 - dx2)), std::max(0L, r.top + (dy1 - dy2)), NULL, NULL, SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);

        WORD_L(IDD_FILES, "Files configuration");
        WORD_L(IDC_GENESISBIOS_FILE, "Configure Genesis bios file");
        WORD_L(IDC_MISC_FILES, "Configure misc file");

        WORD_L(ID_CANCEL, "&Cancel");
        WORD_L(ID_OK, "&OK");

        WORD_L(ID_CHANGE_GENESISBIOS, "Change");

        WORD_L(IDC_STATIC_GENESISBIOS, "Genesis");

        SetDlgItemText(hDlg, IDC_EDIT_GENESISBIOS, Genesis_Bios);

        return true;
        break;

    case WM_COMMAND:
        switch (wParam)
        {
        case ID_CHANGE_GENESISBIOS:
            GetDlgItemText(hDlg, IDC_EDIT_GENESISBIOS, Str_Tmp2, 1024);
            strcpy(Str_Tmp, "genesis.bin");
            DialogsOpen++;
            if (Change_File_S(Str_Tmp, Str_Tmp2, "Genesis bios file", "bios files\0*.bin\0All Files\0*.*\0\0", "bin", hDlg))
                SetDlgItemText(hDlg, IDC_EDIT_GENESISBIOS, Str_Tmp);
            DialogsOpen--;
            break;

        case ID_OK:
            GetDlgItemText(hDlg, IDC_EDIT_GENESISBIOS, Genesis_Bios, 1024);

        case ID_CANCEL:
        case IDCANCEL:
            DialogsOpen--;
            EndDialog(hDlg, true);
            return true;
        }
        break;

    case WM_CLOSE:
        DialogsOpen--;
        EndDialog(hDlg, true);
        return true;
    }

    return false;
}
int GetControlType(int i)
{
    switch (i)
    {
    case 0:
        return Controller_1_Type;
    case 1:
        return Controller_2_Type;
    case 2:
        return Controller_1B_Type;
    case 3:
        return Controller_1C_Type;
    case 4:
        return Controller_1D_Type;
    case 5:
        return Controller_2B_Type;
    case 6:
        return Controller_2C_Type;
    case 7:
        return Controller_2D_Type;
    default:
        return -1;
    }
}

static const char* PathWithoutPrefixDotOrSlash(const char* path)
{
    while (*path &&
        ((*path == '.' && (path[1] == '\\' || path[1] == '/')) ||
            *path == '\\' || *path == '/' || *path == ' '))
        path++;
    return path;
}

int LoadSubMovie(char* filename)
{
    // make the filename absolute before loading
    if (filename[0] && filename[1] != ':')
    {
        char tempFile[1024], curDir[1024];
        strncpy(tempFile, filename, 1024);
        tempFile[1023] = 0;
        const char* tempFilePtr = PathWithoutPrefixDotOrSlash(tempFile);
        if (!*tempFilePtr || tempFilePtr[1] != ':')
            strcpy(curDir, Gens_Path); // note: this should definitely not use Movie_Dir, since it only happens when Movie_Dir fails to provide an absolute path or has been temporarily overridden
        else
            curDir[0] = 0;
        _snprintf(filename, 1024, "%s%s", curDir, tempFilePtr);
    }

    int gmiRV = GetMovieInfo(filename, &SubMovie);

    if (gmiRV < 0)
        SubMovie.Ok = 0;

    if (!SubMovie.Ok)
    {
        bool wasClearSRAM = SubMovie.ClearSRAM;
        memset(&SubMovie, 0, sizeof(typeMovie));
        SubMovie.ClearSRAM = wasClearSRAM;
    }

    return gmiRV;
}

void PlaySubMovie()
{
    if (SubMovie.Ok)
    {
        if (MainMovie.Status) //Modif N - make sure to close existing movie, if any
            CloseMovieFile(&MainMovie);
        MainMovie.Status = 0;
        CopyMovie(&SubMovie, &MainMovie);

        if (GetFileAttributes(MainMovie.PhysicalFileName) & FILE_ATTRIBUTE_READONLY)
            MainMovie.ReadOnly = 2;

        //UpthAdd - Load Controller Settings from Movie File
        if (MainMovie.TriplePlayerHack) {
            Controller_1_Type |= 0x10;
            Controller_1B_Type &= ~1;
            Controller_1C_Type &= ~1;
        }
        else Controller_1_Type &= ~0x10;
        Controller_2_Type &= ~0x10;
        if (MainMovie.PlayerConfig[0] == 3) Controller_1_Type &= ~1;
        if (MainMovie.PlayerConfig[0] == 6) Controller_1_Type |= 1;
        if (MainMovie.PlayerConfig[1] == 3) Controller_2_Type &= ~1;
        if (MainMovie.PlayerConfig[1] == 6) Controller_2_Type |= 1;
        Track1_FrameCount = MainMovie.LastFrame;
        Track2_FrameCount = MainMovie.LastFrame;
        Track3_FrameCount = MainMovie.LastFrame;
        Make_IO_Table();
        MainMovie.Status = MOVIE_PLAYING;
        PlayMovieCanceled = 0;
    }
}

void PutSubMovieErrorInStr_Tmp(int gmiRV, const char* filename, char* header)
{
    if (!SubMovie.Ok)
    {
        switch (gmiRV)
        {
        case -1:
        {
            int err = errno;
            if (err == EINVAL)
                sprintf(Str_Tmp, "ERROR: File \"%s\" failed to load because: user cancelled or denied.", filename);
            else if (err == EMFILE)
                sprintf(Str_Tmp, "ERROR: File \"%s\" failed to load because: %s (%d).", filename, strerror(err), _getmaxstdio()); // can be triggered by a bug in one of the Visual Studio service packs that only happens in Debug configuration
            else
                sprintf(Str_Tmp, "ERROR: File \"%s\" failed to load because: %s.", filename, strerror(err));
        }
        break;
        case -2:
            sprintf(Str_Tmp, "ERROR: File \"%s\" is less than 64 bytes and thus cannot be a GMV.", filename);
            break;
        case -3:
        {
            for (int i = 0; i < 15; i++)
                if (!header[i])
                    strcpy(header + i, header + i + 1);
            sprintf(Str_Tmp, "ERROR: File \"%s\" starts with \"%s\" instead of \"Gens Movie\" and thus cannot be a GMV.", filename, header);
        }
        break;
        default:
            sprintf(Str_Tmp, "ERROR: File \"%s\" not loaded (reason unknown).", filename);
            break;
        }
    }
    else
    {
        sprintf(Str_Tmp, "ERROR: Problem loading savestate.");
    }
}

const char* MakeScriptPathAbsolute(const char* filename, const char* extraDirToCheck)
{
    static char filename2[1024];
    if (filename[0] && filename[1] != ':')
    {
        char tempFile[1024], curDir[1024];
        strncpy(tempFile, filename, 1024);
        tempFile[1023] = 0;
        const char* tempFilePtr = PathWithoutPrefixDotOrSlash(tempFile);
        for (int i = 0; i <= 4; i++)
        {
            if ((!*tempFilePtr || tempFilePtr[1] != ':') && i != 2)
                strcpy(curDir, i != 1 ? ((i != 3 || !extraDirToCheck) ? Lua_Dir : extraDirToCheck) : Gens_Path);
            else
                curDir[0] = 0;
            _snprintf(filename2, 1024, "%s%s", curDir, tempFilePtr);
            char* bar = strchr(filename2, '|');
            if (bar) *bar = 0;
            FILE* file = fopen(filename2, "rb");
            if (bar) *bar = '|';
            if (file || i == 4)
                filename = filename2;
            if (file)
            {
                fclose(file);
                break;
            }
        }
    }
    return filename;
}

const char* GensOpenScript(const char* filename, const char* extraDirToCheck, bool makeSubservient)
{
    if (LuaScriptHWnds.size() < 16)
    {
        // make the filename absolute before loading
        filename = MakeScriptPathAbsolute(filename, extraDirToCheck);

        // now check if it's already open and load it if it isn't
        HWND IsScriptFileOpen(const char* Path);
        HWND scriptHWnd = IsScriptFileOpen(filename);
        if (!scriptHWnd)
        {
            HWND prevWindow = GetActiveWindow();

            HWND hDlg = CreateDialog(ghInstance, MAKEINTRESOURCE(IDD_LUA), HWnd, (DLGPROC)LuaScriptProc);
            if (makeSubservient)
                SendMessage(hDlg, WM_COMMAND, IDC_NOTIFY_SUBSERVIENT, TRUE);
            SendDlgItemMessage(hDlg, IDC_EDIT_LUAPATH, WM_SETTEXT, 0, (LPARAM)filename);
            DialogsOpen++;

            SetActiveWindow(prevWindow);
        }
        else
        {
            RequestAbortLuaScript((int)scriptHWnd, "terminated to restart because of a call to gens.openscript");
            SendMessage(scriptHWnd, WM_COMMAND, IDC_BUTTON_LUARUN, 0);
        }
    }
    else return "Too many script windows are already open.";

    return NULL;
}

void GensOpenFile(const char* filename)
{
    // use ObtainFile to support opening files within archives
    char LogicalName[1024], PhysicalName[1024];
    strcpy(LogicalName, filename);
    strcpy(PhysicalName, filename);

    const char* fileExt = strrchr(LogicalName, '.');
    if (!fileExt++)
        fileExt = "";

    // guess what type of file it is
    GensFileType fileType = GuessFileType(PhysicalName, fileExt);

    // open the file in a way that depends on what type we decided it is
    switch (fileType)
    {
    case FILETYPE_MOVIE:
        GensPlayMovie(LogicalName);
        break;
    case FILETYPE_ROM:
        GensLoadRom(LogicalName);
        break;
    case FILETYPE_SCRIPT:
        GensOpenScript(LogicalName);
        break;
    case FILETYPE_SAVESTATE:
        if (Game)
            Load_State(PhysicalName);
        break;
    case FILETYPE_WATCH:
        Load_Watches(true, PhysicalName);
        break;
    case FILETYPE_CONFIG:
        if (FILE* file = fopen(PhysicalName, "rb"))
        {
            fclose(file);
            Load_Config(PhysicalName, Game);
            strcpy(Str_Tmp, "config loaded from ");
            strcat(Str_Tmp, LogicalName);
            Put_Info(Str_Tmp);
        }
        break;
    case FILETYPE_SRAM:
        if (Game)
            if (FILE* file = fopen(PhysicalName, "rb"))
            {
                fread(SRAM, 1, 64 * 1024, file);
                fclose(file);
                strcpy(Str_Tmp, "SRAM loaded from ");
                strcat(Str_Tmp, LogicalName);
                Put_Info(Str_Tmp);
            }
        break;
    }
}

const char* MakeRomPathAbsolute(const char* filename, const char* extraDirToCheck)
{
    static char filename2[1024];
    if (filename[0] && filename[1] != ':')
    {
        char tempFile[1024], curDir[1024];
        strncpy(tempFile, filename, 1024);
        tempFile[1023] = 0;
        const char* tempFilePtr = PathWithoutPrefixDotOrSlash(tempFile);
        for (int i = 0; i <= 3; i++)
        {
            switch (i)
            {
            case 0:
            case 3:
                if (Rom_Dir[1] == ':')
                    strcpy(curDir, Rom_Dir);
                else
                    _snprintf(curDir, 1024, "%s%s", Gens_Path, PathWithoutPrefixDotOrSlash(Rom_Dir));
                break;
            case 1:
                strcpy(curDir, Gens_Path);
                break;
            case 2:
                if (!extraDirToCheck || !extraDirToCheck[0])
                    continue;
                strncpy(curDir, extraDirToCheck, 1024);
                curDir[1023] = 0;
                break;
            }
            _snprintf(filename2, 1024, "%s%s", curDir, tempFilePtr);
            char* bar = strchr(filename2, '|');
            if (bar) *bar = 0;
            FILE* file = fopen(filename2, "rb");
            if (bar) *bar = '|';
            if (file || i == 3)
                filename = filename2;
            if (file)
            {
                fclose(file);
                break;
            }
        }
    }
    return filename;
}

LRESULT CALLBACK PlayMovieProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    RECT r;
    RECT r2;
    int dx1, dy1, dx2, dy2;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        if (Full_Screen)
        {
            while (ShowCursor(false) >= 0);
            while (ShowCursor(true) < 0);
        }

        GetWindowRect(HWnd, &r);
        dx1 = (r.right - r.left) / 2;
        dy1 = (r.bottom - r.top) / 2;

        GetWindowRect(hDlg, &r2);
        dx2 = (r2.right - r2.left) / 2;
        dy2 = (r2.bottom - r2.top) / 2;

        //SetWindowPos(hDlg, NULL, std::max(0, r.left + (dx1 - dx2)), std::max(0, r.top + (dy1 - dy2)), NULL, NULL, SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);
        SetWindowPos(hDlg, NULL, r.left, r.top, NULL, NULL, SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);

        SendDlgItemMessage(hDlg, IDC_RADIO_PLAY_START, BM_SETCHECK, (WPARAM)BST_CHECKED, 0);
        SendDlgItemMessage(hDlg, IDC_RADIO_PLAY_STATE, BM_SETCHECK, (WPARAM)BST_UNCHECKED, 0);
        SendDlgItemMessage(hDlg, IDC_CHECK_READ_ONLY, BM_SETCHECK, (WPARAM)BST_UNCHECKED, 0);
        SendDlgItemMessage(hDlg, IDC_CHECK_CLEAR_SRAM, BM_SETCHECK, (WPARAM)BST_CHECKED, 0); // Modif N. -- added checkbox

        if ((Controller_1_Type & 1) == 1)
            strcpy(Str_Tmp, "6 BUTTONS");
        else
            strcpy(Str_Tmp, "3 BUTTONS");
        SendDlgItemMessage(hDlg, IDC_STATIC_CON1_CURSET, WM_SETTEXT, 0, (LPARAM)Str_Tmp);

        if ((Controller_2_Type & 1) == 1)
            strcpy(Str_Tmp, "6 BUTTONS");
        else
            strcpy(Str_Tmp, "3 BUTTONS");
        SendDlgItemMessage(hDlg, IDC_STATIC_CON2_CURSET, WM_SETTEXT, 0, (LPARAM)Str_Tmp);
        strcpy(Str_Tmp, "");
        SendDlgItemMessage(hDlg, IDC_STATIC_SAVESTATEREQ, WM_SETTEXT, 0, (LPARAM)Str_Tmp);
        SendDlgItemMessage(hDlg, IDC_STATIC_3PLAYERS, WM_SETTEXT, 0, (LPARAM)Str_Tmp);

        InitMovie(&SubMovie);

        strncpy(Str_Tmp, Movie_Dir, 512);
        strncat(Str_Tmp, Rom_Name, 507);
        strcat(Str_Tmp, ".gmv");
        SendDlgItemMessage(hDlg, IDC_EDIT_MOVIE_NAME, WM_SETTEXT, 0, (LPARAM)Str_Tmp);

        strncpy(Str_Tmp, Movie_Dir, 512);
        strncat(Str_Tmp, Rom_Name, 507);
        strcat(Str_Tmp, ".gst");
        SendDlgItemMessage(hDlg, IDC_EDIT_MOVIE_STATE, WM_SETTEXT, 0, (LPARAM)Str_Tmp);

        DragAcceptFiles(hDlg, TRUE);

        return true;
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_EDIT_MOVIE_NAME:
            switch (HIWORD(wParam))
            {
            case EN_CHANGE:
            {
                char filename[1024];
                char header[16];
                SendDlgItemMessage(hDlg, IDC_EDIT_MOVIE_NAME, WM_GETTEXT, (WPARAM)512, (LPARAM)filename);

                int gmiRV = LoadSubMovie(filename);

                memcpy(header, SubMovie.Header, 16);
                header[15] = 0;

                {
                    sprintf(Str_Tmp, "%d", SubMovie.LastFrame);
                    SendDlgItemMessage(hDlg, IDC_STATIC_MOVIE_FRAME, WM_SETTEXT, 0, (LPARAM)Str_Tmp);
                    sprintf(Str_Tmp, "%d", SubMovie.NbRerecords);
                    SendDlgItemMessage(hDlg, IDC_STATIC_MOVIE_RERECORDS, WM_SETTEXT, 0, (LPARAM)Str_Tmp);
                    if (SubMovie.Vfreq)
                        sprintf(Str_Tmp, "%d", SubMovie.LastFrame / 50 / 60);
                    else
                        sprintf(Str_Tmp, "%d", SubMovie.LastFrame / 60 / 60);
                    SendDlgItemMessage(hDlg, IDC_STATIC_MINUTES, WM_SETTEXT, 0, (LPARAM)Str_Tmp);

                    if (SubMovie.Vfreq)
                        sprintf(Str_Tmp, "%2.4f", (SubMovie.LastFrame % 3000) / 50.0);
                    else
                        sprintf(Str_Tmp, "%2.4f", (SubMovie.LastFrame % 3600) / 60.0);
                    SendDlgItemMessage(hDlg, IDC_STATIC_SECONDS, WM_SETTEXT, 0, (LPARAM)Str_Tmp);
                    SendDlgItemMessage(hDlg, IDC_STATIC_MOVIE_VERSION, WM_SETTEXT, 0, (LPARAM)SubMovie.Header);
                    SendDlgItemMessage(hDlg, IDC_STATIC_COMMENTS_EDIT, WM_SETTEXT, 0, (LPARAM)SubMovie.Note);
                    switch (SubMovie.PlayerConfig[0])
                    {
                    case 3:
                        strncpy(Str_Tmp, "3 BUTTONS", 1024);
                        break;
                    case 6:
                        strncpy(Str_Tmp, "6 BUTTONS", 1024);
                        break;
                    default:
                        strncpy(Str_Tmp, "UNKNOWN", 1024);
                        break;
                    }
                    SendDlgItemMessage(hDlg, IDC_STATIC_CON1_SET, WM_SETTEXT, 0, (LPARAM)Str_Tmp);
                    switch (SubMovie.PlayerConfig[1])
                    {
                    case 3:
                        strncpy(Str_Tmp, "3 BUTTONS", 1024);
                        break;
                    case 6:
                        strncpy(Str_Tmp, "6 BUTTONS", 1024);
                        break;
                    default:
                        strncpy(Str_Tmp, "UNKNOWN", 1024);
                        break;
                    }
                    SendDlgItemMessage(hDlg, IDC_STATIC_CON2_SET, WM_SETTEXT, 0, (LPARAM)Str_Tmp);
                    Str_Tmp[0] = 0;
                    if (SubMovie.Ok && (((SubMovie.PlayerConfig[0] != 3) && (SubMovie.PlayerConfig[0] != 6)) || ((SubMovie.PlayerConfig[1] != 3) && (SubMovie.PlayerConfig[1] != 6)))) //Upth-Modif - since we now load controller settings from the GM
                    {
                        strcpy(Str_Tmp, "Warning: Controller settings cannot be loaded from movie."); //Upth-Modif - if GMV has unknown controller settings
                        ShowWindow(GetDlgItem(hDlg, IDC_STATIC_WARNING_BITMAP), SW_SHOW);
                    }
                    else if (SubMovie.Ok && ((SubMovie.PlayerConfig[0] != (((Controller_1_Type & 1) + 1 * 3))) || (SubMovie.PlayerConfig[1] != (((Controller_2_Type & 1) + 1 * 3))))) //Upth-Add - if controller settings differ
                    {
                        strcpy(Str_Tmp, "Controller settings will be loaded from movie.");
                    }
                    else
                    {
                        ShowWindow(GetDlgItem(hDlg, IDC_STATIC_WARNING_BITMAP), SW_HIDE);
                    }
                    SendDlgItemMessage(hDlg, IDC_STATIC_CON_WARNING, WM_SETTEXT, 0, (LPARAM)Str_Tmp);
                    if (SubMovie.StateRequired)
                        strcpy(Str_Tmp, "Savestate required !");
                    else
                        strcpy(Str_Tmp, "");
                    SendDlgItemMessage(hDlg, IDC_STATIC_SAVESTATEREQ, WM_SETTEXT, 0, (LPARAM)Str_Tmp);
                    if (SubMovie.TriplePlayerHack)
                        strcpy(Str_Tmp, "3 players movie.  Configure controllers with Teamplay and 3 buttons setting");
                    else
                        strcpy(Str_Tmp, "");
                    SendDlgItemMessage(hDlg, IDC_STATIC_3PLAYERS, WM_SETTEXT, 0, (LPARAM)Str_Tmp);
                    if (!SubMovie.Ok)
                        strcpy(Str_Tmp, "0");
                    else if (SubMovie.Vfreq)
                        strcpy(Str_Tmp, "50");
                    else
                        strcpy(Str_Tmp, "60");
                    SendDlgItemMessage(hDlg, IDC_STATIC_VFRESH, WM_SETTEXT, 0, (LPARAM)Str_Tmp);
                }

                if (SubMovie.Ok != 0 && (SubMovie.UseState == 0 || SubMovie.StateOk != 0) && (SubMovie.StateRequired == 0 || SubMovie.StateOk != 0))
                    EnableWindow(GetDlgItem(hDlg, IDC_OK_PLAY), TRUE);
                else
                {
                    PutSubMovieErrorInStr_Tmp(gmiRV, filename, header);

                    SendDlgItemMessage(hDlg, IDC_STATIC_COMMENTS_EDIT, WM_SETTEXT, 0, (LPARAM)Str_Tmp);

                    EnableWindow(GetDlgItem(hDlg, IDC_OK_PLAY), FALSE);
                }

                if (SubMovie.Ok)
                {
                    if (Def_Read_Only && SubMovie.ReadOnly == 0) SubMovie.ReadOnly = 1; //Upth-Add - for the new "Default Read Only" toggle
                    SendDlgItemMessage(hDlg, IDC_CHECK_READ_ONLY, BM_SETCHECK, (WPARAM)(SubMovie.ReadOnly ? BST_CHECKED : BST_UNCHECKED), 0); //Upth-Add - And we add a check or not depending on whether the movie is read only

                    SendDlgItemMessage(hDlg, IDC_CHECK_CLEAR_SRAM, BM_SETCHECK, (WPARAM)(SubMovie.ClearSRAM ? BST_CHECKED : BST_UNCHECKED), 0);
                }

                if (SubMovie.ReadOnly == 0 && SubMovie.Ok != 0 && SubMovie.UseState != 0 && SubMovie.StateOk != 0)
                    EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_RESUME_RECORD), TRUE);
                else
                    EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_RESUME_RECORD), FALSE);
                break;
            }
            return true;
            }   break;
        case IDC_EDIT_MOVIE_STATE:
            switch (HIWORD(wParam))
            {
            case EN_CHANGE:
                SendDlgItemMessage(hDlg, IDC_EDIT_MOVIE_STATE, WM_GETTEXT, (WPARAM)512, (LPARAM)Str_Tmp);
                GetStateInfo(Str_Tmp, &SubMovie);
                if (SubMovie.StateOk)
                {
                    sprintf(Str_Tmp, "%d", SubMovie.StateFrame);
                    SendDlgItemMessage(hDlg, IDC_STATIC_STATE_FRAME, WM_SETTEXT, 0, (LPARAM)Str_Tmp);
                    if (SubMovie.StateFrame > SubMovie.LastFrame)
                    {
                        strcpy(Str_Tmp, "Warning: The savestate is after the end of the movie");
                        SendDlgItemMessage(hDlg, IDC_STATIC_STATE_WARNING, WM_SETTEXT, 0, (LPARAM)Str_Tmp);
                        SubMovie.StateOk = 0;
                    }
                    else
                    {
                        strcpy(Str_Tmp, "");
                        SendDlgItemMessage(hDlg, IDC_STATIC_STATE_WARNING, WM_SETTEXT, 0, (LPARAM)Str_Tmp);
                    }
                }
                else
                {
                    strcpy(Str_Tmp, "Warning: Invalid save state");
                    SendDlgItemMessage(hDlg, IDC_STATIC_STATE_WARNING, WM_SETTEXT, 0, (LPARAM)Str_Tmp);
                }
                break;
            }
            if (SubMovie.Ok != 0 && (SubMovie.UseState == 0 || SubMovie.StateOk != 0) && (SubMovie.StateRequired == 0 || SubMovie.UseState != 0))
                EnableWindow(GetDlgItem(hDlg, IDC_OK_PLAY), TRUE);
            else
                EnableWindow(GetDlgItem(hDlg, IDC_OK_PLAY), FALSE);
            if (SubMovie.ReadOnly == 0 && SubMovie.Ok != 0 && SubMovie.UseState != 0 && SubMovie.StateOk != 0)
                EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_RESUME_RECORD), TRUE);
            else
                EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_RESUME_RECORD), FALSE);
            SendDlgItemMessage(hDlg, IDC_RADIO_PLAY_STATE, BM_CLICK, 0, 0);
            return true;
            break;
        case IDC_RADIO_PLAY_START:
            SendDlgItemMessage(hDlg, IDC_RADIO_PLAY_START, BM_SETCHECK, (WPARAM)BST_CHECKED, 0);
            SendDlgItemMessage(hDlg, IDC_RADIO_PLAY_STATE, BM_SETCHECK, (WPARAM)BST_UNCHECKED, 0);
            SubMovie.UseState = 0;
            if (SubMovie.Ok != 0 && (SubMovie.UseState == 0 || SubMovie.StateOk != 0) && (SubMovie.StateRequired == 0 || SubMovie.StateOk != 0))
                EnableWindow(GetDlgItem(hDlg, IDC_OK_PLAY), TRUE);
            else
                EnableWindow(GetDlgItem(hDlg, IDC_OK_PLAY), FALSE);
            if (SubMovie.ReadOnly != 0 && SubMovie.Ok != 0 && SubMovie.UseState != 0 && SubMovie.StateOk != 0)
                EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_RESUME_RECORD), TRUE);
            else
                EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_RESUME_RECORD), FALSE);
            EnableWindow(GetDlgItem(hDlg, IDC_CHECK_CLEAR_SRAM), TRUE);
            return true;
            break;
        case IDC_RADIO_PLAY_STATE:
            SendDlgItemMessage(hDlg, IDC_RADIO_PLAY_START, BM_SETCHECK, (WPARAM)BST_UNCHECKED, 0);
            SendDlgItemMessage(hDlg, IDC_RADIO_PLAY_STATE, BM_SETCHECK, (WPARAM)BST_CHECKED, 0);
            SubMovie.UseState = 1;
            if (SubMovie.Ok != 0 && (SubMovie.UseState == 0 || SubMovie.StateOk != 0) && (SubMovie.StateRequired == 0 || SubMovie.StateOk != 0))
                EnableWindow(GetDlgItem(hDlg, IDC_OK_PLAY), TRUE);
            else
                EnableWindow(GetDlgItem(hDlg, IDC_OK_PLAY), FALSE);
            if (SubMovie.ReadOnly == 0 && SubMovie.Ok != 0 && SubMovie.UseState != 0 && SubMovie.StateOk != 0)
                EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_RESUME_RECORD), TRUE);
            else
                EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_RESUME_RECORD), FALSE);
            EnableWindow(GetDlgItem(hDlg, IDC_CHECK_CLEAR_SRAM), FALSE);
            return true;
            break;
        case IDC_BUTTON_BROWSE_MOVIE:
            strcpy(Str_Tmp, "");
            SendDlgItemMessage(hDlg, IDC_EDIT_MOVIE_NAME, WM_GETTEXT, (WPARAM)512, (LPARAM)Str_Tmp);
            DialogsOpen++;
            if (Change_File_L(Str_Tmp, Movie_Dir, "Load Movie", "GENs Movie\0*.gmv*\0All Files\0*.*\0\0", "gmv", hDlg))
            {
                SendDlgItemMessage(hDlg, IDC_EDIT_MOVIE_NAME, WM_SETTEXT, 0, (LPARAM)Str_Tmp);
            }
            DialogsOpen--;
            return true;
            break;
        case IDC_BUTTON_STATE_BROWSE:
            strcpy(Str_Tmp, "");
            SendDlgItemMessage(hDlg, IDC_EDIT_MOVIE_STATE, WM_GETTEXT, (WPARAM)512, (LPARAM)Str_Tmp);
            DialogsOpen++;
            if (Change_File_L(Str_Tmp, Movie_Dir, "Load state", "State Files\0*.gs*\0All Files\0*.*\0\0", "gs0", hDlg))
            {
                SendDlgItemMessage(hDlg, IDC_EDIT_MOVIE_STATE, WM_SETTEXT, 0, (LPARAM)Str_Tmp);
            }
            DialogsOpen--;
            return true;
            break;
        case IDC_CHECK_READ_ONLY:
            if (SubMovie.ReadOnly)
            {
                SubMovie.ReadOnly = 0;
                SendDlgItemMessage(hDlg, IDC_CHECK_READ_ONLY, BM_SETCHECK, (WPARAM)BST_UNCHECKED, 0);
            }
            else
            {
                SubMovie.ReadOnly = 1;
                SendDlgItemMessage(hDlg, IDC_CHECK_READ_ONLY, BM_SETCHECK, (WPARAM)BST_CHECKED, 0);
            }
            if (SubMovie.ReadOnly == 0 && SubMovie.Ok != 0 && SubMovie.UseState != 0 && SubMovie.StateOk != 0)
                EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_RESUME_RECORD), TRUE);
            else
                EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_RESUME_RECORD), FALSE);
            return true;
            break;

        case IDC_CHECK_CLEAR_SRAM:
            SubMovie.ClearSRAM = !SubMovie.ClearSRAM;
            SendDlgItemMessage(hDlg, IDC_CHECK_CLEAR_SRAM, BM_SETCHECK, (WPARAM)(SubMovie.ClearSRAM ? BST_CHECKED : BST_UNCHECKED), 0);
            return true;

        case IDC_OK_PLAY:
            if (Full_Screen)
            {
                while (ShowCursor(true) < 0);
                while (ShowCursor(false) >= 0);
            }
            PlaySubMovie();
            DragAcceptFiles(hDlg, FALSE);
            DialogsOpen--;
            EndDialog(hDlg, true);
            return true;
            break;
        case IDC_BUTTON_RESUME_RECORD:
            if (Full_Screen)
            {
                while (ShowCursor(true) < 0);
                while (ShowCursor(false) >= 0);
            }
            if (SubMovie.Ok)
            {
                if (MainMovie.Status) //Modif N - make sure to close existing movie, if any
                    CloseMovieFile(&MainMovie);
                MainMovie.Status = 0;

                CopyMovie(&SubMovie, &MainMovie);
                MainMovie.Status = MOVIE_RECORDING;
                PlayMovieCanceled = 0;
            }
            DragAcceptFiles(hDlg, FALSE);
            DialogsOpen--;
            EndDialog(hDlg, true);
            return true;
            break;
        case IDC_CANCEL_PLAY:
            if (Full_Screen)
            {
                while (ShowCursor(true) < 0);
                while (ShowCursor(false) >= 0);
            }
            PlayMovieCanceled = 1;
            DragAcceptFiles(hDlg, FALSE);
            DialogsOpen--;
            EndDialog(hDlg, true);
            return true;
            break;
        }
        break;

    case WM_CLOSE:
        if (Full_Screen)
        {
            while (ShowCursor(true) < 0);
            while (ShowCursor(false) >= 0);
        }
        PlayMovieCanceled = 1;
        DragAcceptFiles(hDlg, FALSE);
        DialogsOpen--;
        EndDialog(hDlg, true);
        return true;
        break;

    case WM_DROPFILES:
    {
        HDROP hDrop = (HDROP)wParam;
        DragQueryFile(hDrop, 0, Str_Tmp, 1024);
        DragFinish(hDrop);
        SendDlgItemMessage(hDlg, IDC_EDIT_MOVIE_NAME, WM_SETTEXT, 0, (LPARAM)Str_Tmp);
        return true;
    }   break;
    }

    return false;
}
void DoMovieSplice() //Splices saved input back into the movie file
{
    FILE *TempSplice = fopen(TempName, "r+b");
    if (!TempSplice)
    {
        MessageBox(HWnd, "Error opening temporary file", "ERROR", MB_OK | MB_ICONWARNING);
        return;
    }
    fseek(TempSplice, 0, SEEK_END);
    unsigned long size = ftell(TempSplice);
    //MainMovie.LastFrame++; // removed ++ because it was causing the input to be spliced 1 frame late at the end
    fseek(MainMovie.File, (MainMovie.LastFrame * 3) + 64, SEEK_SET);
    char *TempBuffer = (char *)malloc(size);
    fseek(TempSplice, 0, SEEK_SET);
    fread(TempBuffer, 1, size, TempSplice);
    fwrite(TempBuffer, 1, size, MainMovie.File);
    free(TempBuffer);
    if (MainMovie.Status == MOVIE_RECORDING) Put_Info("Movie successfully spliced. Resuming playback from now.");
    else Put_Info("Movie successfully spliced.");
    MainMovie.Status = MOVIE_PLAYING;
    fseek(MainMovie.File, 0, SEEK_END);
    MainMovie.LastFrame = (ftell(MainMovie.File) - 64) / 3;
    SpliceFrame = 0;
    char cfgFile[1024];
    strcpy(cfgFile, Gens_Path);
    strcat(cfgFile, "Gens.cfg");
    WritePrivateProfileString("Splice", "SpliceMovie", "", cfgFile);
    WritePrivateProfileString("Splice", "SpliceFrame", "0", cfgFile);
    WritePrivateProfileString("Splice", "TempFile", "", cfgFile);
    fclose(TempSplice);
    remove(TempName);
    free(TempName);
}
LRESULT CALLBACK PromptSpliceFrameProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) //saves all the input from specified frame to a tempfile, so a prior section can be redone
{
    RECT r;
    RECT r2;
    int dx1, dy1, dx2, dy2;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        if (Full_Screen)
        {
            while (ShowCursor(false) >= 0);
            while (ShowCursor(true) < 0);
        }

        GetWindowRect(HWnd, &r);
        dx1 = (r.right - r.left) / 2;
        dy1 = (r.bottom - r.top) / 2;

        GetWindowRect(hDlg, &r2);
        dx2 = (r2.right - r2.left) / 2;
        dy2 = (r2.bottom - r2.top) / 2;

        //SetWindowPos(hDlg, NULL, std::max(0, r.left + (dx1 - dx2)), std::max(0, r.top + (dy1 - dy2)), NULL, NULL, SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);
        SetWindowPos(hDlg, NULL, r.left, r.top, NULL, NULL, SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);
        strcpy(Str_Tmp, "Enter the frame you wish to rerecord to.");
        SendDlgItemMessage(hDlg, IDC_PROMPT_TEXT, WM_SETTEXT, 0, (LPARAM)Str_Tmp);
        strcpy(Str_Tmp, "When finished, use Tools->Movie->Input Splice.");
        SendDlgItemMessage(hDlg, IDC_PROMPT_TEXT2, WM_SETTEXT, 0, (LPARAM)Str_Tmp);
        return true;
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
        {
            if (Full_Screen)
            {
                while (ShowCursor(true) < 0);
                while (ShowCursor(false) >= 0);
            }
            TempName = tempnam(Gens_Path, "GMtmp");
            FILE *TempSplice = fopen(TempName, "w+b");
            if (TempSplice == NULL)
            {
                MessageBox(HWnd, "Error creating temporary file!", "Operation canceled", MB_OK | MB_ICONWARNING);
                DialogsOpen--;
                EndDialog(hDlg, true);
                return false;
                break;
            }
            SpliceFrame = GetDlgItemInt(hDlg, IDC_PROMPT_EDIT, NULL, false);
            fseek(MainMovie.File, 0, SEEK_END);
            unsigned long size = ftell(MainMovie.File);
            fseek(MainMovie.File, (((SpliceFrame - 1) * 3) + 64), SEEK_SET);
            size -= ftell(MainMovie.File);
            char *TempBuffer = (char *)malloc(size);
            fread(TempBuffer, 1, size, MainMovie.File);
            fwrite(TempBuffer, 1, size, TempSplice);
            free(TempBuffer);
            sprintf(Str_Tmp, "Rerecording to frame #%d", SpliceFrame);
            {
                strncpy(Str_Tmp, MainMovie.FileName, 512);
                MainMovie.FileName[strlen(MainMovie.FileName) - 3] = '\0';
                strcat(MainMovie.FileName, "spl.gmv");
                BackupMovieFile(&MainMovie);
                strncpy(MainMovie.FileName, Str_Tmp, 512);
            }
            Put_Info(Str_Tmp);
            char cfgFile[1024];
            strcpy(cfgFile, Gens_Path);
            strcat(cfgFile, "Gens.cfg");
            strcpy(SpliceMovie, MainMovie.FileName);
            for (int i = strlen(SpliceMovie); i >= 0; i--) if (SpliceMovie[i] == '|') SpliceMovie[i] = '_';
            WritePrivateProfileString("Splice", "SpliceMovie", SpliceMovie, cfgFile);
            sprintf(Str_Tmp, "%d", SpliceFrame);
            WritePrivateProfileString("Splice", "SpliceFrame", Str_Tmp, cfgFile);
            WritePrivateProfileString("Splice", "TempFile", TempName, cfgFile);
            fclose(TempSplice);
            MustUpdateMenu = 1;
            DialogsOpen--;
            EndDialog(hDlg, true);
            return true;
            break;
        }
        case ID_CANCEL:
        case IDCANCEL:
            if (Full_Screen)
            {
                while (ShowCursor(true) < 0);
                while (ShowCursor(false) >= 0);
            }

            DialogsOpen--;
            EndDialog(hDlg, true);
            return true;
            break;
        }
        break;

    case WM_CLOSE:
        if (Full_Screen)
        {
            while (ShowCursor(true) < 0);
            while (ShowCursor(false) >= 0);
        }
        DialogsOpen--;
        EndDialog(hDlg, true);
        return true;
        break;
    }

    return false;
}
LRESULT CALLBACK PromptSeekFrameProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) //Gets seek frame target, and starts frameseek
{
    RECT r;
    RECT r2;
    int dx1, dy1, dx2, dy2;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        if (Full_Screen)
        {
            while (ShowCursor(false) >= 0);
            while (ShowCursor(true) < 0);
        }

        GetWindowRect(HWnd, &r);
        dx1 = (r.right - r.left) / 2;
        dy1 = (r.bottom - r.top) / 2;

        GetWindowRect(hDlg, &r2);
        dx2 = (r2.right - r2.left) / 2;
        dy2 = (r2.bottom - r2.top) / 2;

        //SetWindowPos(hDlg, NULL, std::max(0, r.left + (dx1 - dx2)), std::max(0, r.top + (dy1 - dy2)), NULL, NULL, SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);
        SetWindowPos(hDlg, NULL, r.left, r.top, NULL, NULL, SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);
        strcpy(Str_Tmp, "Enter the frame you wish to seek to.");
        SendDlgItemMessage(hDlg, IDC_PROMPT_TEXT, WM_SETTEXT, 0, (LPARAM)Str_Tmp);
        strcpy(Str_Tmp, "");
        SendDlgItemMessage(hDlg, IDC_PROMPT_TEXT2, WM_SETTEXT, 0, (LPARAM)Str_Tmp);
        return true;
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
        {
            if (Full_Screen)
            {
                while (ShowCursor(true) < 0);
                while (ShowCursor(false) >= 0);
            }
            SeekFrame = GetDlgItemInt(hDlg, IDC_PROMPT_EDIT, NULL, false);
            sprintf(Str_Tmp, "Seeking to frame %d", SeekFrame);
            Put_Info(Str_Tmp);
            MustUpdateMenu = 1;
            DialogsOpen--;
            EndDialog(hDlg, true);
            return true;
            break;
        }
        case ID_CANCEL:
        case IDCANCEL:
            if (Full_Screen)
            {
                while (ShowCursor(true) < 0);
                while (ShowCursor(false) >= 0);
            }

            DialogsOpen--;
            EndDialog(hDlg, true);
            return true;
            break;
        }
        break;

    case WM_CLOSE:
        if (Full_Screen)
        {
            while (ShowCursor(true) < 0);
            while (ShowCursor(false) >= 0);
        }
        DialogsOpen--;
        EndDialog(hDlg, true);
        return true;
        break;
    }

    return false;
}

LRESULT CALLBACK RecordMovieProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    RECT r;
    RECT r2;
    int dx1, dy1, dx2, dy2;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        if (Full_Screen)
        {
            while (ShowCursor(false) >= 0);
            while (ShowCursor(true) < 0);
        }

        GetWindowRect(HWnd, &r);
        dx1 = (r.right - r.left) / 2;
        dy1 = (r.bottom - r.top) / 2;

        GetWindowRect(hDlg, &r2);
        dx2 = (r2.right - r2.left) / 2;
        dy2 = (r2.bottom - r2.top) / 2;

        //SetWindowPos(hDlg, NULL, std::max(0, r.left + (dx1 - dx2)), std::max(0, r.top + (dy1 - dy2)), NULL, NULL, SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);
        SetWindowPos(hDlg, NULL, r.left, r.top, NULL, NULL, SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);

        if ((Controller_1_Type & 1) == 1)
            strcpy(Str_Tmp, "6 BUTTONS");
        else
            strcpy(Str_Tmp, "3 BUTTONS");
        SendDlgItemMessage(hDlg, IDC_STATIC_CON1_CURSET, WM_SETTEXT, 0, (LPARAM)Str_Tmp);

        if ((Controller_2_Type & 1) == 1)
            strcpy(Str_Tmp, "6 BUTTONS");
        else
            strcpy(Str_Tmp, "3 BUTTONS");
        SendDlgItemMessage(hDlg, IDC_STATIC_CON2_CURSET, WM_SETTEXT, 0, (LPARAM)Str_Tmp);
        strncpy(Str_Tmp, Movie_Dir, 512);
        strncat(Str_Tmp, Rom_Name, 507);
        strcat(Str_Tmp, ".gmv");
        SendDlgItemMessage(hDlg, IDC_EDIT_MOVIE_NAME, WM_SETTEXT, 0, (LPARAM)Str_Tmp);
        SendDlgItemMessage(hDlg, IDC_CHECK_3PLAYER, BM_SETCHECK, BST_UNCHECKED, 0);
        SendDlgItemMessage(hDlg, IDC_CHECK_SAVESTATEREQ, BM_SETCHECK, BST_UNCHECKED, 0);
        InitMovie(&SubMovie);
        RecordMovieCanceled = 1;
        return true;
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_EDIT_MOVIE_NAME:
            switch (HIWORD(wParam))
            {
            case EN_CHANGE:
                SendDlgItemMessage(hDlg, IDC_EDIT_MOVIE_NAME, WM_GETTEXT, (WPARAM)512, (LPARAM)Str_Tmp);
                if (Str_Tmp[0])
                    EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_RESUME_RECORD), TRUE);
                else
                    EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_RESUME_RECORD), FALSE);
                GetMovieInfo(Str_Tmp, &SubMovie);
                Str_Tmp[0] = 0;
                if (SubMovie.Ok)
                    strcpy(Str_Tmp, "Warning: File already exists");
                SendDlgItemMessage(hDlg, IDC_STATIC_WARNING_EXIST, WM_SETTEXT, 0, (LPARAM)Str_Tmp);
                break;
            }
            return true;
            break;
        case IDC_BUTTON_BROWSE_MOVIE:
            strncpy(Str_Tmp, Rom_Name, 512);
            //strcat(Str_Tmp,".gmv");
            if (Change_File_S(Str_Tmp, Movie_Dir, "Save Movie", "GENs Movie\0*.gmv\0All Files\0*.*\0\0", "gmv", hDlg))
            {
                SendDlgItemMessage(hDlg, IDC_EDIT_MOVIE_NAME, WM_SETTEXT, 0, (LPARAM)Str_Tmp);
            }
            return true;
            break;
        case IDC_OK_RECORD:
            if (Full_Screen)
            {
                while (ShowCursor(true) < 0);
                while (ShowCursor(false) >= 0);
            }

            if (MainMovie.File != NULL) //Modif N - make sure to close existing movie, if any
                CloseMovieFile(&MainMovie);
            MainMovie.Status = 0;

            InitMovie(&MainMovie);
            MainMovie.ReadOnly = 0;
            MainMovie.Type = TYPEGMV;
            MainMovie.Version = 'A' - '0';
            SendDlgItemMessage(hDlg, IDC_EDIT_MOVIE_NAME, WM_GETTEXT, (WPARAM)512, (LPARAM)Str_Tmp);
            strncpy(MainMovie.FileName, Str_Tmp, 512);
            SendDlgItemMessage(hDlg, IDC_EDIT_NOTE, WM_GETTEXT, (WPARAM)512, (LPARAM)Str_Tmp);
            strncpy(MainMovie.Note, Str_Tmp, 40);
            MainMovie.Note[40] = 0;
            strncpy(MainMovie.Header, "Gens Movie TESTA", 17);
            MainMovie.Status = MOVIE_RECORDING;
            RecordMovieCanceled = 0;
            if ((Controller_1_Type & 1) == 1)
                MainMovie.PlayerConfig[0] = 6;
            else
                MainMovie.PlayerConfig[0] = 3;
            if ((Controller_2_Type & 1) == 1)
                MainMovie.PlayerConfig[1] = 6;
            else
                MainMovie.PlayerConfig[1] = 3;
            if (SendDlgItemMessage(hDlg, IDC_CHECK_3PLAYER, BM_GETCHECK, BST_UNCHECKED, 0) == BST_CHECKED)
            {
                MainMovie.TriplePlayerHack = 1;
            }
            if (SendDlgItemMessage(hDlg, IDC_CHECK_SAVESTATEREQ, BM_GETCHECK, BST_UNCHECKED, 0) == BST_CHECKED)
            {
                MainMovie.StateRequired = 1;
            }
            DialogsOpen--;
            EndDialog(hDlg, true);
            return true;
            break;
        case IDC_CANCEL_RECORD:
            if (Full_Screen)
            {
                while (ShowCursor(true) < 0);
                while (ShowCursor(false) >= 0);
            }
            RecordMovieCanceled = 1;
            DialogsOpen--;
            EndDialog(hDlg, true);
            return true;
            break;
        }
        break;

    case WM_CLOSE:
        if (Full_Screen)
        {
            while (ShowCursor(true) < 0);
            while (ShowCursor(false) >= 0);
        }
        RecordMovieCanceled = 1;
        DialogsOpen--;
        EndDialog(hDlg, true);
        return true;
        break;
    }

    return false;
}

void init_list_box(HWND Box, const char* Strs[], int numColumns, int *columnWidths) //initializes the ram search and/or ram watch listbox
{
    LVCOLUMN Col;
    Col.mask = LVCF_FMT | LVCF_ORDER | LVCF_SUBITEM | LVCF_TEXT | LVCF_WIDTH;
    Col.fmt = LVCFMT_CENTER;
    for (int i = 0; i < numColumns; i++)
    {
        Col.iOrder = i;
        Col.iSubItem = i;
        Col.pszText = (LPSTR)(Strs[i]);
        Col.cx = columnWidths[i];
        ListView_InsertColumn(Box, i, &Col);
    }

    ListView_SetExtendedListViewStyle(Box, LVS_EX_FULLROWSELECT);
}

template <typename T>
T CheatRead(unsigned int address)
{
    T val = 0;
    for (int i = 0; i < sizeof(T); i++)
        val <<= 8, val |= (T)(M68K_RB(address + i));
    return val;
}
template <typename T>
void CheatWrite(unsigned int address, T value)
{
    for (int i = sizeof(T) - 1; i >= 0; i--)
    {
        M68K_WBC(address++, (unsigned char)((value >> (i << 3)) & 0xff));
    }
}

LRESULT CALLBACK AboutProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    RECT r;
    RECT r2;
    int dx1, dy1, dx2, dy2;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        if (Full_Screen)
        {
            while (ShowCursor(false) >= 0);
            while (ShowCursor(true) < 0);
        }

        GetWindowRect(HWnd, &r);
        dx1 = (r.right - r.left) / 2;
        dy1 = (r.bottom - r.top) / 2;

        GetWindowRect(hDlg, &r2);
        dx2 = (r2.right - r2.left) / 2;
        dy2 = (r2.bottom - r2.top) / 2;

        SetWindowPos(hDlg, NULL, std::max(0L, r.left + (dx1 - dx2)), std::max(0L, r.top + (dy1 - dy2)), NULL, NULL, SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);

        SetDlgItemText(hDlg, IDC_EDIT1,
            "Original version (c) 1999/2002 by St�phane Dallongeville" "\r\n" "\r\n"
            "More about this mod at:" "\r\n"
            "https://github.com/lab313ru/smd_ida_tools2"
        );

        return true;
        break;

    case WM_COMMAND:
        switch (wParam)
        {
        case IDOK:
            if (Full_Screen)
            {
                while (ShowCursor(true) < 0);
                while (ShowCursor(false) >= 0);
            }

            DialogsOpen--;
            EndDialog(hDlg, true);
            return true;
        }
        break;

    case WM_CLOSE:
        if (Full_Screen)
        {
            while (ShowCursor(true) < 0);
            while (ShowCursor(false) >= 0);
        }

        DialogsOpen--;
        EndDialog(hDlg, true);
        return true;
        break;
    }

    return false;
}

LRESULT CALLBACK ColorProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    RECT r;
    RECT r2;
    int dx1, dy1, dx2, dy2;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        if (Full_Screen)
        {
            while (ShowCursor(false) >= 0);
            while (ShowCursor(true) < 0);
        }

        GetWindowRect(HWnd, &r);
        dx1 = (r.right - r.left) / 2;
        dy1 = (r.bottom - r.top) / 2;

        GetWindowRect(hDlg, &r2);
        dx2 = (r2.right - r2.left) / 2;
        dy2 = (r2.bottom - r2.top) / 2;

        SetWindowPos(hDlg, NULL, std::max(0L, r.left + (dx1 - dx2)), std::max(0L, r.top + (dy1 - dy2)), NULL, NULL, SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);

        WORD_L(IDC_STATIC_CONT, "Contrast");
        WORD_L(IDC_STATIC_BRIGHT, "Brightness");
        WORD_L(IDC_CHECK_GREYSCALE, "Greyscale");
        WORD_L(IDC_CHECK_INVERT, "Invert");

        WORD_L(ID_APPLY, "&Apply");
        WORD_L(ID_CLOSE, "&Close");
        WORD_L(ID_DEFAULT, "&Default");

        SendDlgItemMessage(hDlg, IDC_SLIDER_CONTRASTE, TBM_SETRANGE, (WPARAM)(BOOL)TRUE, (LPARAM)MAKELONG(0, 200));
        SendDlgItemMessage(hDlg, IDC_SLIDER_CONTRASTE, TBM_SETPOS, (WPARAM)(BOOL)TRUE, (LPARAM)(LONG)(Contrast_Level));
        SendDlgItemMessage(hDlg, IDC_SLIDER_LUMINOSITE, TBM_SETRANGE, (WPARAM)(BOOL)TRUE, (LPARAM)MAKELONG(0, 200));
        SendDlgItemMessage(hDlg, IDC_SLIDER_LUMINOSITE, TBM_SETPOS, (WPARAM)(BOOL)TRUE, (LPARAM)(LONG)(Brightness_Level));

        SendDlgItemMessage(hDlg, IDC_CHECK_GREYSCALE, BM_SETCHECK, (WPARAM)(Greyscale) ? BST_CHECKED : BST_UNCHECKED, 0);
        SendDlgItemMessage(hDlg, IDC_CHECK_INVERT, BM_SETCHECK, (WPARAM)(Invert_Color) ? BST_CHECKED : BST_UNCHECKED, 0);

        return true;
        break;

    case WM_COMMAND:
        switch (wParam)
        {
        case ID_CLOSE:
            if (Full_Screen)
            {
                while (ShowCursor(true) < 0);
                while (ShowCursor(false) >= 0);
            }

            Build_Main_Menu();
            DialogsOpen--;
            EndDialog(hDlg, true);
            return true;

        case ID_APPLY:
            Contrast_Level = SendDlgItemMessage(hDlg, IDC_SLIDER_CONTRASTE, TBM_GETPOS, 0, 0);
            Brightness_Level = SendDlgItemMessage(hDlg, IDC_SLIDER_LUMINOSITE, TBM_GETPOS, 0, 0);
            Greyscale = (SendDlgItemMessage(hDlg, IDC_CHECK_GREYSCALE, BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0;
            Invert_Color = (SendDlgItemMessage(hDlg, IDC_CHECK_INVERT, BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0;

            Recalculate_Palettes();
            if (Game)
            {
                CRam_Flag = 1;
                Show_Genesis_Screen(HWnd);
            }
            return true;

        case ID_DEFAULT:
            SendDlgItemMessage(hDlg, IDC_SLIDER_CONTRASTE, TBM_SETPOS, (WPARAM)(BOOL)TRUE, (LPARAM)(LONG)(100));
            SendDlgItemMessage(hDlg, IDC_SLIDER_LUMINOSITE, TBM_SETPOS, (WPARAM)(BOOL)TRUE, (LPARAM)(LONG)(100));
            SendDlgItemMessage(hDlg, IDC_CHECK_GREYSCALE, BM_SETCHECK, BST_UNCHECKED, 0);
            SendDlgItemMessage(hDlg, IDC_CHECK_INVERT, BM_SETCHECK, BST_UNCHECKED, 0);
            return true;
        }
        break;

    case WM_CLOSE:
        if (Full_Screen)
        {
            while (ShowCursor(true) < 0);
            while (ShowCursor(false) >= 0);
        }

        DialogsOpen--;
        EndDialog(hDlg, true);
        return true;
        break;
    }

    return false;
}

BOOL SetDlgItemFloat(HWND hDlg, int item, float value)
{
    char buf[11];
    sprintf(buf, "%.8f", value);
    return SetDlgItemText(hDlg, item, buf);
}

float GetDlgItemFloat(HWND hDlg, int item)
{
    char buf[11];
    float ret = 0;

    GetDlgItemText(hDlg, item, buf, 11);
    sscanf(buf, "%f", &ret);
    return(ret);
}

LRESULT CALLBACK OptionProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    RECT r;
    RECT r2;
    int dx1, dy1, dx2, dy2;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        if (Full_Screen)
        {
            while (ShowCursor(false) >= 0);
            while (ShowCursor(true) < 0);
        }

        GetWindowRect(HWnd, &r);
        dx1 = (r.right - r.left) / 2;
        dy1 = (r.bottom - r.top) / 2;

        GetWindowRect(hDlg, &r2);
        dx2 = (r2.right - r2.left) / 2;
        dy2 = (r2.bottom - r2.top) / 2;

        SetWindowPos(hDlg, NULL, std::max(0L, r.left + (dx1 - dx2)), std::max(0L, r.top + (dy1 - dy2)), NULL, NULL, SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);

        WORD_L(IDC_AUTOFIXCHECKSUM, "Auto Fix Checksum");
        WORD_L(IDC_AUTOPAUSE, "Auto Pause");
        WORD_L(IDC_FASTBLUR, "Fast Blur");
        WORD_L(IDC_ENABLE_FPS, "Enable");
        WORD_L(IDC_ENABLE_MESSAGE, "Enable");
        WORD_L(IDC_X2_FPS, "Double Sized");
        WORD_L(IDC_X2_MESSAGE, "Double Sized");
        WORD_L(IDC_TRANS_FPS, "Transparency");
        WORD_L(IDC_TRANS_MESSAGE, "Transparency");
        WORD_L(IDC_EFFECT_COLOR, "Intro effect color");
        WORD_L(IDC_OPTION_SYSTEM, "System");
        WORD_L(IDC_OPTION_FPS, "FPS");
        WORD_L(IDC_OPTION_MESSAGE, "Message");
        WORD_L(IDC_OPTION_CANCEL, "&Cancel");
        WORD_L(IDC_OPTION_OK, "&OK");

        SendDlgItemMessage(hDlg, IDC_COLOR_FPS, TBM_SETRANGE, (WPARAM)(BOOL)TRUE, (LPARAM)MAKELONG(0, 3));
        SendDlgItemMessage(hDlg, IDC_COLOR_FPS, TBM_SETPOS, (WPARAM)(BOOL)TRUE, (LPARAM)(LONG)((FPS_Style & 0x6) >> 1));
        SendDlgItemMessage(hDlg, IDC_COLOR_MESSAGE, TBM_SETRANGE, (WPARAM)(BOOL)TRUE, (LPARAM)MAKELONG(0, 3));
        SendDlgItemMessage(hDlg, IDC_COLOR_MESSAGE, TBM_SETPOS, (WPARAM)(BOOL)TRUE, (LPARAM)(LONG)((Message_Style & 0x6) >> 1));
        SendDlgItemMessage(hDlg, IDC_COLOR_EFFECT, TBM_SETRANGE, (WPARAM)(BOOL)TRUE, (LPARAM)MAKELONG(0, 7));
        SendDlgItemMessage(hDlg, IDC_COLOR_EFFECT, TBM_SETPOS, (WPARAM)(BOOL)TRUE, (LPARAM)(LONG)Effect_Color);

        SendDlgItemMessage(hDlg, IDC_AUTOFIXCHECKSUM, BM_SETCHECK, (WPARAM)(Auto_Fix_CS) ? BST_CHECKED : BST_UNCHECKED, 0);
        SendDlgItemMessage(hDlg, IDC_AUTOPAUSE, BM_SETCHECK, (WPARAM)(Auto_Pause) ? BST_CHECKED : BST_UNCHECKED, 0);
        SendDlgItemMessage(hDlg, IDC_FASTBLUR, BM_SETCHECK, (WPARAM)(Fast_Blur) ? BST_CHECKED : BST_UNCHECKED, 0);
        SendDlgItemMessage(hDlg, IDC_ENABLE_FPS, BM_SETCHECK, (WPARAM)(Show_FPS) ? BST_CHECKED : BST_UNCHECKED, 0);
        SendDlgItemMessage(hDlg, IDC_X2_FPS, BM_SETCHECK, (WPARAM)(FPS_Style & 0x10) ? BST_CHECKED : BST_UNCHECKED, 0);
        SendDlgItemMessage(hDlg, IDC_TRANS_FPS, BM_SETCHECK, (WPARAM)(FPS_Style & 0x8) ? BST_CHECKED : BST_UNCHECKED, 0);
        SendDlgItemMessage(hDlg, IDC_ENABLE_MESSAGE, BM_SETCHECK, (WPARAM)(Show_Message) ? BST_CHECKED : BST_UNCHECKED, 0);
        SendDlgItemMessage(hDlg, IDC_X2_MESSAGE, BM_SETCHECK, (WPARAM)(Message_Style & 0x10) ? BST_CHECKED : BST_UNCHECKED, 0);
        SendDlgItemMessage(hDlg, IDC_TRANS_MESSAGE, BM_SETCHECK, (WPARAM)(Message_Style & 0x8) ? BST_CHECKED : BST_UNCHECKED, 0);
        SendDlgItemMessage(hDlg, IDC_DISABLE_BLUE_SCREEN, BM_SETCHECK, (WPARAM)(Disable_Blue_Screen) ? BST_CHECKED : BST_UNCHECKED, 0); //Modif
        SendDlgItemMessage(hDlg, IDC_DISABLE_INTRO_EFFECT, BM_SETCHECK, (WPARAM)(Intro_Style == 0) ? BST_CHECKED : BST_UNCHECKED, 0); //Modif N
        SendDlgItemMessage(hDlg, IDC_FRAMECOUNTER, BM_SETCHECK, (WPARAM)(FrameCounterEnabled) ? BST_CHECKED : BST_UNCHECKED, 0); //Modif
        SendDlgItemMessage(hDlg, IDC_FRAMECOUNTERFRAMES, BM_SETCHECK, (WPARAM)(FrameCounterFrames) ? BST_CHECKED : BST_UNCHECKED, 0); //Modif N.
        SendDlgItemMessage(hDlg, IDC_LAGCOUNTER, BM_SETCHECK, (WPARAM)(LagCounterEnabled) ? BST_CHECKED : BST_UNCHECKED, 0); //Modif
        SendDlgItemMessage(hDlg, IDC_LAGCOUNTERFRAMES, BM_SETCHECK, (WPARAM)(LagCounterFrames) ? BST_CHECKED : BST_UNCHECKED, 0); //Modif N.
        SendDlgItemMessage(hDlg, IDC_CHECK_SHOWINPUT, BM_SETCHECK, (WPARAM)(ShowInputEnabled) ? BST_CHECKED : BST_UNCHECKED, 0); //Modif
        SendDlgItemMessage(hDlg, IDC_CHECK_BACKUP, BM_SETCHECK, (WPARAM)(AutoBackupEnabled) ? BST_CHECKED : BST_UNCHECKED, 0); //Modif
        SendDlgItemMessage(hDlg, IDC_CHECK_DEF_READ_ONLY, BM_SETCHECK, (WPARAM)(Def_Read_Only) ? BST_CHECKED : BST_UNCHECKED, 0); //Upth-Add
        SendDlgItemMessage(hDlg, IDC_CHECK_AUTO_CLOSE, BM_SETCHECK, (WPARAM)(AutoCloseMovie) ? BST_CHECKED : BST_UNCHECKED, 0); //Upth-Add
        SendDlgItemMessage(hDlg, IDC_CHECK_USEMOVIESTATES, BM_SETCHECK, (WPARAM)(UseMovieStates) ? BST_CHECKED : BST_UNCHECKED, 0); //Upth-Add

        SendDlgItemMessage(hDlg, IDC_TOP_LEFT, BM_SETCHECK, (WPARAM)(FrameCounterPosition == FRAME_COUNTER_TOP_LEFT) ? BST_CHECKED : BST_UNCHECKED, 0); //Modif
        SendDlgItemMessage(hDlg, IDC_TOP_RIGHT, BM_SETCHECK, (WPARAM)(FrameCounterPosition == FRAME_COUNTER_TOP_RIGHT) ? BST_CHECKED : BST_UNCHECKED, 0); //Modif
        SendDlgItemMessage(hDlg, IDC_BOTTOM_LEFT, BM_SETCHECK, (WPARAM)(FrameCounterPosition == FRAME_COUNTER_BOTTOM_LEFT) ? BST_CHECKED : BST_UNCHECKED, 0); //Modif
        SendDlgItemMessage(hDlg, IDC_BOTTOM_RIGHT, BM_SETCHECK, (WPARAM)(FrameCounterPosition == FRAME_COUNTER_BOTTOM_RIGHT) ? BST_CHECKED : BST_UNCHECKED, 0); //Modif
        SetDlgItemInt(hDlg, IDC_TEXT_RES_X, Res_X, true); //Upth-Add - These will show the currently configured
        SetDlgItemInt(hDlg, IDC_TEXT_RES_Y, Res_Y, true); //Upth-Add - fullscreen resolution value
        SetDlgItemFloat(hDlg, IDC_TEXT_SCALE, ScaleFactor);
        SetDlgItemInt(hDlg, IDC_TEXT_DELAY, DelayFactor, false); //Upth-Add - Frame Advance delay mod

        return true;
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_OPTION_OK:
        {
            unsigned int res;
            float ScaleFactorOld = ScaleFactor;

            if (Full_Screen)
            {
                while (ShowCursor(true) < 0);
                while (ShowCursor(false) >= 0);
            }

            res = SendDlgItemMessage(hDlg, IDC_COLOR_FPS, TBM_GETPOS, 0, 0);
            FPS_Style = (FPS_Style & ~0x6) | ((res << 1) & 0x6);
            res = SendDlgItemMessage(hDlg, IDC_COLOR_MESSAGE, TBM_GETPOS, 0, 0);
            Message_Style = (Message_Style & 0xF9) | ((res << 1) & 0x6);
            Effect_Color = SendDlgItemMessage(hDlg, IDC_COLOR_EFFECT, TBM_GETPOS, 0, 0);
            Intro_Style = (SendDlgItemMessage(hDlg, IDC_DISABLE_INTRO_EFFECT, BM_GETCHECK, 0, 0) == BST_CHECKED) ? 0 : (Intro_Style ? Intro_Style : 2); //Modif N.

            if (Effect_Color == 0) //Modif N. - black color disables intro
                Intro_Style = 0;

            Auto_Fix_CS = (SendDlgItemMessage(hDlg, IDC_AUTOFIXCHECKSUM, BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0;
            Auto_Pause = (SendDlgItemMessage(hDlg, IDC_AUTOPAUSE, BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0;
            Fast_Blur = (SendDlgItemMessage(hDlg, IDC_FASTBLUR, BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0;
            Show_FPS = (SendDlgItemMessage(hDlg, IDC_ENABLE_FPS, BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0;
            res = SendDlgItemMessage(hDlg, IDC_X2_FPS, BM_GETCHECK, 0, 0);
            FPS_Style = (FPS_Style & ~0x10) | ((res == BST_CHECKED) ? 0x10 : 0);
            res = SendDlgItemMessage(hDlg, IDC_TRANS_FPS, BM_GETCHECK, 0, 0);
            FPS_Style = (FPS_Style & ~0x8) | ((res == BST_CHECKED) ? 0x8 : 0);
            Show_Message = (SendDlgItemMessage(hDlg, IDC_ENABLE_MESSAGE, BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0;
            res = SendDlgItemMessage(hDlg, IDC_X2_MESSAGE, BM_GETCHECK, 0, 0);
            Message_Style = (Message_Style & ~0x10) | ((res == BST_CHECKED) ? 0x10 : 0);
            res = SendDlgItemMessage(hDlg, IDC_TRANS_MESSAGE, BM_GETCHECK, 0, 0);
            Message_Style = (Message_Style & ~0x8) | ((res == BST_CHECKED) ? 0x8 : 0);
            Disable_Blue_Screen = (SendDlgItemMessage(hDlg, IDC_DISABLE_BLUE_SCREEN, BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0;//Modif
            FrameCounterEnabled = (SendDlgItemMessage(hDlg, IDC_FRAMECOUNTER, BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0;//Modif
            FrameCounterFrames = (SendDlgItemMessage(hDlg, IDC_FRAMECOUNTERFRAMES, BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0;//Modif N.
            LagCounterEnabled = (SendDlgItemMessage(hDlg, IDC_LAGCOUNTER, BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0;//Modif
            LagCounterFrames = (SendDlgItemMessage(hDlg, IDC_LAGCOUNTERFRAMES, BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0;//Modif N.
            ShowInputEnabled = (SendDlgItemMessage(hDlg, IDC_CHECK_SHOWINPUT, BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0;//Modif
            AutoBackupEnabled = (SendDlgItemMessage(hDlg, IDC_CHECK_BACKUP, BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0;//Modif
            if (SendDlgItemMessage(hDlg, IDC_TOP_LEFT, BM_GETCHECK, 0, 0) == BST_CHECKED)
                FrameCounterPosition = FRAME_COUNTER_TOP_LEFT;
            else
                if (SendDlgItemMessage(hDlg, IDC_TOP_RIGHT, BM_GETCHECK, 0, 0) == BST_CHECKED)
                    FrameCounterPosition = FRAME_COUNTER_TOP_RIGHT;
                else
                    if (SendDlgItemMessage(hDlg, IDC_BOTTOM_LEFT, BM_GETCHECK, 0, 0) == BST_CHECKED)
                        FrameCounterPosition = FRAME_COUNTER_BOTTOM_LEFT;
                    else
                        FrameCounterPosition = FRAME_COUNTER_BOTTOM_RIGHT;
            Res_X = GetDlgItemInt(hDlg, IDC_TEXT_RES_X, NULL, true); //Upth-Add - This reconfigures
            Res_Y = GetDlgItemInt(hDlg, IDC_TEXT_RES_Y, NULL, true); //Upth-Add - the fullscreen resolution
            ScaleFactor = GetDlgItemFloat(hDlg, IDC_TEXT_SCALE);

            Def_Read_Only = (SendDlgItemMessage(hDlg, IDC_CHECK_DEF_READ_ONLY, BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0; //Upth-Add //Modif N. - it shouldn't save if the user hits Cancel instead of OK
            AutoCloseMovie = (SendDlgItemMessage(hDlg, IDC_CHECK_AUTO_CLOSE, BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0; //Upth-Add //Modif N. - it shouldn't save if the user hits Cancel instead of OK
            UseMovieStates = (SendDlgItemMessage(hDlg, IDC_CHECK_USEMOVIESTATES, BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0; //Upth-Add
            unsigned int TempDelay = GetDlgItemInt(hDlg, IDC_TEXT_DELAY, NULL, false);
            DelayFactor = (TempDelay) ? TempDelay : 1;

            Build_Main_Menu();
            if (ScaleFactorOld != ScaleFactor)
                Set_Window_Size(HWnd);

            DialogsOpen--;
            EndDialog(hDlg, true);
            return true;
        }   break;

        case IDC_OPTION_CANCEL:
        case IDCANCEL:
            if (Full_Screen)
            {
                while (ShowCursor(true) < 0);
                while (ShowCursor(false) >= 0);
            }
            DialogsOpen--;
            EndDialog(hDlg, true);
            return true;
        }
        break;

    case WM_CLOSE:
        if (Full_Screen)
        {
            while (ShowCursor(true) < 0);
            while (ShowCursor(false) >= 0);
        }

        DialogsOpen--;
        EndDialog(hDlg, true);
        return true;
        break;
    }

    return false;
}

void RefreshEnabledPorts(HWND hDlg)
{
    int curController_1_Type = (SendDlgItemMessage(hDlg, IDC_COMBO_PORT1, CB_GETCURSEL, (WPARAM)0, (LPARAM)0) & 1);
    EnableWindow(GetDlgItem(hDlg, IDC_COMBO_PADP1B), curController_1_Type);
    EnableWindow(GetDlgItem(hDlg, IDC_COMBO_PADP1C), curController_1_Type);
    EnableWindow(GetDlgItem(hDlg, IDC_COMBO_PADP1D), curController_1_Type);
    EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_SETKEYSP1B), curController_1_Type);
    EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_SETKEYSP1C), curController_1_Type);
    EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_SETKEYSP1D), curController_1_Type);

    int curController_2_Type = (SendDlgItemMessage(hDlg, IDC_COMBO_PORT2, CB_GETCURSEL, (WPARAM)0, (LPARAM)0) & 1);
    EnableWindow(GetDlgItem(hDlg, IDC_COMBO_PADP2B), curController_2_Type);
    EnableWindow(GetDlgItem(hDlg, IDC_COMBO_PADP2C), curController_2_Type);
    EnableWindow(GetDlgItem(hDlg, IDC_COMBO_PADP2D), curController_2_Type);
    EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_SETKEYSP2B), curController_2_Type);
    EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_SETKEYSP2C), curController_2_Type);
    EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_SETKEYSP2D), curController_2_Type);
}

LRESULT CALLBACK ControllerProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    RECT r;
    RECT r2;
    int dx1, dy1, dx2, dy2;
    int i;
    static HWND Tex0 = NULL;

    switch (uMsg)
    {
    case WM_INITDIALOG: {
        if (Full_Screen)
        {
            while (ShowCursor(false) >= 0);
            while (ShowCursor(true) < 0);
        }

        GetWindowRect(HWnd, &r);
        dx1 = (r.right - r.left) / 2;
        dy1 = (r.bottom - r.top) / 2;

        GetWindowRect(hDlg, &r2);
        dx2 = (r2.right - r2.left) / 2;
        dy2 = (r2.bottom - r2.top) / 2;

        SetWindowPos(hDlg, NULL, std::max(0L, r.left + (dx1 - dx2)), std::max(0L, r.top + (dy1 - dy2)), NULL, NULL, SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);

        Tex0 = GetDlgItem(hDlg, IDC_STATIC_TEXT0);

        if (!Init_Input(ghInstance, hDlg)) return false;

        WORD_L(IDC_JOYINFO1, "Player 1-B 1-C and 1-D are enabled only if a teamplayer is connected to port 1");
        WORD_L(IDC_JOYINFO2, "Player 2-B 2-C and 2-D are enabled only if a teamplayer is connected to port 2");
        WORD_L(IDC_JOYINFO3, "Only a few games support teamplayer (games which have 4 players support), so don't forget to use the \"load config\" and \"save config\" possibility :)");

        for (i = 0; i < 2; i++)
        {
            SendDlgItemMessage(hDlg, IDC_COMBO_PORT1 + i, CB_INSERTSTRING, (WPARAM)0, (LONG)(LPTSTR) "teamplayer");
            SendDlgItemMessage(hDlg, IDC_COMBO_PORT1 + i, CB_INSERTSTRING, (WPARAM)0, (LONG)(LPTSTR) "pad");
        }

        for (i = 0; i < 8; i++)
        {
            SendDlgItemMessage(hDlg, IDC_COMBO_PADP1 + i, CB_INSERTSTRING, (WPARAM)0, (LONG)(LPTSTR) "6 buttons");
            SendDlgItemMessage(hDlg, IDC_COMBO_PADP1 + i, CB_INSERTSTRING, (WPARAM)0, (LONG)(LPTSTR) "3 buttons");
        }
        //SendDlgItemMessage(hDlg, IDC_COMBO_NUMLOAD, CB_INSERTSTRING, (WPARAM) 0, (LONG) (LPTSTR) "       # Load,  Shift # save,    Ctrl # select");
        //SendDlgItemMessage(hDlg, IDC_COMBO_NUMLOAD, CB_INSERTSTRING, (WPARAM) 0, (LONG) (LPTSTR) "       # Load,    Ctrl # save, Shift # select");
        //SendDlgItemMessage(hDlg, IDC_COMBO_NUMLOAD, CB_INSERTSTRING, (WPARAM) 0, (LONG) (LPTSTR) "Shift # Load,         # save,    Ctrl # select");
        //SendDlgItemMessage(hDlg, IDC_COMBO_NUMLOAD, CB_INSERTSTRING, (WPARAM) 0, (LONG) (LPTSTR) "Shift # Load,    Ctrl # save,        # select");
        //SendDlgItemMessage(hDlg, IDC_COMBO_NUMLOAD, CB_INSERTSTRING, (WPARAM) 0, (LONG) (LPTSTR) "   Ctrl # Load,        # save, Shift # select");
        //SendDlgItemMessage(hDlg, IDC_COMBO_NUMLOAD, CB_INSERTSTRING, (WPARAM) 0, (LONG) (LPTSTR) "   Ctrl # Load, Shift # save,        # select");

        SendDlgItemMessage(hDlg, IDC_COMBO_PORT1, CB_SETCURSEL, (WPARAM)((Controller_1_Type >> 4) & 1), (LPARAM)0);
        SendDlgItemMessage(hDlg, IDC_COMBO_PORT2, CB_SETCURSEL, (WPARAM)((Controller_2_Type >> 4) & 1), (LPARAM)0);

        SendDlgItemMessage(hDlg, IDC_COMBO_PADP1, CB_SETCURSEL, (WPARAM)(Controller_1_Type & 1), (LPARAM)0);
        SendDlgItemMessage(hDlg, IDC_COMBO_PADP1B, CB_SETCURSEL, (WPARAM)(Controller_1B_Type & 1), (LPARAM)0);
        SendDlgItemMessage(hDlg, IDC_COMBO_PADP1C, CB_SETCURSEL, (WPARAM)(Controller_1C_Type & 1), (LPARAM)0);
        SendDlgItemMessage(hDlg, IDC_COMBO_PADP1D, CB_SETCURSEL, (WPARAM)(Controller_1D_Type & 1), (LPARAM)0);
        SendDlgItemMessage(hDlg, IDC_COMBO_PADP2, CB_SETCURSEL, (WPARAM)(Controller_2_Type & 1), (LPARAM)0);
        SendDlgItemMessage(hDlg, IDC_COMBO_PADP2B, CB_SETCURSEL, (WPARAM)(Controller_2B_Type & 1), (LPARAM)0);
        SendDlgItemMessage(hDlg, IDC_COMBO_PADP2C, CB_SETCURSEL, (WPARAM)(Controller_2C_Type & 1), (LPARAM)0);
        SendDlgItemMessage(hDlg, IDC_COMBO_PADP2D, CB_SETCURSEL, (WPARAM)(Controller_2D_Type & 1), (LPARAM)0);
        SendDlgItemMessage(hDlg, IDC_CHECK_LEFTRIGHT, BM_SETCHECK, (WPARAM)(LeftRightEnabled) ? BST_CHECKED : BST_UNCHECKED, 0); //Modif
        SendDlgItemMessage(hDlg, IDC_CHECK_BGINPUT, BM_SETCHECK, (WPARAM)(BackgroundInput) ? BST_CHECKED : BST_UNCHECKED, 0); //Modif
        /*if (NumLoadEnabled)*/ EnableWindow(GetDlgItem(hDlg, IDC_COMBO_NUMLOAD), TRUE);
        //else EnableWindow(GetDlgItem(hDlg,IDC_COMBO_NUMLOAD),FALSE);
        //SendDlgItemMessage(hDlg, IDC_CHECK_NUMLOAD, BM_SETCHECK, (WPARAM) (NumLoadEnabled)?BST_CHECKED:BST_UNCHECKED, 0); //Modif N.
        //if (StateSelectCfg > 5)
        //  StateSelectCfg = 0;
        //SendDlgItemMessage(hDlg, IDC_COMBO_NUMLOAD, CB_SETCURSEL, (WPARAM) (StateSelectCfg), (LPARAM) 0);
        SendDlgItemMessage(hDlg, INPUT_FRAMEADVSKIPLAG, BM_SETCHECK, (WPARAM)(frameadvSkipLag) ? BST_CHECKED : BST_UNCHECKED, 0);

        EnableWindow(GetDlgItem(hDlg, IDC_REASSIGNKEY), FALSE);
        EnableWindow(GetDlgItem(hDlg, IDC_REVERTKEY), FALSE);
        EnableWindow(GetDlgItem(hDlg, IDC_USEDEFAULTKEY), FALSE);
        EnableWindow(GetDlgItem(hDlg, IDC_DISABLEKEY), FALSE);

        HWND listbox = GetDlgItem(hDlg, IDC_HOTKEYLIST);

        int stops[4] = { 25 * 4, 25 * 4 + 4, 25 * 4 + 8, 25 * 4 + 16 };
        SendMessage(listbox, LB_SETTABSTOPS, 4, (LONG)(LPSTR)stops);

        PopulateHotkeyListbox(listbox);

        RefreshEnabledPorts(hDlg);

        return true;
    }   break;

    case WM_COMMAND:
    {
        int controlID = LOWORD(wParam);
        int messageID = HIWORD(wParam);

        if (messageID == LBN_SELCHANGE && controlID == IDC_HOTKEYLIST)
        {
            int selCount = SendDlgItemMessage(hDlg, IDC_HOTKEYLIST, LB_GETSELCOUNT, (WPARAM)0, (LPARAM)0);
            EnableWindow(GetDlgItem(hDlg, IDC_REASSIGNKEY), (selCount == 1) ? TRUE : FALSE);
            EnableWindow(GetDlgItem(hDlg, IDC_REVERTKEY), (selCount >= 1) ? TRUE : FALSE);
            EnableWindow(GetDlgItem(hDlg, IDC_USEDEFAULTKEY), (selCount >= 1) ? TRUE : FALSE);
            EnableWindow(GetDlgItem(hDlg, IDC_DISABLEKEY), (selCount >= 1) ? TRUE : FALSE);
        }

        switch (controlID)
        {
        case IDOK:
            Controller_1_Type = (SendDlgItemMessage(hDlg, IDC_COMBO_PORT1, CB_GETCURSEL, (WPARAM)0, (LPARAM)0) & 1);
            Controller_1_Type <<= 4;

            Controller_1_Type |= (SendDlgItemMessage(hDlg, IDC_COMBO_PADP1, CB_GETCURSEL, (WPARAM)0, (LPARAM)0) & 1);
            Controller_1B_Type = (SendDlgItemMessage(hDlg, IDC_COMBO_PADP1B, CB_GETCURSEL, (WPARAM)0, (LPARAM)0) & 1);
            Controller_1C_Type = (SendDlgItemMessage(hDlg, IDC_COMBO_PADP1C, CB_GETCURSEL, (WPARAM)0, (LPARAM)0) & 1);
            Controller_1D_Type = (SendDlgItemMessage(hDlg, IDC_COMBO_PADP1D, CB_GETCURSEL, (WPARAM)0, (LPARAM)0) & 1);

            Controller_2_Type = (SendDlgItemMessage(hDlg, IDC_COMBO_PORT2, CB_GETCURSEL, (WPARAM)0, (LPARAM)0) & 1);
            Controller_2_Type <<= 4;

            Controller_2_Type |= (SendDlgItemMessage(hDlg, IDC_COMBO_PADP2, CB_GETCURSEL, (WPARAM)0, (LPARAM)0) & 1);
            Controller_2B_Type = (SendDlgItemMessage(hDlg, IDC_COMBO_PADP2B, CB_GETCURSEL, (WPARAM)0, (LPARAM)0) & 1);
            Controller_2C_Type = (SendDlgItemMessage(hDlg, IDC_COMBO_PADP2C, CB_GETCURSEL, (WPARAM)0, (LPARAM)0) & 1);
            Controller_2D_Type = (SendDlgItemMessage(hDlg, IDC_COMBO_PADP2D, CB_GETCURSEL, (WPARAM)0, (LPARAM)0) & 1);
            LeftRightEnabled = (SendDlgItemMessage(hDlg, IDC_CHECK_LEFTRIGHT, BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0;//Modif
            BackgroundInput = (SendDlgItemMessage(hDlg, IDC_CHECK_BGINPUT, BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0;//Modif
            //NumLoadEnabled = (SendDlgItemMessage(hDlg, IDC_CHECK_NUMLOAD, BM_GETCHECK, 0, 0) == BST_CHECKED)?1:0;//Modif N.
            //StateSelectCfg = (unsigned char)SendDlgItemMessage(hDlg, IDC_COMBO_NUMLOAD, CB_GETCURSEL, (WPARAM) 0, (LPARAM) 0);
            Make_IO_Table();
            End_Input();
            DialogsOpen--;
            EndDialog(hDlg, true);
            return true;
            break;

        case IDC_BUTTON_SETKEYSP1:
            Setting_Keys(hDlg, 0, (SendDlgItemMessage(hDlg, IDC_COMBO_PADP1, CB_GETCURSEL, (WPARAM)0, (LPARAM)0) & 1));
            return true;
            break;

        case IDC_BUTTON_SETKEYSP1B:
            Setting_Keys(hDlg, 2, (SendDlgItemMessage(hDlg, IDC_COMBO_PADP1B, CB_GETCURSEL, (WPARAM)0, (LPARAM)0) & 1));
            return true;
            break;

        case IDC_BUTTON_SETKEYSP1C:
            Setting_Keys(hDlg, 3, (SendDlgItemMessage(hDlg, IDC_COMBO_PADP1C, CB_GETCURSEL, (WPARAM)0, (LPARAM)0) & 1));
            return true;
            break;

        case IDC_BUTTON_SETKEYSP1D:
            Setting_Keys(hDlg, 4, (SendDlgItemMessage(hDlg, IDC_COMBO_PADP1D, CB_GETCURSEL, (WPARAM)0, (LPARAM)0) & 1));
            return true;
            break;

        case IDC_BUTTON_SETKEYSP2:
            Setting_Keys(hDlg, 1, (SendDlgItemMessage(hDlg, IDC_COMBO_PADP2, CB_GETCURSEL, (WPARAM)0, (LPARAM)0) & 1));
            return true;
            break;

        case IDC_BUTTON_SETKEYSP2B:
            Setting_Keys(hDlg, 5, (SendDlgItemMessage(hDlg, IDC_COMBO_PADP2B, CB_GETCURSEL, (WPARAM)0, (LPARAM)0) & 1));
            return true;
            break;

        case IDC_BUTTON_SETKEYSP2C:
            Setting_Keys(hDlg, 6, (SendDlgItemMessage(hDlg, IDC_COMBO_PADP2C, CB_GETCURSEL, (WPARAM)0, (LPARAM)0) & 1));
            return true;
            break;

        case IDC_BUTTON_SETKEYSP2D:
            Setting_Keys(hDlg, 7, (SendDlgItemMessage(hDlg, IDC_COMBO_PADP2D, CB_GETCURSEL, (WPARAM)0, (LPARAM)0) & 1));
            return true;
            break;

        case IDC_REASSIGNKEY:
        case IDC_REVERTKEY:
        case IDC_USEDEFAULTKEY:
        case IDC_DISABLEKEY:
            ModifyHotkeyFromListbox(GetDlgItem(hDlg, IDC_HOTKEYLIST), controlID, Tex0, hDlg);
            break;
        case INPUT_FRAMEADVSKIPLAG:
            frameadvSkipLag ^= 1;
            break;

        case IDC_COMBO_PORT1:
        case IDC_COMBO_PORT2:
            RefreshEnabledPorts(hDlg);
            return true;
        }
    }   break;
    case WM_CLOSE:
        End_Input();
        DialogsOpen--;
        EndDialog(hDlg, true);
        return true;
        break;
    }

    return false;
}
int SaveFlags()
{
    int flags = Z80_State & 0x1;
    flags |= ((Sound_Rate / 22050) << 10);
    flags |= LeftRightEnabled << 16;
    return flags;
}
void LoadFlags(int flags)
{
    if (flags & 0x1)
        Z80_State |= 0x1;
    else
        Z80_State &= ~0x1;
    Change_Sample_Rate(HWnd, (flags >> 10) & 0x3);
    LeftRightEnabled = (flags >> 16) & 1;
    return;
}

#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

