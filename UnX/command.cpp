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
#define _CRT_SECURE_NO_WARNINGS

#include "command.h"

template <>
str_hash_compare <std::string, std::less <std::string> >::size_type
str_hash_compare <std::string, std::less <std::string> >::hash_string (const std::string& _Keyval) const
{
  const bool case_insensitive = true;

        size_type   __h    = 0;
  const size_type   __len  = _Keyval.size ();
  const value_type* __data = _Keyval.data ();

  for (size_type __i = 0; __i < __len; ++__i) {
    /* Hash Collision Discovered: "r_window_res_x" vs. "r_window_pos_x" */
    //__h = 5 * __h + eTB_CaseAdjust (__data [__i], case_insensitive);

    /* New Hash: sdbm   -  Collision Free (08/04/12) */
    __h = eTB_CaseAdjust (__data [__i], case_insensitive) +
                         (__h << 06)  +  (__h << 16)      -
                          __h;
  }

  return __h;
}


template <>
str_hash_compare <std::string, std::less <std::string> >::size_type
str_hash_compare <std::string, std::less <std::string> >::operator() (const std::string& _Keyval) const
{
  return hash_string (_Keyval);
}

template <>
bool
str_hash_compare <std::string, std::less <std::string> >::operator() (const std::string& _lhs, const std::string& _rhs) const
{
  return hash_string (_lhs) < hash_string (_rhs);
}

class eTB_SourceCmd : public eTB_Command
{
public:
  eTB_SourceCmd (eTB_CommandProcessor* cmd_proc) {
    processor_ = cmd_proc;
  }

  eTB_CommandResult execute (const char* szArgs) {
    /* TODO: Replace with a special tokenizer / parser... */
    FILE* src = fopen (szArgs, "r");

    if (! src) {
      return
        eTB_CommandResult ( "source", szArgs,
                              "Could not open file!",
                                false,
                                  NULL,
                                    this );
    }

    char line [1024];

    static int num_lines = 0;

    while (fgets (line, 1024, src) != NULL) {
      num_lines++;

      /* Remove the newline character... */
      line [strlen (line) - 1] = '\0';

      processor_->ProcessCommandLine (line);

      //printf (" Source Line %d - '%s'\n", num_lines++, line);
    }

    fclose (src);

    return eTB_CommandResult ( "source", szArgs,
                                 "Success",
                                   num_lines,
                                     NULL,
                                       this );
  }

  int getNumArgs (void) {
    return 1;
  }

  int getNumOptionalArgs (void) {
    return 0;
  }

  const char* getHelp (void) {
    return "Load and execute a file containing multiple commands "
           "(such as a config file).";
  }

private:
  eTB_CommandProcessor* processor_;

};

eTB_CommandProcessor::eTB_CommandProcessor (void)
{
  eTB_Command* src = new eTB_SourceCmd (this);

  AddCommand ("source", src);
}

const eTB_Command*
eTB_CommandProcessor::AddCommand (const char* szCommand, eTB_Command* pCommand)
{
  if (szCommand == NULL || strlen (szCommand) < 1)
    return NULL;

  if (pCommand == NULL)
    return NULL;

  /* Command already exists, what should we do?! */
  if (FindCommand (szCommand) != NULL)
    return NULL;

  commands_.insert (eTB_CommandRecord (szCommand, pCommand));

  return pCommand;
}

bool
eTB_CommandProcessor::RemoveCommand (const char* szCommand)
{
  if (FindCommand (szCommand) != NULL) {
    std::unordered_map <std::string, eTB_Command*, str_hash_compare <std::string> >::iterator
      command = commands_.find (szCommand);

    commands_.erase (command);
    return true;
  }

  return false;
}

eTB_Command*
eTB_CommandProcessor::FindCommand (const char* szCommand) const
{
  std::unordered_map <std::string, eTB_Command*, str_hash_compare <std::string> >::const_iterator
    command = commands_.find (szCommand);

  if (command != commands_.end ())
    return (command)->second;

  return NULL;
}



const eTB_Variable*
eTB_CommandProcessor::AddVariable (const char* szVariable, eTB_Variable* pVariable)
{
  if (szVariable == NULL || strlen (szVariable) < 1)
    return NULL;

  if (pVariable == NULL)
    return NULL;

  /* Variable already exists, what should we do?! */
  if (FindVariable (szVariable) != NULL)
    return NULL;

  variables_.insert (eTB_VariableRecord (szVariable, pVariable));

  return pVariable;
}

bool
eTB_CommandProcessor::RemoveVariable (const char* szVariable)
{
  if (FindVariable (szVariable) != NULL) {
    std::unordered_map <std::string, eTB_Variable*, str_hash_compare <std::string> >::iterator
      variable = variables_.find (szVariable);

    variables_.erase (variable);
    return true;
  }

  return false;
}

