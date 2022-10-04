#include <windows.h>
#include <stdio.h>

//glew
#define GLEW_STATIC
#if defined _M_X64
#include "glew\glew.h"
#pragma comment(lib, "glew/x64//glew32s.lib")
#elif defined _M_IX86
#include "glew\glew.h"
#pragma comment(lib, "glew/x86/glew32s.lib")
#endif
#include <gl/GL.h>
#pragma comment(lib, "OpenGL32.lib")

//detours
#include "detours.h"
#if defined _M_X64
#pragma comment(lib, "detours.X64/detours.lib")
#elif defined _M_IX86
#pragma comment(lib, "detours.X86/detours.lib")
#endif

//imgui
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_opengl3.h"
#include "ImGui/imgui_impl_win32.h"
#include "ImGui/imgui_stdlib.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
typedef LRESULT(CALLBACK* WNDPROC)(HWND, UINT, WPARAM, LPARAM);

#ifdef _WIN64
#define GWL_WNDPROC GWLP_WNDPROC
#endif

WNDPROC oWndProc;
static HWND hwndWindow = NULL;

typedef BOOL(__stdcall* twglSwapBuffers) (HDC hDc); //opengl swapbuffers
twglSwapBuffers orig_wglSwapBuffers = NULL;

typedef BOOL(__stdcall* tSwapBuffers) (HDC unnamedParam1); //GDI swapbuffers
tSwapBuffers orig_SwapBuffers;

static void (WINAPI* orig_glClear) (GLbitfield mask) = glClear;
static void (WINAPI* orig_glDrawElements) (GLenum mode, GLsizei count, GLenum type, const void * indices) = glDrawElements;
//todo: glBindTexture
static void (WINAPI* orig_glDrawElementsBaseVertex) (GLenum mode, GLsizei count, GLenum type, const void * indices, GLint baseVertex) = NULL;
static void (WINAPI* orig_glBindMultiTextureEXT) (GLenum texunit, GLenum target, GLuint texture) = NULL;



//Globals
bool InitOnce1 = false;
bool InitOnce2 = false;
bool InitOnce3 = false;
bool InitOnce4 = false;
bool InitOnce5 = false;
bool InitOnce6 = false;
bool showmenu = false;
GLuint texture2d;
unsigned int cubemap;

//item states
bool wallhack = 1;
bool chams = 0;
int countnum = -1;

DWORD gameWidth;  //game window width
DWORD gameHeight; //game window height

//get dir
using namespace std;
#include <fstream>
char dlldir[320];
char* GetDirectoryFile(char* filename)
{
    static char path[320];
    strcpy_s(path, dlldir);
    strcat_s(path, filename);
    return path;
}

//log
void Log(const char* fmt, ...)
{
    if (!fmt)	return;

    char		text[4096];
    va_list		ap;
    va_start(ap, fmt);
    vsprintf_s(text, fmt, ap);
    va_end(ap);

    ofstream logfile(GetDirectoryFile((PCHAR)"log.txt"), ios::app);
    if (logfile.is_open() && text)	logfile << text << endl;
    logfile.close();
}

HWND tmp_window = NULL;
BOOL CALLBACK EnumWindowsCallback(HWND handle, LPARAM lParam)
{
    DWORD wndProcId;
    GetWindowThreadProcessId(handle, &wndProcId);

    if (GetCurrentProcessId() != wndProcId)
        return TRUE; // skip to next window

    tmp_window = handle;
    return FALSE; // window found abort search
}

HWND GetProcessWindow()
{
    tmp_window = NULL;
    EnumWindows(EnumWindowsCallback, NULL);
    return tmp_window;
}


class BotCoordinate {
public:
    long x;
    long y;
    BotCoordinate(long x, long y) {
        this->x = x;
        this->y = y;
    }
    BotCoordinate diff(BotCoordinate coord2) {
        return BotCoordinate(x - coord2.x, y - coord2.y);
    }
    bool equals(BotCoordinate coord2) {
        return x == coord2.x && y == coord2.y;
    }
    bool lessThan(long i) {
        return abs(x) < i && abs(y) < i;
    }


};

