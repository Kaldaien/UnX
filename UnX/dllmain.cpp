/**
 * This file is part of UnX.
 *
 * UnX is free software : you can red3d11istribute it
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
#include <process.h>
#include <comdef.h>

#include "config.h"
#include "log.h"
#include "hook.h"
#include "command.h"

#include "language.h"
#include "display.h"
#include "input.h"
#include "window.h"


HMODULE      hDLLMod      = { nullptr }; // Handle to SELF
HMODULE      hInjectorDLL = { nullptr }; // Handle to Special K
std::wstring injector_name;

typedef void (__stdcall *SKX_SetPluginName_pfn)           (const wchar_t* name);
                         SKX_SetPluginName_pfn
                         SKX_SetPluginName = nullptr;

typedef void (__stdcall *SK_PlugIn_ControlPanelWidget_pfn)(void);
extern                   SK_PlugIn_ControlPanelWidget_pfn
                         SK_PlugIn_ControlPanelWidget_Original;

extern void   __stdcall  UNX_ControlPanelWidget           (void);


BOOL
__stdcall
DeferredDllMain (       HMODULE  hModSpecialK,
                  const wchar_t* wszFullInjectorPath )
{
  injector_name = wszFullInjectorPath;
  hInjectorDLL  = hModSpecialK;


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


  config.system.injector = injector_name;

  if (! UNX_LoadConfig ())
  {
    // Save a new config if none exists
    UNX_SaveConfig ();
  }

  // This is not meant to be loaded from the INI; it is _STORED_ there
  //   for debug purposes.
  config.system.injector = injector_name;


  SKX_SetPluginName = 
    (SKX_SetPluginName_pfn)
      GetProcAddress (hInjectorDLL, "SKX_SetPluginName");

  SK_GetCommandProcessor =
    (SK_GetCommandProcessor_pfn)
      GetProcAddress (hInjectorDLL, "SK_GetCommandProcessor");

  //
  // If this is NULL, the injector system isn't working right!!!
  //
  if (SKX_SetPluginName != nullptr)
    SKX_SetPluginName (plugin_name.c_str ());


  // Plugin State
  if (UNX_Init_MinHook () == MH_OK)
  {
    UNX_CreateDLLHook2 ( config.system.injector.c_str (),
                         "SK_PlugIn_ControlPanelWidget",
                                UNX_ControlPanelWidget,
               (LPVOID *)&SK_PlugIn_ControlPanelWidget_Original );

    // Initialize memory addresses
    UNX_Scan ((const uint8_t *)"XYZ123", strlen ("XYZ123"), nullptr);

    unx::LanguageManager::Init ();
    unx::DisplayFix::Init      ();
    unx::InputManager::Init    ();
    unx::WindowManager::Init   ();

    if (MH_OK == UNX_ApplyQueuedHooks ())
      return TRUE;
  }

  return FALSE;
}


__declspec (dllexport)
BOOL
WINAPI
SKPlugIn_Init (HMODULE hModSpecialK)
{
                                  wchar_t wszSKFileName [MAX_PATH + 2] = { };
         GetModuleFileName (hModSpecialK, wszSKFileName, MAX_PATH);
  return DeferredDllMain   (hModSpecialK, wszSKFileName);
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
                       hDLLMod = hModule;
      DisableThreadLibraryCalls (hModule);
    } break;


    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
      break;


    case DLL_PROCESS_DETACH:
    {
      if (dll_log != nullptr)
      {
        unx::LanguageManager::Shutdown ();
        unx::WindowManager::Shutdown   ();
        unx::InputManager::Shutdown    ();
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
    } break;
  }


  return TRUE;
}