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

#include "ushell.h"

#if !defined SHELL_GREET
#define SHELL_GREET "\nType \"help\" for the list of available commands\n"
#endif

#if !defined SHELL_PROMPT
#define SHELL_PROMPT ": "
#endif

#if !defined SHELL_MAX_CMD_ARGS
#define SHELL_MAX_CMD_ARGS 10
#endif

using namespace os;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

namespace ushell
{

  class ushell_cmd* ushell::ushell_cmds_[SHELL_MAX_COMMANDS];

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
  ushell::do_ushell (void* args)
  {
    int c;
    char buffer[SHELL_MAX_LINE_LEN], * p;
    char greet[] =
      { SHELL_GREET };
    char prompt[] =
      { SHELL_PROMPT };

#if SHELL_FILE_CMDS == true
    ph.set_default ("/flash/");
#endif

    tty = static_cast<posix::tty_canonical*> (posix::open (char_device_, 0));
    if (tty != nullptr)
      {
        struct termios tio_orig, tio;

        rtos::sysclock.sleep_for (rtos::sysclock.frequency_hz * 2);

        if (!((tty->tcgetattr (&tio)) < 0))
          {
            // configure the tty in canonical mode
            memcpy (&tio_orig, &tio, sizeof(struct termios));

#if SHELL_USE_READLINE == true
            tio.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
            tio.c_oflag |= (OPOST | ONLCR);
            tio.c_cflag |= CS8;
            tio.c_lflag &= ~(ECHO | ICANON | IEXTEN);
            tio.c_cc[VMIN] = 1;
            tio.c_cc[VTIME] = 0;
            rl.init (tty, nullptr);
#else
            tio.c_lflag |= (ICANON | ECHO | ECHOE);
            tio.c_iflag |= (ICRNL | IMAXBEL);
            tio.c_oflag |= (OPOST | ONLCR);
            tio.c_cc[VEOF] = 4;   // ctrl-d
            tio.c_cc[VERASE] = '\b';
#endif

            if (!((tty->tcsetattr (TCSANOW, &tio)) < 0))
              {
                tty->write (greet, strlen (greet));

                do
                  {
#if SHELL_USE_READLINE == true
                    if ((c = rl.readline (prompt, buffer, sizeof(buffer))) > 0)
#else
                    tty->write (prompt, strlen (prompt));
                    if ((c = tty->read (buffer, sizeof(buffer))) > 0)
#endif
                      {
                        p = buffer;
                        for (int n = 0; n < c; n++, p++)
                          {
                            if (*p == '\r' || *p == '\n' || *p == '\0')
                              {
                                break;
                              }
                          }

                        // clear buffer to end
                        memset (p, 0, sizeof(buffer) - c);

                        // parse and execute command
                        int result = cmd_parser (buffer);
                        if (result == ush_exit)
                          {
                            // exit command, we must leave now
                            break;
                          }
                        else if (result != ush_ok)
                          {
                            printf ("error %d\n", result);
                          }
                      }
                  }
                while (!(c < 0)); // exit on tty error

                // restore the original tty settings
                tty->tcsetattr (TCSANOW, &tio_orig);
#if SHELL_USE_READLINE == true
                rl.end ();
#endif
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

  int
  ushell::cmd_parser (char* buff)
  {
    class ushell_cmd** pclasses;
    char* pbuff;
    char* argv[SHELL_MAX_CMD_ARGS];
    int result = ush_ok;

    if (*buff != '\0') // ignore empty strings, at least one character is expected
      {
        pbuff = buff;

        // extract the command
        while (*pbuff != ' ' && *pbuff != '\0')
          {
            pbuff++;
          }
        *pbuff++ = '\0'; // add terminator
        argv[0] = buff; // first argument is the command itself

        // iterate all linked command classes
        result = ush_cmd_not_found;
        for (pclasses = ushell_cmds_; *pclasses != nullptr; pclasses++)
          {
            if (!strcasecmp ((*pclasses)->get_cmd_info ()->command, buff))
              {
                int argc;

                // valid command, parse parameters, if any
                for (argc = 1; argc < SHELL_MAX_CMD_ARGS; argc++)
                  {
                    if (*pbuff == '\0')
                      {
                        break;          // end of line reached
                      }
                    if (*pbuff == '\"')
                      {
                        pbuff++;        // skip the "
                        argv[argc] = pbuff;
                        while (*pbuff != '\"' && *pbuff != '\0')
                          {
                            pbuff++;
                          }
                        *pbuff++ = '\0';
                        if (*pbuff != '\0')  // if not end of line...
                          {
                            pbuff++;    // skip the trailing space
                          }
                      }
                    else
                      {
                        while (*pbuff == ' ' || *pbuff == '\t')
                          {
                            pbuff++;    // skip leading white spaces, if any
                          }
                        if (*pbuff == '\0')
                          {
                            break;      // end of line, exit
                          }
                        argv[argc] = pbuff;
                        while (*pbuff != ' ' && *pbuff != '\0')
                          {
                            pbuff++;
                          }
                        *pbuff++ = '\0';
                      }
                  }
                argv[argc] = nullptr;
                result = (*pclasses)->do_cmd (this, argc, argv);
                break;
              }
          }
      }

    return result;
  }

  bool
  ushell::link_cmd (class ushell_cmd* ucmd)
  {
    bool result = false;
    int i;

    for (i = 0; i < SHELL_MAX_COMMANDS; i++)
      {
        if (ushell_cmds_[i] == nullptr)
          {
            ushell_cmds_[i] = ucmd;
            result = true;
            break;
          }
      }

    // check if the table overflowed
    assert(i < SHELL_MAX_COMMANDS);

    return result;
  }

  //----------------------------------------------------------------------------

  ushell_cmd::ushell_cmd (void)
  {
    trace::printf ("%s() %p\n", __func__, this);
    ushell::link_cmd (this);
  }

  ushell_cmd::~ushell_cmd ()
  {
    trace::printf ("%s() %p\n", __func__, this);
  }

  ushell_cmd::cmd_info_t*
  ushell_cmd::get_cmd_info (void)
  {
    return &info_;
  }

}

#pragma GCC diagnostic pop