// This class needs to be 3 bytes long. Do not add virtual functions since this will add a vtable pointer to the struct
class Rgb {

public:
    BYTE r;
    BYTE g;
    BYTE b;
    bool rgbInRange(Rgb compare, long range) {
        return abs((long)r - (long)compare.r) <= range &&
            abs((long)g - (long)compare.g) <= range &&
            abs((long)b - (long)compare.b) <= range;

    }
    bool rgbEqualPlayer() {
        Rgb player = { 255, 0, 0 };//red
        return rgbInRange(player, 50);
    }
    char* format(char* buffer) {
        //sprintf(buffer, "Rgb(%ld,%ld,%ld)", r, g, b);
        return buffer;
    }
};

char* renderedTexture = NULL;
BotCoordinate closestEnemyPixelToCrosshair = BotCoordinate(-1, -1);

#define isSnipingKeyDown() (GetKeyState(VK_SHIFT) & 0x8000)
#define isShootingKeyDown() (GetKeyState(VK_LBUTTON) & 0x8000)

typedef int TEAM;
#define TEAM_AXIS 0
#define TEAM_ALLIES 1

// used to perform a manual binary search when finding models by stride count in hooked_glDrawElements
int high_count = 0;
int low_count = 0;

char enemyTextureString[256];
int headshotLen = 0;

#define AIM_FIELD_OF_VIEW 100//200

BotCoordinate screenCenter() {
    return BotCoordinate(gameWidth / 2, gameHeight / 2);
}

/*
  //you can also put this in a thread
        //for (auto start = std::chrono::steady_clock::now(), now = start; now < start + std::chrono::seconds{ 10 }; now = std::chrono::steady_clock::now())
        //{
            //do something
        //}
        //printf("Did something for 10 seconds.\n");

*/

void click() {
    INPUT input;
    input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    input.type = INPUT_MOUSE;
    input.mi.dy = 0;
    input.mi.dx = 0;
    input.mi.mouseData = 0;
    input.mi.dwExtraInfo = 0;
    input.mi.time = 0;
    SendInput(1, &input, sizeof(input));
    input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(1, &input, sizeof(input));
}

DWORD WINAPI reverseSnipingRecoil(LPVOID) {
    Sleep(500);
    INPUT input;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    input.type = INPUT_MOUSE;
    input.mi.dy = 260;
    input.mi.dx = 0;
    input.mi.mouseData = 0;
    input.mi.dwExtraInfo = 0;
    input.mi.time = 0;

    SendInput(1, &input, sizeof(input));
    return 0;
}

void applyAimBotMouseMovement(BotCoordinate shotCoordinate) 
{
    BotCoordinate diff = shotCoordinate.diff(screenCenter());
    INPUT input;
    double factor = 5; //0.5;
    double maximum = 20; //8.0;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    input.type = INPUT_MOUSE;

    if (abs(diff.x) < 3 && abs(diff.y < 3)) {
        factor = 1.0;
    }

    if (isSnipingKeyDown()) {
        factor = 1.0;
    }

    input.mi.dy = (long)(min((double)diff.y * factor, maximum));
    input.mi.dx = (long)(min((double)diff.x * factor, maximum));
    input.mi.mouseData = 0;
    input.mi.dwExtraInfo = 0;
    input.mi.time = 0;

    SendInput(1, &input, sizeof(input));

    if (isSnipingKeyDown() && diff.lessThan(2)) {
        click();
        //CreateThread(NULL, 0, reverseSnipingRecoil, NULL, 0, NULL);
    }
    
}

