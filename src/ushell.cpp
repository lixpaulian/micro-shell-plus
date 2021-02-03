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

  class ushell_cmd* ushell::ushell_cmds_[USH_MAX_COMMANDS];

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
    char buffer[256];
    char greet[] =
      { "\nType \"help\" for the list of available commands\n" };
    char prompt[] =
      { ": " };

    tty = static_cast<posix::tty_canonical*> (posix::open (char_device_, 0));
    if (tty != nullptr)
      {
        struct termios tio_orig, tio;

        rtos::sysclock.sleep_for (rtos::sysclock.frequency_hz * 2);

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
                tty->write (greet, strlen (greet));
                do
                  {
                    tty->write (prompt, strlen (prompt));
                    if ((c = tty->read (buffer, sizeof(buffer))) > 0)
                      {
                        buffer[c] = '\0';

                        // parse and execute command
                        int result = cmd_parser (buffer, c);
                        if (result == ush_exit)
                          {
                            // exit command, we must leave now
                            break;
                          }
                        else if (result != ush_ok)
                          {
                            printf ("Error %d\n", result);
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

  int
  ushell::cmd_parser (char* buff, ssize_t len)
  {
    class ushell_cmd** pclasses;
    char* pbuff;
    char* argv[max_params];
    int result = ush_ok;

    if (len > 1) // ignore empty strings, at least two chars are expected
      {
        pbuff = buff;

        // extract the command
        while (*pbuff != ' ' && *pbuff != '\0' && *pbuff != '\n' && len--)
          {
            pbuff++;
          }
        *pbuff++ = '\0'; // add terminator
        len--;

        // iterate all linked command classes
        result = ush_cmd_not_found;
        for (pclasses = ushell_cmds_; *pclasses != nullptr; pclasses++)
          {
            if (!strcasecmp ((*pclasses)->get_cmd_info ()->command, buff))
              {
                int argc;

                // valid command, parse parameters, if any
                for (argc = 0; argc < max_params; argc++)
                  {
                    if (!len)
                      {
                        break;          // end of line reached
                      }
                    if (*pbuff == '\"')
                      {
                        pbuff++;        // skip the "
                        len--;
                        argv[argc] = pbuff;
                        while (*pbuff != '\"' && *pbuff != '\0'
                            && *pbuff != '\n' && len--)
                          {
                            pbuff++;
                          }
                        *pbuff++ = '\0';
                        len--;
                        if (len--)      // if not end of line...
                          {
                            pbuff++;    // skip the trailing space
                          }
                      }
                    else
                      {
                        while ((*pbuff == ' ' || *pbuff == '\t'
                            || *pbuff == '\n') && len--)
                          {
                            pbuff++;    // skip possible leading white spaces
                          }
                        if (len <= 0)
                          {
                            break;
                          }
                        argv[argc] = pbuff;
                        while (*pbuff != ' ' && *pbuff != '\0' && *pbuff != '\n'
                            && len--)
                          {
                            pbuff++;
                          }
                        *pbuff++ = '\0';
                        len--;
                      }
                  }
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

    for (int i = 0; i < USH_MAX_COMMANDS; i++)
      {
        if (ushell_cmds_[i] == nullptr)
          {
            ushell_cmds_[i] = ucmd;
            result = true;
            break;
          }
      }

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

  cmd_info_t*
  ushell_cmd::get_cmd_info (void)
  {
    return &info_;
  }

  //----------------------------------------------------------------------------

  ush_version::ush_version (void)
  {
    trace::printf ("%s() %p\n", __func__, this);
    info_.command = "ver";
    info_.help_text = "Show the ushell version";
  }

  ush_version::~ush_version ()
  {
    trace::printf ("%s() %p\n", __func__, this);
  }

  int
  ush_version::do_cmd (class ushell* ush, int argc, char* argv[])
  {
    uint8_t version_major, version_minor, version_patch;

    ush->get_version (version_major, version_minor, version_patch);
    ush->printf ("Version %d.%d.%d\n", version_major, version_minor,
                 version_patch);

    return ush_ok;
  }

  //----------------------------------------------------------------------------

  ush_help::ush_help (void)
  {
    trace::printf ("%s() %p\n", __func__, this);
    info_.command = "help";
    info_.help_text = "List available commands";
  }

  ush_help::~ush_help ()
  {
    trace::printf ("%s() %p\n", __func__, this);
  }

  int
  ush_help::do_cmd (class ushell* ush, int argc, char* argv[])
  {
    ush->printf ("Following commands are available:\n");
    class ushell_cmd** pclasses;
    for (pclasses = ushell::ushell_cmds_; *pclasses != nullptr; pclasses++)
      {
        ush->printf ("  %-10s%s\n", (*pclasses)->get_cmd_info ()->command,
                     (*pclasses)->get_cmd_info ()->help_text);
      }
    ush->printf ("For help on a specific command, type \"<cmd> -h\"\n");

    return ush_ok;
  }

  //----------------------------------------------------------------------------

  ush_quit::ush_quit (void)
  {
    trace::printf ("%s() %p\n", __func__, this);
    info_.command = "exit";
    info_.help_text = "Exit (or restart) the ushell";
  }

  ush_quit::~ush_quit ()
  {
    trace::printf ("%s() %p\n", __func__, this);
  }

  int
  ush_quit::do_cmd (class ushell* ush, int argc, char* argv[])
  {
    return ush_exit;
  }

  //----------------------------------------------------------------------------

  ush_test::ush_test (void)
  {
    trace::printf ("%s() %p\n", __func__, this);
    info_.command = "test";
    info_.help_text = "This is a test command";
  }

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

  // instantiate all command classes

  ush_version version
    { };

  ush_quit quit
    { };

  ush_help help
    { };

  ush_test test
    { };

}

#pragma GCC diagnostic pop
