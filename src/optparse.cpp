/*
 * optparse.cpp
 *
 * A C++ equivalent of the Unix getopt() call.
 *
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 *
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * For more information, please refer to <http://unlicense.org/>
 *
 * This file, as well as the associated header file is based on work done
 * by Christopher Wellons <skeeto>. The original UNLICENSE also applies to
 * this code. See also <https://github.com/skeeto/optparse>.
 *
 * Following changes/additions were made to the original code:
 *
 * - Wrapped the code into a C++ class.
 *
 * - Fixed an issue that would crash the program if the list of arguments does
 * not end with a null argument.
 *
 * - Improved the handling of options with optional arguments (defined with
 * double semicolons, e.g. "d::"): now it is possible to specify them with, or
 * without a space in between, e.g. -d3 or -d 3.
 *
 * The API is similar to the original version, however the parser must be
 * instantiated before use. After the opt_parse object, all known functions
 * and variables becomes accessible:
 *
 *      opt_parse getopt { argc, argv };
 *      int ch;
 *
 *      while ((ch = getopt.optparse ("c::thu")) != -1)
 *      {
 *        switch (ch)
 *          {
 *            case 'c':
 *            if (getopt.optarg)
 *              {
 *                // use the value pointed by optarg
 *              }
 *            else
 *              {
 *                // no parameter for the -c option
 *              }
 *            break;
 *
 *            case 't':
 *              // handle the -t option
 *            break;
 *
 *            case 'h':
 *              // handle the -h option
 *            break;
 *
 *            case 'u':
 *              // handle the -u option
 *            break;
 *
 *            case '?':
 *              // invalid option
 *            printf ("%s\n", getopt.errmsg);
 *            break;
 *          }
 *      }
 *
 * This software is primarily intended for embedded systems where memory
 * is at a premium.
 *
 * Lix N. Paulian (lix@paulian.net), 6 February 2021
 */

#include "optparse.h"

namespace ushell
{

  opt_parse::opt_parse (int argc, char** argv) :
      argc
        { argc }, //
      argv
        { argv }
  {
    permute = true;
    optind = 1;
    subopt = 0;
    optarg = nullptr;
    optopt = 0;
    errmsg[0] = '\0';
  }

  opt_parse::~opt_parse ()
  {
  }

  int
  opt_parse::optparse (const char* optstring)
  {
    int type;
    char* next;
    char* option = argv[optind];
    errmsg[0] = '\0';
    optopt = 0;
    optarg = nullptr;

    if (option == nullptr || argc < 2)
      {
        return -1;
      }
    else if (optparse_is_dashdash (option))
      {
        optind++; /* consume "--" */
        argc--;
        return -1;
      }
    else if (!optparse_is_shortopt (option))
      {
        if (permute)
          {
            int index = optind++;
            argc--;
            int r = optparse (optstring);
            optparse_permute (index);
            optind--;
            argc++;
            return r;
          }
        else
          {
            return -1;
          }
      }

    option += subopt + 1;
    optopt = option[0];
    type = optparse_argtype (optstring, option[0]);
    next = argv[optind + 1];
    switch (type)
      {
      case -1:
        {
          char str[2] =
            { 0, 0 };
          str[0] = option[0];
          optind++;
          argc--;
          return optparse_error (msg_invalid, str);
        }

      case optparse_none:
        if (option[1])
          {
            subopt++;
          }
        else
          {
            subopt = 0;
            optind++;
            argc--;
          }
        return option[0];

      case optparse_required:
        subopt = 0;
        optind++;
        argc--;
        if (option[1])
          {
            optarg = option + 1;
          }
        else if (next != 0)
          {
            optarg = next;
            optind++;
            argc--;
          }
        else
          {
            char str[2] =
              { 0, 0 };
            str[0] = option[0];
            optarg = nullptr;
            return optparse_error (msg_missing, str);
          }
        return option[0];

      case optparse_optional:
        subopt = 0;
        optind++;
        argc--;
        if (option[1])
          {
            // optional argument present, no spaces between option
            optarg = option + 1;
          }
        else
          {
            if (optparse (optstring) < 0 && argc > 1)
              {
                // optional argument present, at least a space between option
                optarg = argv[optind];
                optind++;
                argc--;
              }
            else
              {
                // optional argument is missing
                optarg = nullptr;
              }
          }
        return option[0];
      }
    return 0;
  }

  char*
  opt_parse::optparse_arg (void)
  {
    char* option = argv[optind];
    subopt = 0;
    if (option != 0)
      {
        optind++;
        argc--;
      }
    return option;
  }