Rgb pixels[10000000];
void readPixelsAndShoot() {
    closestEnemyPixelToCrosshair = BotCoordinate(10, 10);

    if (!isSnipingKeyDown() || GetForegroundWindow() != hwndWindow)
    //if ((!isShootingKeyDown() && !isSnipingKeyDown())||GetForegroundWindow() != hwndWindow) 
    {
        return;
    }

    BotCoordinate center = screenCenter();
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(center.x - AIM_FIELD_OF_VIEW, center.y - AIM_FIELD_OF_VIEW, AIM_FIELD_OF_VIEW * 2, AIM_FIELD_OF_VIEW * 2, GL_RGB, GL_UNSIGNED_BYTE, pixels);

    // optimised by reading out from pixels at the center of the screen first. 
    // bottomLeftCorner begins in the center of the screen and moves to the bottom left corner of the sticky aim square. 
    // When a pixel is found matching the enemy colour, apply aimbot mouse movement
    long bottomLeftCorner = AIM_FIELD_OF_VIEW - 1;
    long width = 2;
    while (bottomLeftCorner >= 0) {
        for (long y = 0; y < width; y++) {
            for (long x = 0; x < width; x++) {

                long ycoord = (bottomLeftCorner + y);
                long xcoord = bottomLeftCorner + x;

                Rgb p = pixels[xcoord + ycoord * (AIM_FIELD_OF_VIEW + AIM_FIELD_OF_VIEW)];

                if (p.rgbEqualPlayer()) {
                    // y goes the other way when converting glReadPixel coordinates to screen coordinates
                    closestEnemyPixelToCrosshair = BotCoordinate((gameWidth / 2 - AIM_FIELD_OF_VIEW) + xcoord, (gameHeight / 2 + AIM_FIELD_OF_VIEW) - ycoord);
                    applyAimBotMouseMovement(closestEnemyPixelToCrosshair);
                    return;
                }
                // skip pixels we have already tested since we read inner pixels before the outer pixels
                if (y != 0 && y != width - 1 && x == 0) {
                    x = width - 2;
                }
            }
        }
        bottomLeftCorner--;
        width += 2;
    }
    closestEnemyPixelToCrosshair = BotCoordinate(10, 10);
}

void drawAimbotSquare() {
    //drawSquare(screenCenter(), AIM_FIELD_OF_VIEW + 3);
}

void drawAimAtDisplay() {
    if (closestEnemyPixelToCrosshair.x == -1) return;
    //drawSquare(closestEnemyPixelToCrosshair, 5);
}


void WINAPI hooked_glClear(GLbitfield mask)
{
    if (InitOnce5 == FALSE)
    {
        InitOnce5 = TRUE;
        Log("glClear hooked");
    }

    if (mask == GL_DEPTH_BUFFER_BIT)
    {
        mask = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT;
    }

    return orig_glClear(mask);
}


BOOL __stdcall hooked_SwapBuffers(HDC hDC) // GDI32
{
    if (InitOnce6 == FALSE)
    {
        InitOnce6 = TRUE;
        Log("GDI SwapBuffers hooked");
    }

    readPixelsAndShoot();

    return orig_SwapBuffers(hDC);
}


