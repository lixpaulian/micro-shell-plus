/*
 * This file is part of the µOS++ distribution.
 *   (https://github.com/micro-os-plus)
 * Copyright (c) 2017 Liviu Ionescu.
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
 */

/*
 * 7/12/2020: Added support for cooked terminal mode, aka "canonical" (LNP).
 * 4/01/2021: re-implemented the read and write calls to support some of the
 * termios flags (LNP).
 */

#include <cmsis-plus/diag/trace.h>
#include "tty-canonical.h"

// ----------------------------------------------------------------------------

namespace os
{
  namespace posix
  {
    // ========================================================================

    tty_canonical::tty_canonical (tty_impl& impl, const char* name) :
        tty
          { impl, name }
    {
      type_ |= type::tty;
#if defined(OS_TRACE_POSIX_IO_TTY)
      trace::printf ("tty_canonical::%s(\"%s\")=@%p\n", __func__, name_, this);
#endif
    }

    tty_canonical::~tty_canonical () noexcept
    {
#if defined(OS_TRACE_POSIX_IO_TTY)
      trace::printf ("tty_canonical::%s() @%p %s\n", __func__, this, name_);
#endif
    }

    // ------------------------------------------------------------------------

    /**
     * @brief Force a break.
     * @param duration: length of break in milliseconds (implementation dependent).
     * @return 0 if successfull, -1 otherwise.
     */
    inline int
    tty_canonical::tcsendbreak (int duration)
    {
      return impl ().do_tcsendbreak (duration);
    }

    /**
     * @brief Return the tty's current configuration.
     * @param ptio: pointer on a termios structure.
     * @return 0 if successfull, -1 otherwise.
     */
    inline int
    tty_canonical::tcgetattr (struct termios* ptio)
    {
      int ret;

      if (!((ret = impl ().do_tcgetattr (ptio)) < 0))
        {
          lf_.icanon ? (ptio->c_lflag |= ICANON) : (ptio->c_lflag &= ~ICANON);
          lf_.echo ? (ptio->c_lflag |= ECHO) : (ptio->c_lflag &= ~ECHO);
          lf_.echoe ? (ptio->c_lflag |= ECHOE) : (ptio->c_lflag &= ~ECHOE);

          if_.istrip ? (ptio->c_iflag |= ISTRIP) : (ptio->c_iflag &= ~ISTRIP);
          if_.icrnl ? (ptio->c_iflag |= ICRNL) : (ptio->c_iflag &= ~ICRNL);
          if_.igncr ? (ptio->c_iflag |= IGNCR) : (ptio->c_iflag &= ~IGNCR);
          if_.inlcr ? (ptio->c_iflag |= INLCR) : (ptio->c_iflag &= ~INLCR);
          if_.imaxbel ?
              (ptio->c_iflag |= IMAXBEL) : (ptio->c_iflag &= ~IMAXBEL);

          of_.opost ? (ptio->c_oflag |= OPOST) : (ptio->c_oflag &= ~OPOST);
          of_.onlcr ? (ptio->c_oflag |= ONLCR) : (ptio->c_oflag &= ~ONLCR);
          of_.ocrnl ? (ptio->c_oflag |= OCRNL) : (ptio->c_oflag &= ~OCRNL);

          ptio->c_cc[VEOF] = cc_.veof;
          ptio->c_cc[VEOL] = cc_.veol;
          ptio->c_cc[VEOL2] = cc_.veol2;
          ptio->c_cc[VERASE] = cc_.verase;
          ptio->c_cc[VKILL] = cc_.vkill;
        }

      return ret;
    }

    /**
     * @brief Set the tty configuration.
     * @param options: optional actions (see termios.h).
     * @param ptio: pointer on a termios structure.
     * @return 0 if successfull, -1 otherwise.
     */
    inline int
    tty_canonical::tcsetattr (int options, const struct termios* ptio)
    {
      lf_.icanon = (ptio->c_lflag & ICANON) ? true : false;
      lf_.echo = (ptio->c_lflag & ECHO) ? true : false;
      lf_.echoe = (ptio->c_lflag & ECHOE) ? true : false;

      if_.istrip = (ptio->c_iflag & ISTRIP) ? true : false;
      if_.icrnl = (ptio->c_iflag & ICRNL) ? true : false;
      if_.igncr = (ptio->c_iflag & IGNCR) ? true : false;
      if_.inlcr = (ptio->c_iflag & INLCR) ? true : false;
      if_.imaxbel = (ptio->c_iflag & IMAXBEL) ? true : false;

      of_.opost = (ptio->c_oflag & OPOST) ? true : false;
      of_.onlcr = (ptio->c_oflag & ONLCR) ? true : false;
      of_.ocrnl = (ptio->c_oflag & OCRNL) ? true : false;

      cc_.veof = ptio->c_cc[VEOF];
      cc_.veol = ptio->c_cc[VEOL];
      cc_.veol2 = ptio->c_cc[VEOL2];
      cc_.verase = ptio->c_cc[VERASE];
      cc_.vkill = ptio->c_cc[VKILL];

      return impl ().do_tcsetattr (options, ptio);
    }

    /**
     * @brief Flush a tty queue.
     * @param queue_selector: one of TCIFLUSH, TCOFLUSH or TCIOFLUSH.
     * @return 0 if successfull, -1 otherwise.
     */
    inline int
    tty_canonical::tcflush (int queue_selector)
    {
      return impl ().do_tcflush (queue_selector);
    }

    /**
     * @brief Wait until all characters have been sent.
     * @return 0 if successfull, -1 otherwise.
     */
    inline int
    tty_canonical::tcdrain (void)
    {
      return impl ().do_tcdrain ();
    }