const eTB_Variable*
eTB_CommandProcessor::FindVariable (const char* szVariable) const
{
  std::unordered_map <std::string, eTB_Variable*, str_hash_compare <std::string> >::const_iterator
    variable = variables_.find (szVariable);

  if (variable != variables_.end ())
    return (variable)->second;

  return NULL;
}



eTB_CommandResult
eTB_CommandProcessor::ProcessCommandLine (const char* szCommandLine)
{
  if (szCommandLine != NULL && strlen (szCommandLine))
  {
    char*  command_word     = _strdup (szCommandLine);
    size_t command_word_len = strlen  (command_word);

    char*  command_args     = command_word;
    size_t command_args_len = 0;

    /* Terminate the command word on the first space... */
    for (size_t i = 0; i < command_word_len; i++) {
      if (command_word [i] == ' ') {
        command_word [i] = '\0';

        if (i < (command_word_len - 1)) {
          command_args     = &command_word [i + 1];
          command_args_len = strlen (command_args);

          /* Eliminate trailing spaces */
          for (unsigned int j = 0; j < command_args_len; j++) {
            if (command_word [i + j + 1] != ' ') {
              command_args = &command_word [i + j + 1];
              break;
            }
          }

          command_args_len = strlen (command_args);
        }

        break;
      }
    }

    //char* lowercase_cmd_word = strdup (command_word);
    //for (int i = strlen (lowercase_cmd_word) - 1; i >= 0; i--)
      //lowercase_cmd_word [i] = __ascii_tolower (lowercase_cmd_word [i]);

    std::string cmd_word       (command_word);
    //std::string cmd_word_lower (lowercase_cmd_word);
    std::string cmd_args       (command_args_len > 0 ? command_args : "");
    /* ^^^ cmd_args is what is passed back to the object that issued
             this command... If no arguments were passed, it MUST be
               an empty string. */

    //free (lowercase_cmd_word);
    free (command_word);

    eTB_Command* cmd = SK_GetCommandProcessor ()->FindCommand (cmd_word.c_str ());

    if (cmd != NULL) {
      return cmd->execute (cmd_args.c_str ());
    }

    /* No command found, perhaps the word was a variable? */

    const eTB_Variable* var = SK_GetCommandProcessor ()->FindVariable (cmd_word.c_str ());

    if (var != NULL) {
      if (var->getType () == eTB_Variable::Boolean)
      {
        if (command_args_len > 0) {
          eTB_VarStub <bool>* bool_var = (eTB_VarStub <bool>*) var;
          bool                bool_val = false;

          /* False */
          if (! (_stricmp (cmd_args.c_str (), "false") && _stricmp (cmd_args.c_str (), "0") &&
                 _stricmp (cmd_args.c_str (), "off"))) {
            bool_val = false;
            bool_var->setValue (bool_val);
          }

          /* True */
          else if (! (_stricmp (cmd_args.c_str (), "true") && _stricmp (cmd_args.c_str (), "1") &&
                      _stricmp (cmd_args.c_str (), "on"))) {
            bool_val = true;
            bool_var->setValue (bool_val);
          }

          /* Toggle */
          else if (! (_stricmp (cmd_args.c_str (), "toggle") && _stricmp (cmd_args.c_str (), "~") &&
                      _stricmp (cmd_args.c_str (), "!"))) {
            bool_val = ! bool_var->getValue ();
            bool_var->setValue (bool_val);

            /* ^^^ TODO: Consider adding a toggle (...) function to
                           the bool specialization of eTB_VarStub... */
          } else {
            // Unknown Trailing Characters
          }
        }
      }

      else if (var->getType () == eTB_Variable::Int)
      {
        if (command_args_len > 0) {
          int original_val = ((eTB_VarStub <int>*) var)->getValue ();
          int int_val = 0;

          /* Increment */
          if (! (_stricmp (cmd_args.c_str (), "++") && _stricmp (cmd_args.c_str (), "inc") &&
                 _stricmp (cmd_args.c_str (), "next"))) {
            int_val = original_val + 1;
          } else if (! (_stricmp (cmd_args.c_str (), "--") && _stricmp (cmd_args.c_str (), "dec") &&
                        _stricmp (cmd_args.c_str (), "prev"))) {
            int_val = original_val - 1;
          } else
            int_val = atoi (cmd_args.c_str ());

          ((eTB_VarStub <int>*) var)->setValue (int_val);
        }
      }

      else if (var->getType () == eTB_Variable::Short)
      {
        if (command_args_len > 0) {
          short original_val = ((eTB_VarStub <short>*) var)->getValue ();
          short short_val    = 0;

          /* Increment */
          if (! (_stricmp (cmd_args.c_str (), "++") && _stricmp (cmd_args.c_str (), "inc") &&
                 _stricmp (cmd_args.c_str (), "next"))) {
            short_val = original_val + 1;
          } else if (! (_stricmp (cmd_args.c_str (), "--") && _stricmp (cmd_args.c_str (), "dec") &&
                        _stricmp (cmd_args.c_str (), "prev"))) {
            short_val = original_val - 1;
          } else
            short_val = (short)atoi (cmd_args.c_str ());

          ((eTB_VarStub <short>*) var)->setValue (short_val);
        }
      }

      else if (var->getType () == eTB_Variable::Float)
      {
        if (command_args_len > 0) {
//          float original_val = ((eTB_VarStub <float>*) var)->getValue ();
          float float_val = (float)atof (cmd_args.c_str ());

          ((eTB_VarStub <float>*) var)->setValue (float_val);
        }
      }

      return eTB_CommandResult (cmd_word, cmd_args, var->getValueString (), true, var, NULL);
    } else {
      /* Default args --> failure... */
      return eTB_CommandResult (cmd_word, cmd_args); 
    }
  } else {
    /* Invalid Command Line (not long enough). */
    return eTB_CommandResult (szCommandLine); /* Default args --> failure... */
  }
}