BOOL __stdcall hooked_wglSwapBuffers(HDC hDc)
{
    if (InitOnce1 == FALSE)
    {
        InitOnce1 = TRUE;
        Log("wglSwapBuffers hooked");
        hwndWindow = GetProcessWindow();

        glewExperimental = GL_TRUE;

        if (glewInit() != GLEW_OK)
        {
            Log("Failed to initialize GLEW");
        }

        if (glewInit() == GLEW_OK)
        {
            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO();
            io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;
            ImGui_ImplWin32_Init(hwndWindow);
            ImGui_ImplOpenGL3_Init();
        }
    }

    //get desktop size
    gameWidth = GetSystemMetrics(SM_CXSCREEN);
    gameHeight = GetSystemMetrics(SM_CYSCREEN);

    if ((GetAsyncKeyState(VK_INSERT) & 1) && (hwndWindow == GetFocus()))
        showmenu = !showmenu;

    //if (GetAsyncKeyState(VK_END) & 1) // Unload
    //{
    //    MH_DisableHook(MH_ALL_HOOKS);
    //    SetWindowLongPtr(Window, GWL_WNDPROC, (LONG_PTR)oWndProc); // Reset WndProc
    //}

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // HACK MENU
    if (showmenu)
    {
        ImGui::Begin("OpenGL Imgui");
        ImGui::Text("countnum == %d", countnum);
        ImGui::Text("gameWidth == %d", gameWidth/2);
        ImGui::Text("gameHeight == %d", gameHeight/2);

        //ImGui::Text("Project under development.");
        ImGui::Separator();

        //if (ImGui::CollapsingHeader("Stats")) {
          //  ImGui::Text("Health: %d/%d", health, healthmax);
            //ImGui::Text("Mana: %d/%d", mana, manamax);
        //}
        ImGui::Separator();

        if (ImGui::CollapsingHeader("Wallhack")) {
            ImGui::Checkbox("Wallhack", &wallhack);
            //ImGui::Checkbox("Enabled##Healing", &enabled_auto_heal);
        }
        ImGui::Separator();


        if (ImGui::CollapsingHeader("Chams")) {
            ImGui::Checkbox("Chams", &chams);
            ImGui::Text("Use arrows to navigate.");
        }

        ImGui::End();
    }

    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());


    readPixelsAndShoot();
    //drawAimbotSquare();
    //drawAimAtDisplay();

    return orig_wglSwapBuffers(hDc);
}
#pragma endregion

#pragma region WndProc
LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
        return true;

    if (showmenu)
        return 1;

    return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}




int dcount;
void WINAPI hooked_glBindMultiTextureEXT(GLenum texunit, GLenum target, GLuint texture)
{
    if (InitOnce2 == FALSE)
    {
        InitOnce2 = TRUE;
        Log("glBindMultiTextureEXT hooked");
        glGenTextures(1, &texture2d);
        glGenTextures(1, &cubemap);
    }
   
    return orig_glBindMultiTextureEXT(texunit, target, texture);
}

void WINAPI hooked_glDrawElements(GLenum mode, GLsizei count, GLenum type, const void * indices) 
{
    if (InitOnce3 == FALSE)
    {
        InitOnce3 = TRUE;
        Log("glDrawElements hooked");
    }

    return orig_glDrawElements(mode, count, type, indices);
}

void WINAPI hooked_glDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, const void * indices, GLint baseVertex) 
{
    if (InitOnce4 == FALSE)
    {
        InitOnce4 = TRUE;
        Log("glDrawElementsBaseVertex hooked");
    }
    
    glColorMask(1, 1, 1, 1); //let all colors through
    //uac skin
    if(count == 11982||
        //demonic skin
        count == 12558|| count == 12702||
        //utilitarian skin
        count == 11670||
        //templar skin
        count == 11028||count == 11958||count == 11706||
        //bounty hunter skin
        count == 11502||count == 11265||
        //robotic skin
        count == 12042||
        //cyberdemonic skn
        count == 13086||count == 13236||count == 13452||count == 13302||
        //evil cultist skin
        count == 11922||count == 11634)
    {
        glColorMask(1, 0, 0, 1); //red

        // save OpenGL state
        glPushMatrix();
        glPushAttrib(GL_ALL_ATTRIB_BITS);

        //required?
        glEnable(GL_COLOR_MATERIAL);
        glDisableClientState(GL_COLOR_ARRAY);
        glEnable(GL_TEXTURE_2D);

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_ALWAYS);

        glEnable(GL_TEXTURE_GEN_S); //enable texture coordinate generation
        glEnable(GL_TEXTURE_GEN_T);

        /*
        glBindTexture(GL_TEXTURE_2D, texture2d);
        //green, works in many games but not in doom 2016+
        uint8_t textureColor[16] = {
            0, 255, 0, 0,  // Red
            0, 255, 0, 0,  // Green
            0, 255, 0, 0,  // Blue
            0, 255, 0, 0
        };
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, textureColor);
        */
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap);
        glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGBA, 2, 2, 0, GL_RGB, GL_FLOAT, nullptr);

        glColorMask(1, 0, 0, 1); //red
        //draw point
        //glEnable(GL_POINT_SMOOTH);
        glPointSize((float)20.0f);
        glBegin(GL_POINTS);
        glVertex2f(0, 0);
        glEnd();

        //drawBox(-20.0, -20.0, -20.0, 20.0, 20.0, 20.0, 60, 2.0);
        //drawBox(20.0, 20.0, 20.0, -20.0, -20.0, -20.0, 60, 2.0);

        glDisable(GL_TEXTURE_GEN_S);
        glDisable(GL_TEXTURE_GEN_T);

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        // restore OpenGL state
        glPopAttrib();
        glPopMatrix();
    }


    //Hold down P key until the current texture disappears, press I to log values of those textures
    if (GetAsyncKeyState('O') & 1) //-
        countnum--;
    if (GetAsyncKeyState('P') & 1) //+
        countnum++;
    if (GetAsyncKeyState(VK_MENU) && GetAsyncKeyState('9') & 1) //reset, set to -1
        countnum = -1;

    //Log
    if (countnum == count / 1000)
        if (GetAsyncKeyState('I') & 1)
            Log("count == %d", count);

    //delete texture
    if (countnum == count / 1000)
        return;


    return orig_glDrawElementsBaseVertex(mode, count, type, indices, baseVertex);
}

