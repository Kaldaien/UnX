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
#include "command.h"

SK_GetCommandProcessor_pfn SK_GetCommandProcessor = nullptr;

typedef SK_IVariable* (__stdcall *SK_CreateVar_pfn)( SK_IVariable::VariableType  type,
                                                     void*                       var,
                                                     SK_IVariableListener       *pListener );
SK_CreateVar_pfn SK_CreateVar = nullptr;

SK_IVariable*
UNX_CreateVar ( SK_IVariable::VariableType  type,
                void*                       var,
                SK_IVariableListener       *pListener )
{
  extern HMODULE hInjectorDLL;

  if (SK_CreateVar == nullptr) {
    SK_CreateVar =
      (SK_CreateVar_pfn)GetProcAddress (hInjectorDLL, "SK_CreateVar");
  }

  return SK_CreateVar (type, var, pListener);
}