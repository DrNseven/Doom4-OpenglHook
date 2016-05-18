#include <windows.h>

#define GLEW_STATIC

#if defined _M_X64
#include "GLx64/glew.h"
#pragma comment(lib, "GLx64/glew32s.lib")
#elif defined _M_IX86
#include "GLx86/glew.h"
#pragma comment(lib, "GLx86/glew32s.lib")
#endif

#include <gl/gl.h> 
#pragma comment(lib,"opengl32.lib")

#include "MinHook/include/MinHook.h" //detour x86&x64
//add all minhook files to your project

//==========================================================================================================================

bool FirstInit = false; //init once

#include <fstream>
using namespace std;
char dlldir[320];
char* GetDirectoryFile(char *filename)
{
	static char path[320];
	strcpy_s(path, dlldir);
	strcat_s(path, filename);
	return path;
}

void Log(const char *fmt, ...)
{
	if (!fmt)	return;

	char		text[4096];
	va_list		ap;
	va_start(ap, fmt);
	vsprintf_s(text, fmt, ap);
	va_end(ap);

	ofstream logfile(GetDirectoryFile("log.txt"), ios::app);
	if (logfile.is_open() && text)	logfile << text << endl;
	logfile.close();
}

//==========================================================================================================================

//Typedef the function prototype straight from MSDN
typedef BOOL(__stdcall * twglSwapBuffers) (_In_ HDC hDc);

//Create instance of function
twglSwapBuffers owglSwapBuffers;
 
//Execution will get detoured to this
BOOL __stdcall hwglSwapBuffers(_In_ HDC hDc)
{
	//draw text here

	//Log("hwglSwapBuffers");
	if (FirstInit == FALSE)
	{
		glewExperimental = GL_TRUE;	
		if (glewInit() != GLEW_OK)
		{
			//Log("Failed to initialize GLEW");
		}
		FirstInit = TRUE;
	}

    //return execution to original function
    return owglSwapBuffers(hDc);
}

//==========================================================================================================================

typedef void(*func_glDrawElementsBaseVertex_t) (GLenum mode, GLsizei count, GLenum type, GLvoid *indices, GLint basevertex);
func_glDrawElementsBaseVertex_t oglDrawElementsBaseVertex;

void hglDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, GLvoid *indices, GLint basevertex)
{
	//wallhack here

	oglDrawElementsBaseVertex(mode, count, type, indices, basevertex);
}

//==========================================================================================================================

typedef void(*func_glBindMultiTextureEXT_t) (GLenum texunit, GLenum target, GLuint texture);
func_glBindMultiTextureEXT_t oglBindMultiTextureEXT;

void hglBindMultiTextureEXT(GLenum texunit, GLenum target, GLuint texture)
{
	//model rec here

	glColorMask(0, 1, 0, 1); //test

	oglBindMultiTextureEXT(texunit, target, texture);
}

//==========================================================================================================================

typedef PROC(*func_wglGetProcAddress_t) (LPCSTR lpszProc);
static func_wglGetProcAddress_t _wglGetProcAddress;
func_wglGetProcAddress_t	owglGetProcAddress;

PROC hwglGetProcAddress(LPCSTR ProcName)
{
	//Log("hwglGetProcAddress");
	//wglGetProcAddress is for opengl extensions
	if (!strcmp(ProcName, "glDrawElementsBaseVertex"))
	{
		oglDrawElementsBaseVertex = (func_glDrawElementsBaseVertex_t)owglGetProcAddress(ProcName);
		return (FARPROC)hglDrawElementsBaseVertex;

	}
	else if (!strcmp(ProcName, "glBindMultiTextureEXT"))
	{
		oglBindMultiTextureEXT = (func_glBindMultiTextureEXT_t)owglGetProcAddress(ProcName);
		return (FARPROC)hglBindMultiTextureEXT;
	}
	//glBufferSubData
	//glVertexAttribPointer
	//glViewportIndexedf
	//glGetUniformLocation
	//glBufferData
	//glCompressedTexSubImage2DARB

	return owglGetProcAddress(ProcName);
}

//==========================================================================================================================

DWORD WINAPI OpenglInit(__in  LPVOID lpParameter)
{
	while (GetModuleHandle("opengl32.dll") == 0)
	{
		Sleep(100);
	}

	//HMODULE dll = LoadLibrary(TEXT("opengl32"));
	HMODULE hMod = GetModuleHandle("opengl32.dll");
	if (hMod)
	{
		//use GetProcAddress to find address of wglSwapBuffers in opengl32.dll

		void* ptr = GetProcAddress(hMod, "wglSwapBuffers");
		MH_Initialize();
		MH_CreateHook(ptr, hwglSwapBuffers, reinterpret_cast<void**>(&owglSwapBuffers));
		MH_EnableHook(ptr);

		_wglGetProcAddress = (func_wglGetProcAddress_t)GetProcAddress(hMod, "wglGetProcAddress");
		//MH_Initialize();
		MH_CreateHook(_wglGetProcAddress, hwglGetProcAddress, (void**)&owglGetProcAddress);
		MH_EnableHook(_wglGetProcAddress);
	}

	return 1;
}

//==========================================================================================================================

BOOL __stdcall DllMain (HINSTANCE hinstDll, DWORD fdwReason, LPVOID lpvReserved)
{
	switch(fdwReason)
	{
		case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls (hinstDll);
		GetModuleFileName(hinstDll, dlldir, 512);
		for (int i = strlen(dlldir); i > 0; i--) { if (dlldir[i] == '\\') { dlldir[i + 1] = 0; break; } }
			
		CreateThread(0, 0, OpenglInit, 0, 0, 0); //init
		break;

		case DLL_PROCESS_DETACH:
			break;
	}
	return TRUE;
}

