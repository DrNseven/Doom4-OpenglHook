#include <windows.h>
#include <stdio.h>
#pragma comment(lib,"winmm.lib")

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

typedef BOOL(__stdcall* tSwapBuffers) (HDC unnamedParam1); //GDI swapbuffers
tSwapBuffers orig_SwapBuffers;

typedef BOOL(__stdcall* twglSwapBuffers) (HDC hDc); //opengl swapbuffers
twglSwapBuffers orig_wglSwapBuffers = NULL;

static void (WINAPI* orig_glBindMultiTextureEXT) (GLenum texunit, GLenum target, GLuint texture) = NULL; //called by doom4 2016
static void (WINAPI* orig_glDrawElementsBaseVertex) (GLenum mode, GLsizei count, GLenum type, const void* indices, GLint baseVertex) = NULL;//doom4 2016

static void (WINAPI* orig_glBindTexture) (GLenum target, GLuint texture) = glBindTexture; //called by quake3, enemy territory ect.
static void (WINAPI* orig_glDrawElements) (GLenum mode, GLsizei count, GLenum type, const void * indices) = glDrawElements; //q3, et
static void (WINAPI* orig_glVertexPointer) (GLint size, GLenum type, GLsizei stride, const void* pointer) = glVertexPointer; //q3, et
static void (WINAPI* orig_glTexCoordPointer) (GLint size, GLenum type, GLsizei stride, const void* pointer) = glTexCoordPointer; //q3, et, cod1


//Globals
bool InitOnce1 = false;
bool InitOnce2 = false;
bool InitOnce3 = false;
bool InitOnce4 = false;
bool InitOnce5 = false;
bool InitOnce6 = false;
bool InitOnce7 = false;
bool InitOnce8 = false;
bool showmenu = false;
GLuint texture2d;
unsigned int cubemap;

//item states
bool wallhack = 1;
bool chams = 0;
int countnum = -1;

DWORD gameWidth;  //game window width
DWORD gameHeight; //game window height

unsigned int asdelay = 100;                       //wait ms before MOUSEEVENTF_LEFTUP
DWORD dwLastAction = timeGetTime();             //as timer
bool IsPressed = false;                         //is left mouse down, are we shooting

//other
#include "main.h"


LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
        return true;

    if (showmenu)
        return 1;

    return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

BOOL __stdcall hooked_SwapBuffers(HDC hDC) // GDI32
{
    if (InitOnce1 == FALSE)
    {
        InitOnce1 = TRUE;
        Log("GDI SwapBuffers hooked");
        hwndWindow = GetProcessWindow();
    }

    //get gamewindow size
    //gameWidth = GetSystemMetrics(SM_CXSCREEN);
    //gameHeight = GetSystemMetrics(SM_CYSCREEN);

    //readPixelsAndShoot();

    return orig_SwapBuffers(hDC);
}


BOOL __stdcall hooked_wglSwapBuffers(HDC hDc)
{
    if (InitOnce2 == FALSE)
    {
        InitOnce2 = TRUE;
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

    //get gamewindow size
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
        //ImGui::SetNextWindowPos(ImVec2(50.0f, 400.0f)); //pos
        ImGui::SetNextWindowSize(ImVec2(510.0f, 400.0f)); //size
        ImVec4 Bgcol = ImColor(0.0f, 0.4f, 0.28f, 0.8f); //bg color
        ImGui::PushStyleColor(ImGuiCol_WindowBg, Bgcol);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.2f, 0.2f, 0.2f, 0.8f)); //frame color

        ImGui::Begin("OpenGL Imgui");
        ImGui::Text("countnum == %d", countnum);
        ImGui::Text("gameWidth == %d", gameWidth);
        ImGui::Text("gameHeight == %d", gameHeight);

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

void WINAPI hooked_glBindMultiTextureEXT(GLenum texunit, GLenum target, GLuint texture)
{
    if (InitOnce3 == FALSE)
    {
        InitOnce3 = TRUE;
        Log("glBindMultiTextureEXT hooked");
        glGenTextures(1, &texture2d);
        glGenTextures(1, &cubemap);
    }
   
    return orig_glBindMultiTextureEXT(texunit, target, texture);
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

        glEnable(GL_TEXTURE_GEN_S); //enable texture coordinate generation, to draw to any texture
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
        //glPointSize((float)20.0f);
        //glBegin(GL_POINTS);
        //glVertex2f(0, 0);
        //glEnd();

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


void WINAPI hooked_glBindTexture(GLenum target, GLuint texture)
{
    if (InitOnce5 == FALSE)
    {
        InitOnce5 = TRUE;
        Log("glBindTexture hooked");
    }

    return orig_glBindTexture(target, texture);
}

void WINAPI hooked_glDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices)
{
    if (InitOnce6 == FALSE)
    {
        InitOnce6 = TRUE;
        Log("glDrawElements hooked");
    }

    return orig_glDrawElements(mode, count, type, indices);
}

