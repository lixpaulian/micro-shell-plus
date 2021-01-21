/*
 * getline.cpp
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
 * Created on: 4 December 2020 (LNP)
 */



#include <cmsis-plus/rtos/os.h>
#include <cmsis-plus/diag/trace.h>
#include <cmsis-plus/posix-io/io.h>

#include "getline.h"

using namespace os;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

getline*
open (const char* path, int oflag, ...)
{
  // Forward to the variadic version of the function.
  std::va_list args;
  va_start(args, oflag);
  getline* const ret = static_cast<getline*> (posix::vopen (path, oflag, args));
  va_end(args);

  return ret;
}


getline::getline (posix::io_impl& impl, type t) : posix::io { impl, t }
{
  trace::printf ("%s() %p\n", __func__, this);
}

getline::~getline ()
{
  trace::printf ("%s() %p\n", __func__, this);
}

/**
 * @brief Get a character and echo it, if required.
 * @param secret: if true, replace echo-ed characters with asterisks ('*').
 * @return A character if success, EOF otherwise; other negative numbers for
 *      special keys (e.g. arrow keys) or timeout.
 */
int
getline::get_echo (bool secret)
{
  int in, n;
  char command = 0;

  for (in = 0;;)
    {
       bool do_echo = false;

      // wait for an input character
      if ((n = this->read (&in, 1)) > 0)
        {
          switch (in)
            {
            case esc:
              command = 1;
              continue;

            case '[':
              if (command)
                {
                  command = in;
                  continue;
                }
              do_echo = true;
              break;

            case 'A':
              if (command == '[')
                {
                  command = 0;
                  in = up_arrow;
                }
              else
                {
                  do_echo = true;
                }
              break;

            case 'B':
              if (command == '[')
                {
                  command = 0;
                  in = down_arrow;
                }
              else
                {
                  do_echo = true;
                }
              break;

            case 'C':
              if (command == '[')
                {
                  command = 0;
                  in = right_arrow;
                }
              else
                {
                  do_echo = true;
                }
              break;

            case 'D':
              if (command == '[')
                {
                  command = 0;
                  in = left_arrow;
                }
              else
                {
                  do_echo = true;
                }
              break;

            default:
              do_echo = true;
              break;
            }
        }
      else if (n == 0)
        {
          in = timeout;
        }
      else
        {
          in = EOF;     // I/O error, the caller should check errno
        }

      if (do_echo && echo)
        {
          if (secret && in != '\n' && in != '\r')
            {
              put_c ('*');
            }
          else
            {
              put_c (in); // echo the character back
            }
        }
      break; // break-out the loop and return to caller
    }

  return in;
}

int
getline::get_line (const char* prompt, bool secret)
{
  int c, n = 0;
  char* p = buffer_;
  bool leave = false;

//  memset (buffer_, 0, sizeof(buffer_));

  if (echo)
    {
      write (prompt, strlen (prompt));
    }

  do
    {
      c = get_echo (secret);

      switch (c)
        {
        case EOF:
          n = c;
          leave = true;
          break;

        case ctrl_c: // cancel
          n = user_cancel; // todo: we should not exit, rather clear the line and go to line start
          break;

        case up_arrow:
          n = up_arrow;
          leave = true;
          break;

        case down_arrow:
          {
#if 0
            const char _erase_to_eol[] =
              { '\r', esc, '[', 'K', '\0' };
            tty->write (_erase_to_eol, strlen (_erase_to_eol));
            if (prompt)
              {
                tty->write (prompt, strlen (prompt));
              }
            p = buffer_; // restore buffer start
          if (cliHist (c == CIN_UP_ARROW ? CLI_HIST_PREV : CLI_HIST_NEXT,
                       p) == ERROR)
            {
              n = 0;
              break;
            }
            n = strlen (p);
            tty->write (p, n);
            p += n;
#endif
          }
          n = down_arrow;
          leave = true;
          break;

        case backspace:
          if (n > 0)
            {
              const char _bs[] =
                { " \b" };
              this->write (_bs, strlen (_bs));
              *p-- = '\0';
              n--;
            }
          else
            {
              const char _one_right[] =
                { esc, '[', 'C', '\0' };
              this->write (_one_right, strlen (_one_right));
            }
          break;

        case '\r':
          put_c ('\n');
          leave = true;
          break;

        case '\n':
          put_c ('\r');
          leave = true;
          break;

        default:
          *p++ = (char) c;
          if (n++ >= (int) sizeof(buffer_))
            {
              p--;        // if buffer overflow, wait
              n--;
              leave = true;
            }
        }
    }
  while (!leave);

  *p = '\0';     // insert terminator

  return n;
}