    /**
     * @brief Implements the line discipline for read.
     * @param buf: buffer to return the data read.
     * @param nbyte: buffer length.
     * @return Number of characters returned in the buffer.
     */
    ssize_t
    tty_canonical::read (void* buf, std::size_t nbyte)
    {
      ssize_t count;

      if (lf_.icanon)
        {
          count = get_line (buf, nbyte);
        }
      else
        {
          if ((count = impl ().do_read (buf, nbyte)) > 0)
            {
              if (if_.icrnl || if_.igncr || if_.inlcr || if_.istrip)
                {
                  count = process_input (buf, count);
                }
              if (lf_.echo)
                {
                  impl ().do_write (buf, count);
                }
            }
        }

      return count;
    }

    /**
     * @brief Implements the line discipline for write.
     * @param buf: buffer to send to the tty.
     * @param nbyte: number of bytes to send.
     * @return Number of bytes sent.
     */
    ssize_t
    tty_canonical::write (const void* buf, std::size_t nbyte)
    {
      ssize_t count = 0;

      if (nbyte)
        {
          if (of_.opost && (of_.ocrnl || of_.onlcr))
            {
              count = put_line (buf, nbyte);
            }
          else
            {
              count = impl ().do_write (buf, nbyte);
            }
        }

      return count;
    }

    /**
     * @brief Implements the canonical input mode; this call returns either after
     *  an end of line character is received or the buffer becomes full.
     * @param buf: input buffer.
     * @param nbyte: buffer length.
     * @return Number of characters in the buffer, or EOF if error at read.
     */
    int
    tty_canonical::get_line (void* buf, std::size_t nbyte)
    {
      int c, n = 0;
      char* p = (char*) buf;

      do
        {
          c = 0;
          if (impl ().do_read (&c, 1) > 0)
            {
              if (c == cc_.veof)
                {
                  break; // end of file
                }
              if (c == cc_.verase)
                {
                  // handle erase
                  if (n > 0)
                    {
                      if (lf_.echoe)
                        {
                          char _bs[] =
                            { '\b', ' ', '\b', '\0' };
                          _bs[0] = _bs[2] = cc_.verase;
                          for (char* s = _bs; *s != '\0'; s++)
                            {
                              echo_char (*s);
                            }
                        }
                      else
                        {
                          echo_char (c);
                        }
                      p--;
                      n--;
                    }
                }
              else if (c == cc_.vkill)
                {
                  // handle kill
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
                  if (n++ >= (int) nbyte)
                    {
                      // buffer overflow
                      p--;
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
          else
            {
              break;
            }
        }
      while (c != '\n');

      return n;
    }

    /**
     * @brief Stub function to send characters to the tty w/wo echoing them.
     * @param c: character to be sent.
     */
    inline void
    tty_canonical::echo_char (char c)
    {
      if (lf_.echo)
        {
          impl ().do_write (&c, 1);
        }
    }

    /**
     * @brief Implements the raw output mode.
     * @param buf: buffer to send to the tty.
     * @param nbyte: number of bytes to send.
     * @return Number of bytes sent.
     */
    ssize_t
    tty_canonical::put_line (const void* buf, std::size_t nbyte)
    {
      ssize_t count = 0, n;
      const char* p = static_cast<const char*> (buf);
      const char* q = static_cast<const char*> (buf);

      do
        {
          if ((*p == '\n' && of_.onlcr) || (*p == '\r' && of_.ocrnl))
            {
              if (p - q)
                {
                  if ((n = impl ().do_write (q, p - q)) < 0)
                    {
                      count = n;
                      break;
                    }
                  else
                    {
                      count += n;
                    }
                }
              if (*p == '\n')
                {
                  impl ().do_write ("\r\n", 2);
                }
              else
                {
                  impl ().do_write ("\n", 1);
                }
              count++;
              p++;
              q = p;
            }
          else
            {
              p++;
            }
        }
      while (--nbyte);

      if (p - q)        // anything left to write?
        {
          if ((n = impl ().do_write (q, p - q)) < 0)
            {
              count = n;
            }
          else
            {
              count += n;
            }
        }

      return count;
    }

    /**
     * @brief Implements the raw input mode.
     * @param buf: input buffer.
     * @param nbyte: buffer's length.
     * @return Number of characters returned in the buffer.
     */
    ssize_t
    tty_canonical::process_input (void* buf, ssize_t nbyte)
    {
      char* p = static_cast<char*> (buf);
      ssize_t n = nbyte;

      do
        {
          if (*p == '\r' && if_.igncr)
            {
              memmove (p, p + 1, nbyte - 1);
              n--;
              continue;
            }
          if (if_.istrip)
            {
              *p &= 0x7F;
            }
          if (*p == '\r' && if_.icrnl && if_.igncr == false)
            {
              *p = '\n';
            }
          else if (*p == '\n' && if_.inlcr)
            {
              *p = '\r';
            }
          p++;
        }
      while (--nbyte);

      return n;
    }

  // ========================================================================
#if 0
    tty_impl::tty_impl (void)
    {
#if defined(OS_TRACE_POSIX_IO_TTY)
      trace::printf ("tty_impl::%s()=@%p\n", __func__, this);
#endif
    }

    tty_impl::~tty_impl ()
    {
#if defined(OS_TRACE_POSIX_IO_TTY)
      trace::printf ("tty_impl::%s() @%p\n", __func__, this);
#endif
    }

    int
    tty_impl::do_isatty (void)
    {
      return 1; // Yes!
    }
#endif

  // ==========================================================================
  } /* namespace posix */
} /* namespace os */

// ----------------------------------------------------------------------------
