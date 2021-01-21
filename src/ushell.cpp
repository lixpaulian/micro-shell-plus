/*
 * ushell.cpp
 *
 * Copyright (c) 2020 Lix N. Paulian (lix@paulian.net)
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

ushell::ushell (const char* name, const char* char_device) :
    th_name_
      { name }, //
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
ushell::ushell_thread (void* args)
{
  int c;
  ushell* self = static_cast<ushell*> (args);
  char buffer[256];
  char prompt[] =
    { ": " };

  self->tty = static_cast<posix::tty_canonical*> (posix::open (
      self->char_device_, 0));

  if (self->tty != nullptr)
    {
      struct termios tio_orig, tio;

      if (!((self->tty->tcgetattr (&tio)) < 0))
        {
          // configure the tty in canonical mode
          memcpy (&tio_orig, &tio, sizeof(struct termios));
          tio.c_lflag |= (ICANON | ECHO | ECHOE);
          tio.c_iflag |= (ICRNL | IMAXBEL);
          tio.c_oflag |= (OPOST | ONLCR);
          tio.c_cc[VEOF] = 4;   // ctrl-d
          tio.c_cc[VERASE] = '\b';
          if (!((self->tty->tcsetattr (TCSANOW, &tio)) < 0))
            {
              do
                {
                  self->tty->write (prompt, strlen (prompt));
                  if ((c = self->tty->read (buffer, sizeof(buffer))) > 0)
                    {
                      int result = self->cmdExec (buffer);  // parse and execute command

                      if (result == ERROR)
                        {
//                          if (co->errType == EXITCOMMAND) /* exit command, we must leave now */
//                            {
//                              break;
//                            }
                          self->printf ("ERROR\n");
                        }
                    }
                }
              while (!(c < 0)); // exit on tty error

              // restore the original tty configuration
              self->tty->tcsetattr (TCSANOW, &tio_orig);
            }
          self->tty->close ();
        }
    }

  return nullptr;
}

int
ushell::cmdExec (char* buff)
{
  int result = SUCCESS;

#if 0
  char* pbuff;
  const cmds_t* pcmd;
  const cmdstab_t* pcmdtab;
   bool state = false;
  char* argv[MAX_PARAMS]; /* pointers on parameters */
  int argc; /* parameter counter */

  result = ERROR; // init result just in case of failure
  co->errType = CMD_NOT_FOUND;

  pbuff = buff;
  while (*pbuff != ' ' && *pbuff != '\0')
    {
      pbuff++;
    }
  *pbuff++ = '\0'; // insert terminator

  /* iterate all command tables, one by one */
  for (pcmdtab = cmdtabs; pcmdtab->cmdstable; pcmdtab++)
    {
      // lookup in the commands table
      for (pcmd = pcmdtab->cmdstable; pcmd->name; pcmd++)
        {
          if (!strcasecmp (buff, pcmd->name)
              && (pcmd->perms & (3 << co->cli_channel))
              && (!(pcmd->perms & CLI_EXEC_EXTENDED) || co->extended_cmds))
            {
              // valid command, parse parameters, if any
              for (argc = 0; argc < MAX_PARAMS; argc++)
                {
                  if (!*pbuff)
                    {
                      break;                  // end of line reached
                    }
                  if (*pbuff == '\"')
                    {
                      pbuff++;              // suck up the "
                      argv[argc] = pbuff;
                      while (*pbuff != '\"' && *pbuff != '\0')
                        {
                          pbuff++;
                        }
                      *pbuff++ = '\0';
                      if (*pbuff)           // if not end of line...
                        {
                          pbuff++;          // suck up the trailing space too
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

              if ((pcmd->perms & (CLI_RESTRICTED_PERMS_MASK << co->cli_channel)))
                {
                  co->restricted = false;
                }
              else
                {
                  co->restricted = true;
                }

              result = (*pcmd->func) (co, argc, &argv[0]); // execute command
              state = true; /* all done, exit both loops */
              break;
            }
        }
      if (state) /* if all done, break out */
        {
          break;
        }
    }
#endif
  return result;
}

int
ushell::printf (const char* fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  int result = vdprintf (tty->file_descriptor (), fmt, ap);
  va_end(ap);

  return result;
}

#pragma GCC diagnostic pop