#if 0
int
tty_canonical::get_line (void* buf, std::size_t nbyte)
{
  int c, n = 0;
  char* p = (char*) buf;
  char* end = p;
  char command = 0;

  do
    {
      c = 0;
      if (impl ().do_read (&c, 1) > 0)
        {
          switch (c)
            {
            case esc:
              command = 1;
              continue;

            case '[':
              if (command)
                {
                  command = c;
                  continue;
                }
              break;

            case 'C':
              if (command == '[')
                {
                  // handle right arrow
                  command = 0;
                  char _ra[] =
                    { esc, '[', 'C', '\0' };
                  if (end > p)
                    {
                      p++;
                      for (char* s = _ra; *s != '\0'; s++)
                        {
                          echo_char (*s);
                        }
                    }
                }
              break;

            case 'D':
              if (command == '[')
                {
                  // handle left arrow
                  command = 0;
                  char _la[] =
                    { esc, '[', 'D', '\0' };
                  if (p > (char*) buf)
                    {
                      p--;
                      for (char* s = _la; *s != '\0'; s++)
                        {
                          echo_char (*s);
                        }
                    }
                }
              break;

            default:
              // handle erase & kill
              if (c == cc_.verase)
                {
                  if (n > 0)
                    {
                      char _bs[] =
                        { '\b', ' ', '\b', '\0' };
                      _bs[0] = _bs[2] = cc_.verase;
                      for (char* s = _bs; *s != '\0'; s++)
                        {
                          echo_char (*s);
                        }
                      p--;
                      end--;
                      n--;
                    }
                }
              else if (c == cc_.vkill)
                {
                  char _kill[] =
                    { esc, '[', 'K', '\0' };
                  while (n)
                    {
                      echo_char (cc_.verase);
                      n--;
                    }
                  for (char* s = _kill; *s != '\0'; s++)
                    {
                      echo_char (*s);
                    }
                  p = (char*) buf;
                  end = p;
                }
              else
                {
                  // handle iflags
                  if (if_.istrip)
                    {
                      c &= 0x7F;
                    }
                  if (c == '\r')
                    {
                      if (if_.igncr)
                        {
                          continue;
                        }
                      else if (if_.icrnl)
                        {
                          c = '\n';
                        }
                    }
                  else if (c == '\n' && if_.inlcr)
                    {
                      c = '\r';
                    }

                  // store character in buffer
                  *p++ = (char) c;
                  end++;
                  if (n++ >= (int) nbyte)
                    {
                      p--;  // if buffer overflow, wait for a cr/lf
                      end--;
                      n--;
                      if (if_.imaxbel)
                        {
                          echo_char (bell);
                        }
                      c = '\n'; // force an exit
                    }
                  else
                    {
                      // handle oflags
                      if (c == '\r')
                        {
                          if (of_.opost && of_.ocrnl)
                            {
                              echo_char ('\n');
                            }
                          else
                            {
                              echo_char ('\r');
                            }
                        }
                      else if (c == '\n')
                        {
                          if (of_.opost && of_.onlcr)
                            {
                              echo_char ('\r');
                              echo_char ('\n');
                            }
                          else
                            {
                              echo_char ('\n');
                            }
                        }
                      else
                        {
                          echo_char (c);
                        }
                    }
                }
            }
        }
      else
        {
          break;
        }
    }
  while (c != '\n');

  return n;
}

inline void
tty_canonical::echo_char (char c)
{
  if (lf_.echo)
    {
      impl ().do_write (&c, 1);
    }
}

#endif

