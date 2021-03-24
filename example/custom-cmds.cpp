/*
 * custom-cmds.cpp
 *
 * Copyright (c) 2021 Lix N. Paulian (lix@paulian.net)
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Created on: 4 February 2021 (LNP)
 */

#include <cmsis-plus/rtos/os.h>
#include <cmsis-plus/diag/trace.h>

#include "custom-cmds.h"

using namespace os;
using namespace os::rtos;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

namespace ushell
{

  /**
   * @brief Constructor for the "test" class.
   */
  ush_test::ush_test (void)
  {
    trace::printf ("%s() %p\n", __func__, this);
    info_.command = "test";
    info_.help_text = "This is a test command";
  }

  /**
   * Destructor.
   */
  ush_test::~ush_test ()
  {
    trace::printf ("%s() %p\n", __func__, this);
  }

  int
  ush_test::do_cmd (class ushell* ush, int argc, char* argv[])
  {
    ush->printf ("Got %d arguments:\n", argc);

    for (int i = 0; i < argc; i++)
      {
        ush->printf ("%s\n", argv[i]);
      }

    return ush_ok;
  }

  //----------------------------------------------------------------------------

  /**
   * @brief Constructor for the "memory dump" class.
   */
  ush_mem_dump::ush_mem_dump (void)
  {
    trace::printf ("%s() %p\n", __func__, this);
    info_.command = "dump";
    info_.help_text = "Dump memory content";
  }

  /**
   * Destructor.
   */
  ush_mem_dump::~ush_mem_dump ()
  {
    trace::printf ("%s() %p\n", __func__, this);
  }

  int
  ush_mem_dump::do_cmd (class ushell* ush, int argc, char* argv[])
  {
    unsigned int address;
    int len = default_len;

    if (argc < 2 || !strcasecmp (argv[1], "-h"))
      {
        ush->printf (
            "Usage:\t%s start [size]\n"
            "\tnote: while <start> expects a hex value, <size> is decimal\n",
            argv[0]);
      }
    else
      {
        if (argc >= 2)
          {
            sscanf (argv[1], "%x", &address);
          }
        if (argc > 2)
          {
            sscanf (argv[2], "%d", &len);
          }

        while (len > 0)
          {
            int count;

            if (address % default_bytes)
              {
                count = default_bytes - (address % default_bytes);
              }
            else
              {
                count = (len - default_bytes) < 0 ? len : default_bytes;
              }
            ush->printf ("%06X  ", address);

            // hex dump
            uint8_t* tmp = (uint8_t*) address;
            for (int i = 0; i < count; i++)
              {
                ush->printf ("%02X ", *(uint8_t*) address++);
              }

            // spaces between hex and ascii dumps
            int padding = default_bytes - count + 1;
            for (int i = 0; i < padding; i++)
              {
                ush->printf ("   ");
              }

            // ascii dump
            for (int i = 0; i < count; i++, tmp++)
              {
                ush->printf ("%c", isprint (*tmp) ? *tmp : '.');
              }
            ush->printf ("\n");
            len -= count;
          }
      }

    return ush_ok;
  }

  //----------------------------------------------------------------------------

  ush_test test
    { };

  ush_mem_dump dump
    { };
}