#include <cstdarg>

eTB_CommandResult
eTB_CommandProcessor::ProcessCommandFormatted (const char* szCommandFormat, ...)
{
  va_list ap;
  int     len;

  va_start         (ap, szCommandFormat);
  len = _vscprintf (szCommandFormat, ap);
  va_end           (ap);

  char* szFormattedCommandLine =
    (char *)malloc (sizeof (char) * (len + 1));

  *(szFormattedCommandLine + len) = '\0';

  va_start (ap, szCommandFormat);
  vsprintf (szFormattedCommandLine, szCommandFormat, ap);
  va_end   (ap);

  eTB_CommandResult result =
    ProcessCommandLine (szFormattedCommandLine);

  free (szFormattedCommandLine);

  return result;
}

/** Variable Type Support **/


template <>
eTB_VarStub <bool>::eTB_VarStub ( bool*                  var,
                                  eTB_iVariableListener* pListener ) :
                                          var_ (var)
{
  listener_ = pListener;
  type_     = Boolean;
}

template <>
std::string
eTB_VarStub <bool>::getValueString (void) const
{
  if (getValue ())
    return std::string ("true");
  else
    return std::string ("false");
}

template <>
eTB_VarStub <const char*>::eTB_VarStub ( const char**           var,
                                         eTB_iVariableListener* pListener ) :
                                                 var_ (var)
{
  listener_ = pListener;
  type_     = String;
}

template <>
eTB_VarStub <int>::eTB_VarStub ( int*                  var,
                                 eTB_iVariableListener* pListener ) :
                                         var_ (var)
{
  listener_ = pListener;
  type_     = Int;
}

template <>
std::string
eTB_VarStub <int>::getValueString (void) const
{
  char szIntString [32];
  snprintf (szIntString, 32, "%d", getValue ());

  return std::string (szIntString);
}


template <>
eTB_VarStub <short>::eTB_VarStub ( short*                 var,
                                   eTB_iVariableListener* pListener ) :
                                         var_ (var)
{
  listener_ = pListener;
  type_     = Short;
}

template <>
std::string
eTB_VarStub <short>::getValueString (void) const
{
  char szShortString [32];
  snprintf (szShortString, 32, "%d", getValue ());

  return std::string (szShortString);
}


template <>
eTB_VarStub <float>::eTB_VarStub ( float*                 var,
                                   eTB_iVariableListener* pListener ) :
                                         var_ (var)
{
  listener_ = pListener;
  type_     = Float;
}

template <>
std::string
eTB_VarStub <float>::getValueString (void) const
{
  char szFloatString [32];
  snprintf (szFloatString, 32, "%f", getValue ());

  // Remove trailing 0's after the .
  size_t len = strlen (szFloatString);
  for (size_t i = (len - 1); i > 1; i--) {
    if (szFloatString [i] == '0' && szFloatString [i - 1] != '.')
      len--;
    if (szFloatString [i] != '0' && szFloatString [i] != '\0')
      break;
  }

  szFloatString [len] = '\0';

  return std::string (szFloatString);
}


#include <Windows.h>

//
// In case the injector's command processor is not workable
// t
eTB_CommandProcessor*
__stdcall
FALLBACK_GetCommandProcessor (void)
{
  static eTB_CommandProcessor* command = nullptr;

  if (command == nullptr) {
    //
    // The ABI is stable, so we don't need this hack
    //
    //command = new eTB_CommandProcessor ();

    SK_GetCommandProcessor_pfn SK_GetCommandProcessorFromDLL =
      (SK_GetCommandProcessor_pfn)
        GetProcAddress (LoadLibrary (L"dxgi.dll"), "SK_GetCommandProcessor");

    command = SK_GetCommandProcessorFromDLL ();
  }

  return command;
}

// This _should_ be overwritten with Special K's DLL import
SK_GetCommandProcessor_pfn SK_GetCommandProcessor = &FALLBACK_GetCommandProcessor;

////
//// TODO: Eliminate the dual-definition of this class, and add SpecialK as a compile-time
////         dependency. That project will need some additional re-design for this
////          to happen, but it is an important step for modularity.
////