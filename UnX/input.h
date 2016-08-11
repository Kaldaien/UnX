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
#ifndef __UNX__INPUT_H__
#define __UNX__INPUT_H__

#include "command.h"

namespace unx
{
  namespace InputManager
  {
    void Init     ();
    void Shutdown ();

    class Hooker {
    private:
      HANDLE          hMsgPump;

      static Hooker*  pInputHook;
    protected:
      Hooker (void) { }

    public:
      static Hooker* getInstance (void)
      {
        if (pInputHook == nullptr)
          pInputHook = new Hooker ();

        return pInputHook;
      }

      void Start (void);
      void End   (void);

      HANDLE GetThread (void) {
        return hMsgPump;
      }

      static unsigned int
        __stdcall
        MessagePump (LPVOID hook_ptr);
    };
  }
}

#endif /* __UNX__INPUT_H__ */