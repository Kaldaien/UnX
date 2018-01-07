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
#define NOMINMAX

#include <Windows.h>

#include "config.h"

#include "hook.h"
#include "log.h"

#include <algorithm>

LPVOID __UNX_base_img_addr = nullptr;
LPVOID __UNX_end_img_addr  = nullptr;

void*
__stdcall
UNX_Scan (const void* pattern, size_t len, const void* mask)
{
  return UNX_ScanAligned (pattern, len, mask);
}

void*
__stdcall
UNX_ScanAlignedEx (const void* pattern, size_t len, const void* mask, void* after, int align)
{
  uint8_t* base_addr =
    reinterpret_cast <uint8_t *> (GetModuleHandle (nullptr));

  MEMORY_BASIC_INFORMATION minfo;
  VirtualQuery (base_addr, &minfo, sizeof minfo);

           base_addr = static_cast <uint8_t *> (minfo.BaseAddress);//AllocationBase;
  uint8_t* end_addr  = static_cast <uint8_t *> (minfo.BaseAddress) + minfo.RegionSize;

  size_t pages = 0;

  // Account for possible overflow in 32-bit address space in very rare (address randomization) cases
  uint8_t* const PAGE_WALK_LIMIT = 
    base_addr + static_cast <uintptr_t>(1UL << 27) > base_addr ?
                                                     base_addr + static_cast      <uintptr_t>( 1UL << 27) :
                                                                 reinterpret_cast <uint8_t *>(~0UL      );

  //
  // For practical purposes, let's just assume that all valid games have less than 256 MiB of
  //   committed executable image data.
  //
  while (VirtualQuery (end_addr, &minfo, sizeof minfo) && end_addr < PAGE_WALK_LIMIT)
  {
    if (minfo.Protect & PAGE_NOACCESS || (! (minfo.Type & MEM_IMAGE)))
      break;

    pages += VirtualQuery (end_addr, &minfo, sizeof minfo);

    end_addr =
      static_cast <uint8_t *> (minfo.BaseAddress) + minfo.RegionSize;
  } 

  if (end_addr > PAGE_WALK_LIMIT)
  {
    static bool warned = false;

    if (! warned)
    {
      dll_log->Log ( L"[ Sig Scan ] Module page walk resulted in end addr. out-of-range: %ph",
                      end_addr );
      dll_log->Log ( L"[ Sig Scan ]  >> Restricting to %ph",
                      PAGE_WALK_LIMIT );
      warned = true;
    }

    end_addr =
      static_cast <uint8_t *> (PAGE_WALK_LIMIT);
  }

  __UNX_base_img_addr = base_addr;
  __UNX_end_img_addr  = end_addr;

  uint8_t* begin = std::max (static_cast <uint8_t *> (after) + align, base_addr);
  uint8_t* it    = begin;
  size_t   idx   = 0;

  while (it < end_addr)
  {
    VirtualQuery (it, &minfo, sizeof minfo);

    // Bail-out once we walk into an address range that is not resident, because
    //   it does not belong to the original executable.
    if (minfo.RegionSize == 0)
      break;

    uint8_t* next_rgn =
     (uint8_t *)minfo.BaseAddress + minfo.RegionSize;

    if ( (! (minfo.Type    & MEM_IMAGE))  ||
         (! (minfo.State   & MEM_COMMIT)) ||
             minfo.Protect & PAGE_NOACCESS )
    {
      it    = next_rgn;
      idx   = 0;
      begin = it;

      continue;
    }

    // Do not search past the end of the module image!
    if (next_rgn >= end_addr)
      break;

    while (it < next_rgn)
    {
      uint8_t* scan_addr = it;

      bool match = (*scan_addr == static_cast <const uint8_t *> (pattern) [idx]);

      // For portions we do not care about... treat them
      //   as matching.
      if (mask != nullptr && (! static_cast <const uint8_t *> (mask) [idx]))
        match = true;

      if (match)
      {
        if (++idx == len)
        {
          if ((reinterpret_cast <uintptr_t> (begin) % align) == 0)
          {
            return
              static_cast <void *> (begin);
          }

          else
          {
            begin += idx;
            begin += align - (reinterpret_cast <uintptr_t> (begin) % align);

            it     = begin;
            idx    = 0;
          }
        }

        else
          ++it;
      }

      else
      {
        // No match?!
        if (it > end_addr - len)
          break;

        begin += idx;
        begin += align - (reinterpret_cast <uintptr_t> (begin) % align);

        it  = begin;
        idx = 0;
      }
    }
  }

  return nullptr;
}

void*
__stdcall
UNX_ScanAligned (const void* pattern, size_t len, const void* mask, int align)
{
  return UNX_ScanAlignedEx (pattern, len, mask, nullptr, align);
}

void
UNX_FlushInstructionCache ( LPCVOID base_addr,
                            size_t  code_size )
{
  FlushInstructionCache ( GetCurrentProcess (),
                            base_addr,
                              code_size );
}

void
UNX_InjectMachineCode ( LPVOID  base_addr,
                        void   *new_code,
                        size_t  code_size,
                        DWORD   permissions,
                        void   *old_code = nullptr )
{
  __try {
    DWORD dwOld =
      PAGE_NOACCESS;

    if ( VirtualProtect ( base_addr,   code_size,
                          permissions, &dwOld )   )
    {
      if (old_code != nullptr) memcpy (old_code, base_addr, code_size);
                               memcpy (base_addr, new_code, code_size);

      VirtualProtect ( base_addr, code_size,
                       dwOld,     &dwOld );

      return;
    }
  }

  __except ( ( GetExceptionCode () == EXCEPTION_ACCESS_VIOLATION ) ? 
               EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH )
  {
    //assert (false);

    // Bad memory address, just discard the write attempt
    //
    //   This isn't atomic; if we fail, it's possible we wrote part
    //     of the data successfully - consider an undo mechanism.
    //
  }

  UNX_FlushInstructionCache (base_addr, code_size);
}