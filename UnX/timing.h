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

namespace unx {
  namespace TimingFix {
    void Init     (void);
    void Shutdown (void);
  }
}

typedef BOOL (WINAPI *QueryPerformanceCounter_pfn)(
  _Out_ LARGE_INTEGER *lpPerformanceCount
);

typedef void (WINAPI *Sleep_pfn)(
  _In_  DWORD          dwMilliseconds
);


extern QueryPerformanceCounter_pfn QueryPerformanceCounter_Original;
extern Sleep_pfn                   SK_Sleep;