  int
  opt_parse::optparse_long (const optparse_long_t* longopts, int* longindex)
  {
    int i;
    char* option = argv[optind];

    if (option == nullptr)
      {
        return -1;
      }
    else if (optparse_is_dashdash (option))
      {
        optind++; /* consume "--" */
        argc--;
        return -1;
      }
    else if (optparse_is_shortopt (option))
      {
        return optparse_long_fallback (longopts, longindex);
      }
    else if (!optparse_is_longopt (option))
      {
        if (permute)
          {
            int index = optind++;
            argc--;
            int r = optparse_long (longopts, longindex);
            optparse_permute (index);
            optind--;
            argc++;
            return r;
          }
        else
          {
            return -1;
          }
      }

    /* Parse as long option. */
    errmsg[0] = '\0';
    optopt = 0;
    optarg = nullptr;
    option += 2; /* skip "--" */
    optind++;
    argc--;
    for (i = 0; !optparse_longopts_end (longopts, i); i++)
      {
        const char* name = longopts[i].longname;
        if (optparse_longopts_match (name, option))
          {
            char* arg;
            if (longindex)
              {
                *longindex = i;
              }
            optopt = longopts[i].shortname;
            arg = optparse_longopts_arg (option);
            if (longopts[i].argtype == optparse_none && arg != 0)
              {
                return optparse_error (msg_too_many, name);
              }
            if (arg != 0)
              {
                optarg = arg;
              }
            else if (longopts[i].argtype == optparse_required)
              {
                optarg = argv[optind];
                if (optarg == nullptr)
                  {
                    return optparse_error (msg_missing, name);
                  }
                else
                  {
                    optind++;
                    argc--;
                  }
              }
            return optopt;
          }
      }
    return optparse_error (msg_invalid, option);
  }

  //----------------------------------------------------------------------------

  int
  opt_parse::optparse_error (const char* msg, const char* data)
  {
    unsigned p = 0;
    const char* sep = " -- '";

    while (*msg)
      {
        errmsg[p++] = *msg++;
      }
    while (*sep)
      {
        errmsg[p++] = *sep++;
      }
    while (p < sizeof(errmsg) - 2 && *data)
      {
        errmsg[p++] = *data++;
      }
    errmsg[p++] = '\'';
    errmsg[p++] = '\0';
    return '?';
  }

  bool
  opt_parse::optparse_is_dashdash (const char* arg)
  {
    return argc > 1 && arg != nullptr && arg[0] == '-' && arg[1] == '-'
        && arg[2] == '\0';
  }

  bool
  opt_parse::optparse_is_shortopt (const char* arg)
  {
    return argc > 1 && arg != nullptr && arg[0] == '-' && arg[1] != '-'
        && arg[1] != '\0';
  }

  bool
  opt_parse::optparse_is_longopt (const char* arg)
  {
    return argc > 1 && arg != nullptr && arg[0] == '-' && arg[1] == '-'
        && arg[2] != '\0';
  }

  void
  opt_parse::optparse_permute (int index)
  {
    char* nonoption = argv[index];
    int i;
    for (i = index; i < optind - 1; i++)
      {
        argv[i] = argv[i + 1];
      }
    argv[optind - 1] = nonoption;
  }

  int
  opt_parse::optparse_argtype (const char* optstring, char c)
  {
    int count = optparse_none;

    if (c == ':')
      {
        return -1;
      }
    for (; *optstring && c != *optstring; optstring++)
      {
        ;
      }
    if (!*optstring)
      {
        return -1;
      }
    if (optstring[1] == ':')
      {
        count += optstring[2] == ':' ? 2 : 1;
      }
    return count;
  }

  bool
  opt_parse::optparse_longopts_end (const optparse_long_t* longopts, int i)
  {
    return !longopts[i].longname && !longopts[i].shortname;
  }

  void
  opt_parse::optparse_from_long (const optparse_long_t* longopts,
                                 char* optstring)
  {
    char* p = optstring;

    for (int i = 0; !optparse_longopts_end (longopts, i); i++)
      {
        if (longopts[i].shortname && longopts[i].shortname < 127)
          {
            int a;
            *p++ = longopts[i].shortname;
            for (a = 0; a < (int) longopts[i].argtype; a++)
              {
                *p++ = ':';
              }
          }
      }
    *p = '\0';
  }

  /* Unlike strcmp(), handles options containing "=". */
  bool
  opt_parse::optparse_longopts_match (const char* longname, const char* option)
  {
    const char* a = option, * n = longname;

    if (longname == nullptr)
      {
        return false;
      }
    for (; *a && *n && *a != '='; a++, n++)
      {
        if (*a != *n)
          {
            return false;
          }
      }
    return *n == '\0' && (*a == '\0' || *a == '=');
  }

  /* Return the part after "=", or nullptr. */
  char*
  opt_parse::optparse_longopts_arg (char* option)
  {
    char* p = nullptr;

    for (; *option && *option != '='; option++)
      {
        ;
      }
    if (*option == '=')
      {
        p = option + 1;
      }
    return p;
  }

  int
  opt_parse::optparse_long_fallback (const optparse_long_t* longopts,
                                     int* longindex)
  {
    int result;
    char optstring[96 * 3 + 1]; /* 96 ASCII printable characters */

    optparse_from_long (longopts, optstring);
    result = optparse (optstring);
    if (longindex != 0)
      {
        *longindex = -1;
        if (result != -1)
          {
            for (int i = 0; !optparse_longopts_end (longopts, i); i++)
              {
                if (longopts[i].shortname == optopt)
                  {
                    *longindex = i;
                  }
              }
          }
      }
    return result;
  }

}
