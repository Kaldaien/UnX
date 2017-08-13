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
#ifndef __UNX__LANGUAGE_H__
#define __UNX__LANGUAGE_H__

#include "command.h"

enum asset_type_t {
  Voice       = 0x1,
  SoundEffect = 0x2,
  Video       = 0x4,
  Any         = 0xf
};

namespace unx
{
  namespace LanguageManager
  {
    void Init     ();
    void Shutdown ();

    bool ApplyPatch (asset_type_t type = Any);
  }
}

#endif /* __UNX__LANGUAGE_H__ */