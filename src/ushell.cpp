/*
 * ushell.cpp
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
 * Created on: 23 Nov 2020 (LNP)
 */

#include <cmsis-plus/rtos/os.h>
#include <cmsis-plus/diag/trace.h>
#include <cmsis-plus/posix-io/io.h>
#include <stdarg.h>

#include "ushell.h"

using namespace os;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"

namespace ushell
{
  ushell::ushell (const char* char_device) :
      char_device_
        { char_device }
  {
    trace::printf ("%s() %p\n", __func__, this);
  }

  ushell::~ushell ()
  {
    trace::printf ("%s() %p\n", __func__, this);
  }

  void*
  ushell::ushell_th (void* args)
  {
    int c;
    char buffer[256];
    char prompt[] =
      { ": " };

    tty = static_cast<posix::tty_canonical*> (posix::open (char_device_, 0));

    if (tty != nullptr)
      {
        struct termios tio_orig, tio;

        if (!((tty->tcgetattr (&tio)) < 0))
          {
            // configure the tty in canonical mode
            memcpy (&tio_orig, &tio, sizeof(struct termios));
            tio.c_lflag |= (ICANON | ECHO | ECHOE);
            tio.c_iflag |= (ICRNL | IMAXBEL);
            tio.c_oflag |= (OPOST | ONLCR);
            tio.c_cc[VEOF] = 4;   // ctrl-d
            tio.c_cc[VERASE] = '\b';
            if (!((tty->tcsetattr (TCSANOW, &tio)) < 0))
              {
                do
                  {
                    tty->write (prompt, strlen (prompt));
                    if ((c = tty->read (buffer, sizeof(buffer))) > 0)
                      {
                        buffer[c] = '\0';
                        bool result = cmd_parser (buffer); // parse and execute command

                        if (result == false)
                          {
                            if (error_type == USH_EXIT)
                              {
                                // exit command, we must leave now
                                break;
                              }
                          }
                      }
                  }
                while (!(c < 0)); // exit on tty error

                // restore the original tty settings
                tty->tcsetattr (TCSANOW, &tio_orig);
              }
            tty->close ();
          }
      }

    return nullptr;
  }

  //----------------------------------------------------------------------------

  int
  ushell::printf (const char* fmt, ...)
  {
    va_list ap;

    va_start(ap, fmt);
    int result = vdprintf (tty->file_descriptor (), fmt, ap);
    va_end(ap);

    return result;
  }

  //----------------------------------------------------------------------------

  bool
  ushell::cmd_parser (char* buff)
  {
    char* pbuff;
//    const cmds_t* pcmd;
//    const cmdstab_t* pcmdtab;
    bool done = false;
    char* argv[max_params];     // pointers on parameters
    int argc;                   // parameter counter
    bool result = false;
    error_type = USH_INVALID_COMMAND;

    // isolate the command
    pbuff = buff;
    while (*pbuff != ' ' && *pbuff != '\0')
      {
        pbuff++;
      }
    *pbuff++ = '\0'; // insert terminator

#if 0
    /* iterate all commands tables, one by one */
    for (pcmdtab = cmdtabs; pcmdtab->cmdstable; pcmdtab++)
      {
        // lookup in the commands table
        for (pcmd = pcmdtab->cmdstable; pcmd->name; pcmd++)
          {
            if (!strcasecmp (buff, pcmd->name))
              {
                // valid command, parse parameters, if any
                for (argc = 0; argc < max_params; argc++)
                  {
                    if (!*pbuff)
                      {
                        break;                  // end of line reached
                      }
                    if (*pbuff == '\"')
                      {
                        pbuff++;                // suck up the "
                        argv[argc] = pbuff;
                        while (*pbuff != '\"' && *pbuff != '\0')
                          {
                            pbuff++;
                          }
                        *pbuff++ = '\0';
                        if (*pbuff)             // if not end of line...
                          {
                            pbuff++;            // suck up the trailing space
                          }
                      }
                    else
                      {
                        argv[argc] = pbuff;
                        while (*pbuff != ' ' && *pbuff != '\0')
                          {
                            pbuff++;
                          }
                        *pbuff++ = '\0';
                      }
                  }
                result = (*pcmd->func) (argc, &argv[0]); // execute command
                done = true;    // all done
                break;
              }
          }
        if (done)               // if all done, break out
          {
            break;
          }
      }
#endif
    return result;
  }

}

#pragma GCC diagnostic pop
