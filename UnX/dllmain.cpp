/**
 * This file is part of UnX.
 *
 * UnX is free software : you can redistribute it
 * and/or modify it under the terms of the GNU General Public License
 * as published by The Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * UnX is distributed in the hope that it will be useful,
 *
 * But WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with UnX.
 *
 *   If not, see <http://www.gnu.org/licenses/>.
 *
**/
#include <Windows.h>

#include "config.h"
#include "log.h"
#include "command.h"

#include "hook.h"

#include "language.h"
#include "timing.h"
#include "display.h"
#include "input.h"
#include "window.h"

#include <process.h>
#include <comdef.h>

#pragma comment (lib, "kernel32.lib")

extern "C" BOOL WINAPI _CRT_INIT (HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved);

HMODULE hDLLMod      = { 0 }; // Handle to SELF
HMODULE hInjectorDLL = { 0 }; // Handle to Special K

typedef HRESULT (__stdcall *SK_UpdateSoftware_pfn)(const wchar_t* wszProduct);
typedef bool    (__stdcall *SK_FetchVersionInfo_pfn)(const wchar_t* wszProduct);

typedef void (__stdcall *SK_SetPluginName_pfn)(std::wstring name);
SK_SetPluginName_pfn SK_SetPluginName = nullptr;

std::wstring injector_name;

DWORD
__stdcall
DllThread (LPVOID user)
{
  std::wstring plugin_name = L"Untitled Project X v " + UNX_VER_STR;

  //
  // Until we modify the logger to use Kernel32 File I/O, we have to
  //   do this stuff from here... dependency DLLs may not be loaded
  //     until initial DLL startup finishes and threads are allowed
  //       to start.
  //
  dll_log = UNX_CreateLog (L"logs/UnX.log");

  dll_log->LogEx ( false, L"--------------- [Untitled X] "
                          L"---------------\n" );
  dll_log->Log   (        L"Untitled X Plug-In\n"
                          L"============ (Version: v %s) "
                          L"============",
                            UNX_VER_STR.c_str () );

  if (! UNX_LoadConfig ()) {
    config.system.injector = injector_name;

    // Save a new config if none exists
    UNX_SaveConfig ();
  }

  config.system.injector = injector_name;

  SK_SetPluginName = 
    (SK_SetPluginName_pfn)
      GetProcAddress (hInjectorDLL, "SK_SetPluginName");

  SK_GetCommandProcessor =
    (SK_GetCommandProcessor_pfn)
      GetProcAddress (hInjectorDLL, "SK_GetCommandProcessor");

  //
  // If this is NULL, the injector system isn't working right!!!
  //
  if (SK_SetPluginName != nullptr)
    SK_SetPluginName (plugin_name);

  // Plugin State
  if (UNX_Init_MinHook () == MH_OK) {
    CoInitializeEx (nullptr, COINIT_MULTITHREADED);

    unx::LanguageManager::Init ();
    unx::DisplayFix::Init      ();
    //unx::RenderFix::Init       ();
    unx::InputManager::Init    ();

    unx::TimingFix::Init       ();

    unx::WindowManager::Init   ();
  }

  return 0;
}

__declspec (dllexport)
BOOL
WINAPI
SKPlugIn_Init (HMODULE hModSpecialK)
{
  wchar_t wszSKFileName [MAX_PATH];
          wszSKFileName [MAX_PATH - 1] = L'\0';

  GetModuleFileName (hModSpecialK, wszSKFileName, MAX_PATH - 1);

  injector_name = wszSKFileName;

  hInjectorDLL = hModSpecialK;


  SK_UpdateSoftware_pfn SK_UpdateSoftware =
    (SK_UpdateSoftware_pfn)
      GetProcAddress ( hInjectorDLL,
                         "SK_UpdateSoftware" );

  SK_FetchVersionInfo_pfn SK_FetchVersionInfo =
    (SK_FetchVersionInfo_pfn)
      GetProcAddress ( hInjectorDLL,
                         "SK_FetchVersionInfo" );

  if (! wcsstr (injector_name.c_str (), L"SpecialK")) {
    if ( SK_FetchVersionInfo != nullptr &&
         SK_UpdateSoftware   != nullptr ) {
      if (SK_FetchVersionInfo (L"UnX")) {
        SK_UpdateSoftware (L"UnX");
      }
    }
  }

  DllThread (nullptr);

  return TRUE;
}

BOOL
APIENTRY
DllMain (HMODULE hModule,
         DWORD   ul_reason_for_call,
         LPVOID  /* lpReserved */)
{
  switch (ul_reason_for_call)
  {
    case DLL_PROCESS_ATTACH:
    {
      _CRT_INIT ((HINSTANCE)hModule, ul_reason_for_call, nullptr);
      hDLLMod = hModule;
    } break;

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
      _CRT_INIT ((HINSTANCE)hModule, ul_reason_for_call, nullptr);
      break;

    case DLL_PROCESS_DETACH:
    {
      if (dll_log != nullptr) {
        unx::LanguageManager::Shutdown ();
        unx::WindowManager::Shutdown   ();
        //unx::RenderFix::Shutdown       ();
        unx::InputManager::Shutdown    ();
        unx::TimingFix::Shutdown       ();
        unx::DisplayFix::Shutdown      ();

        UNX_UnInit_MinHook ();
        UNX_SaveConfig     ();

        dll_log->LogEx ( false, L"============ (Version: v %s) "
                                L"============\n",
                                  UNX_VER_STR.c_str () );
        dll_log->LogEx ( true,  L"End unx.dll\n" );
        dll_log->LogEx ( false, L"--------------- [Untitled X] "
                                L"---------------" );

        dll_log->close ();
      }
      _CRT_INIT ((HINSTANCE)hModule, ul_reason_for_call, nullptr);
    } break;
  }

  return TRUE;
}