DWORD WINAPI Initalization(__in  LPVOID lpParameter)
{
    while (GetModuleHandle(L"opengl32.dll") == NULL) { Sleep(100); }
    Sleep(100);

    HMODULE hMod = GetModuleHandle(L"opengl32.dll");
    if (hMod)
    {
        orig_wglSwapBuffers = (twglSwapBuffers)(PVOID)GetProcAddress(hMod, "wglSwapBuffers");

        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourAttach(&(PVOID&)orig_wglSwapBuffers, hooked_wglSwapBuffers);
        DetourTransactionCommit();

        do
            hwndWindow = GetProcessWindow(); //GetForegroundWindow();
        while (hwndWindow == NULL);

        //
        oWndProc = (WNDPROC)SetWindowLongPtr(hwndWindow, GWL_WNDPROC, (LONG_PTR)WndProc);

        return true;
    }

    HMODULE gMod = GetModuleHandle(L"gdi32.dll");
    if (gMod)
    {
        orig_SwapBuffers = (tSwapBuffers)(PVOID)GetProcAddress(gMod, "SwapBuffers");

        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourAttach(&(PVOID&)orig_SwapBuffers, hooked_SwapBuffers);
        DetourTransactionCommit();

        return true;
    }
    else
        return false;
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved)
{
    if (dwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinst);
        CreateThread(0, 0, Initalization, 0, 0, 0);

        orig_glDrawElementsBaseVertex = (void (WINAPI*)(GLenum, GLsizei, GLenum, const void*, GLint))wglGetProcAddress("glDrawElementsBaseVertex");
        orig_glBindMultiTextureEXT = (void (WINAPI*)(GLenum, GLenum, GLuint))wglGetProcAddress("glBindMultiTextureEXT");

        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourAttach(&(PVOID&)orig_glClear, hooked_glClear);
        DetourAttach(&(PVOID&)orig_glDrawElements, hooked_glDrawElements);
        DetourAttach(&(PVOID&)orig_glBindMultiTextureEXT, hooked_glBindMultiTextureEXT);
        DetourAttach(&(PVOID&)orig_glDrawElementsBaseVertex, hooked_glDrawElementsBaseVertex);
        DetourTransactionCommit();
    }
    else if (dwReason == DLL_PROCESS_DETACH) {
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourDetach(&(PVOID&)orig_glClear, hooked_glClear);
        DetourDetach(&(PVOID&)orig_glDrawElements, hooked_glDrawElements);
        DetourDetach(&(PVOID&)orig_glBindMultiTextureEXT, hooked_glBindMultiTextureEXT);
        DetourDetach(&(PVOID&)orig_glDrawElementsBaseVertex, hooked_glDrawElementsBaseVertex);
        DetourTransactionCommit();
    }

    return TRUE;
}