void WINAPI hooked_glVertexPointer(GLint size, GLenum type, GLsizei stride, const void* pointer)
{
    if (InitOnce7 == FALSE)
    {
        InitOnce7 = TRUE;
        Log("glVertexPointer hooked");
    }

    return orig_glVertexPointer(size, type, stride, pointer);
}

void WINAPI hooked_glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const void* pointer)
{
    if (InitOnce8 == FALSE)
    {
        InitOnce8 = TRUE;
        Log("glTexCoordpointer hooked");
    }

    return orig_glTexCoordPointer(size, type, stride, pointer);
}


DWORD WINAPI InitSwapbuffers(LPVOID lpParameter)
{
    while (GetModuleHandle(L"opengl32.dll") == NULL) { Sleep(100); }
    Sleep(100);
    
    HMODULE hMod = GetModuleHandle(L"opengl32.dll");
    if (hMod)
    {
        orig_wglSwapBuffers = (twglSwapBuffers)(PVOID)GetProcAddress(hMod, "wglSwapBuffers"); //gl

        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourAttach(&(PVOID&)orig_wglSwapBuffers, hooked_wglSwapBuffers);
        DetourTransactionCommit();

        do
            hwndWindow = GetProcessWindow(); //GetForegroundWindow();
        while (hwndWindow == NULL);

        oWndProc = (WNDPROC)SetWindowLongPtr(hwndWindow, GWL_WNDPROC, (LONG_PTR)WndProc);
    }
    
    HMODULE gMod = GetModuleHandle(L"gdi32.dll");
    if (gMod)
    {
        orig_SwapBuffers = (tSwapBuffers)(PVOID)GetProcAddress(gMod, "SwapBuffers"); //gdi

        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourAttach(&(PVOID&)orig_SwapBuffers, hooked_SwapBuffers);
        DetourTransactionCommit();
    }

    return true;
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved)
{
    if (dwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinst);
        CreateThread(0, 0, InitSwapbuffers, 0, 0, 0);

        orig_glDrawElementsBaseVertex = (void (WINAPI*)(GLenum, GLsizei, GLenum, const void*, GLint))wglGetProcAddress("glDrawElementsBaseVertex");
        orig_glBindMultiTextureEXT = (void (WINAPI*)(GLenum, GLenum, GLuint))wglGetProcAddress("glBindMultiTextureEXT");

        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourAttach(&(PVOID&)orig_glBindMultiTextureEXT, hooked_glBindMultiTextureEXT);
        DetourAttach(&(PVOID&)orig_glDrawElementsBaseVertex, hooked_glDrawElementsBaseVertex);

        DetourAttach(&(PVOID&)orig_glBindTexture, hooked_glBindTexture);
        DetourAttach(&(PVOID&)orig_glDrawElements, hooked_glDrawElements);
        DetourAttach(&(PVOID&)orig_glVertexPointer, hooked_glVertexPointer);
        DetourAttach(&(PVOID&)orig_glTexCoordPointer, hooked_glTexCoordPointer);     
        DetourTransactionCommit();
    }
    else if (dwReason == DLL_PROCESS_DETACH) {
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourDetach(&(PVOID&)orig_glBindMultiTextureEXT, hooked_glBindMultiTextureEXT);
        DetourDetach(&(PVOID&)orig_glDrawElementsBaseVertex, hooked_glDrawElementsBaseVertex);

        DetourDetach(&(PVOID&)orig_glBindTexture, hooked_glBindTexture);
        DetourDetach(&(PVOID&)orig_glDrawElements, hooked_glDrawElements);
        DetourDetach(&(PVOID&)orig_glVertexPointer, hooked_glVertexPointer);
        DetourDetach(&(PVOID&)orig_glTexCoordPointer, hooked_glTexCoordPointer);
        DetourTransactionCommit();
    }

    return TRUE;
}
