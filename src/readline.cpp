/*
 * readline.cpp
 *
 * Copyright (c) 2021 Lix N. Paulian (lix@paulian.net)
 *
 * This file, as well as the associated header file (readline.h) is based
 * on work done by Vladimir Antonov. The original MIT License also applies to
 * this code. See also <https://github.com/Holixus/readline>.
 *
 * The scope of changes was to create a "readline" intended for small embedded
 * systems, keeping calls to malloc/free to a reasonable minimum inckuding the
 * option to completely eliminate them. Depending on the available resources
 * "readline" can be configured from a minimum functionality to full featured
 * (history, auto-completion, non-volatile history, etc.).
 *
 * Following changes/additions were made to the original code:
 *
 * - Wrapped the code into a C++ class. With proper use, the code is fully
 * re-entrant and thread safe.
 *
 * - Removed the "windowing" code.
 *
 * - Substantially reduced the stack use.
 *
 * - Made UTF-8 support optional, by disabling it RAM and flash resources are
 * further reduced.
 *
 * More to come, this is still work in progress.
 *
 * 12 February 2021
 *
 * The original copyright notice follows.
 */

/* MIT License

 Copyright (c) 2010 Vladimir Antonov

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.

 */

#include <cmsis-plus/rtos/os.h>
#include <cmsis-plus/diag/trace.h>
#include <cmsis-plus/posix/termios.h>

#include "readline.h"

#ifndef countof
# define countof(arr)  (sizeof(arr)/sizeof(arr[0]))
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

namespace ushell
{

  read_line::read_line (rl_get_completion_fn gc) :
      get_completion_
        { gc }
  {
  }

  read_line::~read_line ()
  {
  }

  void
  read_line::init (os::posix::tty_canonical* tty, char const* file) // todo: load history file
  {
    hh_t hh;

    // init tty
    tty_ = tty;

    // init the history, first add an empty entry
    hh.prev = 0;
    hh.next = sizeof(hh_t) + 1;
    memcpy (history_, &hh, sizeof(hh_t));
    history_[sizeof(hh_t)] = '\0';

    // add behind the empty entry an "end of history" entry
    hh.next = 0;
    memcpy (history_ + sizeof(hh_t) + 1, &hh, sizeof(hh_t));
    current_ = history_;
  }

  int
  read_line::readline (char const* prompt, void* buff, size_t len)
  {
    const char nl[] =
      { "\n" };
    length_ = cur_pos_ = 0;
    finish_ = false;
    raw_ = (char*) buff;
    raw_len_ = len;
    raw_[0] = '\0';
#if SHELL_UTF8_SUPPORT == true
    line_[0] = '\0';
#endif

    out (prompt, strlen (prompt));

    char ch, seq[12], * seqpos = seq;
    while (tty_->read (&ch, 1) > 0)
      {
        if (seqpos >= seq + sizeof(seq))
          {
            seqpos = seq; // wrong sequence -- wrong reaction :)
          }
        *seqpos++ = ch;
        *seqpos = 0;
        if (!skip_char_seq (seq))
          {
            continue;   // wrong seq
          }
        if (exec_seq (seq))
          {
            break;      // finish
          }
        seqpos = seq;
      }

    cursor_end (this);
    out (nl, strlen (nl));
#if SHELL_UTF8_SUPPORT == true
    gtoutf8 (raw_, line_, -1);
#endif
    history_add (raw_);
    return strlen (raw_);
  }

  void
  read_line::end (void)
  {
    // TODO clean-up
  }

  //----------------------------------------------------------------------------

  void
  read_line::history_add (char const* string)
  {
    if (*string)
      {
        hh_t hh;
        char* p = history_ + sizeof(hh_t) + 1; // skip the empty entry

        memcpy (&hh, p, sizeof(hh_t));
        if (strncmp (p + sizeof(hh_t), string, strlen (string)))
          {
            size_t rec_len = sizeof(hh_t) + strlen (string) + 1; // + null terminator
            hh.prev = rec_len;
            memcpy (p, &hh, sizeof(hh_t));

            // shift memory up to make space for the new entry
            memmove (p + rec_len, p,
                     sizeof(history_) - (rec_len + sizeof(hh_t) + 1));

            hh.prev = sizeof(hh_t) + 1;
            hh.next = rec_len;
            memcpy (p, &hh, sizeof(hh_t));
            memcpy (p + sizeof(hh_t), string, strlen (string));
            p[rec_len - 1] = '\0';      // insert null terminator
          }
        current_ = history_;            // reset history pointer
      }
  }

  int
  read_line::out (const char* data, int size)
  {
    return tty_->write (data, size);
  }

  read_line::rl_glyph_t
  read_line::utf8_to_glyph (char const** utf8)
  {
    rl_glyph_t glyph = 0;
    char const* raw = *utf8;

    if (!(*raw & 0x80))
      {
        glyph = *raw++;
      }
    else if ((*raw & 0xE0) == 0xC0)
      {
        if ((raw[1] & 0300) == 0200)
          {
            glyph = ((raw[0] & 0x1F) << 6) + (raw[1] & 0x3F);
            raw += 2;
          }
        else
          {
            return 0;
          }
      }
    else if ((*raw & 0xF0) == 0xE0)
      {
        if ((raw[1] & 0300) == 0200 && (raw[2] & 0300) == 0200)
          {
            glyph = ((raw[0] & 0xF) << 12) + ((raw[1] & 0x3F) << 6)
                + (raw[2] & 0x3F);
            raw += 3;
          }
        else
          {
            return 0;
          }
      }
    *utf8 = raw;

    return glyph;
  }

  read_line::rl_glyph_t*
  read_line::utf8tog (rl_glyph_t* glyphs, char const* raw)
  {
    while (*raw)
      {
        rl_glyph_t gl = utf8_to_glyph (&raw);
        if (gl)
          {
            *glyphs++ = gl;
          }
        else
          {
            ++raw;      // skip a char of wrong utf8 sequence
          }
      }

    return glyphs;
  }

  int
  read_line::utf8_width (char const* raw)
  {
    int count = 0;
    while (*raw)
      {
        if (!utf8_to_glyph (&raw))
          ++raw;        // skip a char of wrong utf8 sequence
        ++count;
      }

    return count;
  }

  int
  read_line::one_gtoutf8 (char* raw, rl_glyph_t glyph)
  {
    int count = 0;

    if (glyph)
      {
        if (!(glyph & 0xFF80))
          {
            *raw++ = glyph;
            count = 1;
          }
        else if (!(glyph & 0xF800))
          {
            *raw++ = 0xC0 | (glyph >> 6);
            *raw++ = 0x80 | (glyph & 0x3F);
            count = 2;
          }
        else
          {
            *raw++ = 0xE0 | (glyph >> 12);
            *raw++ = 0x80 | (0x3F & glyph >> 6);
            *raw++ = 0x80 | (0x3F & glyph);
            count = 3;
          }
      }
    *raw = 0;
    return count;
  }

  char*
  read_line::gtoutf8 (char* raw, rl_glyph_t const* glyphs, int count)
  {
    int n = 0;

    while (*glyphs && count--)
      {
        n = one_gtoutf8 (raw, *glyphs++);
        raw += n;
      }
    *raw = 0;
    return raw;
  }

  int
  read_line::skip_char_seq (char const* start)
  {
    char const* raw = start;

#if SHELL_UTF8_SUPPORT == true
    rl_glyph_t glyph = utf8_to_glyph (&raw);

    if (glyph != '\033')
#else
    if (*raw++ != '\033')
#endif
      {
        return raw - start;
      }

    unsigned char ch = *raw++;
    switch (ch)
      {
      case '[':
      case 'O':
        while (isdigit (*raw) || *raw == ';')
          {
            ++raw;
          }
        ch = *raw++;
        if (64 <= ch && ch <= 126)
          {
            return raw - start;
          }
        return 0;

      default:
        if (32 <= ch && ch <= 127)
          {
            return raw + 1 - start;
          }
      }

    return 0; // not closed sequence
  }

  void
  read_line::move (int count)
  {
    if (count < 0)
      {
        while (count++)
          {
            out (bs, strlen (bs));
          }
      }
    else if (count > 0)
      {
#if SHELL_UTF8_SUPPORT == true
        char buf[4];
        rl_glyph_t* gl = line_ + cur_pos_;

        while (count-- && *gl)
          {
            int n = one_gtoutf8 (buf, *gl++);
            if (n)
              {
                out (buf, n);
              }
          }
#else
        count = std::min (count, (int) strlen (raw_ + cur_pos_));
        out (raw_ + cur_pos_, count);
#endif
      }
    return;
  }

  void
  read_line::update_tail (int afterspace)
  {
    int c = afterspace;
    char buf[4];

#if SHELL_UTF8_SUPPORT == true
    rl_glyph_t* gl = line_ + cur_pos_;

    while (*gl)
      {
        int n = one_gtoutf8 (buf, *gl++);
        if (n)
          {
            out (buf, n);
          }
      }
#else
    out (raw_ + cur_pos_, strlen (raw_ + cur_pos_));
#endif

    buf[0] = ' ';
    while (c--)
      {
        out (buf, 1);
      }

    c = afterspace + length_ - cur_pos_;
    if (c > 0)
      {
        cur_pos_ += c;
        move (-c);
        cur_pos_ -= c;
      }
  }

  void
  read_line::write_part (int start, int length)
  {
#if SHELL_UTF8_SUPPORT == true

    char buf[4];
    rl_glyph_t* gl = line_ + start;

    while (length-- && *gl)
      {
        int n = one_gtoutf8 (buf, *gl++);
        if (n)
          {
            out (buf, n);
          }
      }
#else
    int count;

    if (length < 0)
      {
        count = strlen (raw_ + start);
      }
    else
      {
        count = std::min (length, (int) strlen (raw_ + start));
      }
    out (raw_ + start, count);
#endif
  }

  void
  read_line::set_text (char const* text, int redraw)
  {
    if (redraw)
      {
        cursor_home (this);
      }

    int oldlen = length_;

    strncpy (raw_, text, std::min (strlen (text) + 1, raw_len_ - 1));
    raw_[raw_len_ - 1] = '\0'; // make sure we have a terminator
#if SHELL_UTF8_SUPPORT == true
    rl_glyph_t* end = utf8tog (line_, raw_);
    *end = '\0';
    length_ = cur_pos_ = end - line_;
#else
    length_ = cur_pos_ = strlen (text);
#endif

    if (redraw)
      {
        write_part (0, length_);
        if (oldlen > length_)
          {
            update_tail (oldlen - length_);
          }
      }
  }

  void
  read_line::insert_seq (char const* seq)
  {
#if SHELL_UTF8_SUPPORT == true
    int count = utf8_width (seq);
    int max_count = countof(line_) - length_ - 1;

    count = std::min (count, max_count);

    if (count)
      {
        if (length_ - cur_pos_)
          {
            memmove (line_ + cur_pos_ + count, line_ + cur_pos_,
                     sizeof(rl_glyph_t) * (length_ - cur_pos_));
          }
        utf8tog (line_ + cur_pos_, seq);
        length_ += count;
        line_[length_] = 0;
      }
#else
    int count = strlen (seq);
    int max_count = raw_len_ - length_ - 1;

    count = std::min (count, max_count);

    if (count)
      {
        if (length_ - cur_pos_)
          {
            memmove (raw_ + cur_pos_ + count, raw_ + cur_pos_,
                     (length_ - cur_pos_));
          }
        memcpy (raw_ + cur_pos_, seq, strlen (seq));
        length_ += count;
        raw_[length_] = 0;
      }
#endif
    write_part (cur_pos_, count);
    cur_pos_ += count;
    update_tail (0);
  }

  bool
  read_line::exec_seq (char* seq)
  {
    const rl_command_t* cmd = rl_commands, * end = rl_commands
        + countof(rl_commands);

    while (cmd < end)
      {
        if (!strcmp (cmd->seq, seq))
          {
            cmd->handler (this);
            break;
          }
        ++cmd;
      }

    if (cmd == end && (seq[0] & 0xE0))
      {
        insert_seq (seq);
      }

    return finish_;
  }

  int
  read_line::next_word (void)
  {
    unsigned int pos = cur_pos_, length = length_;

#if SHELL_UTF8_SUPPORT == true
    while (pos < length && line_[pos] != ' ')
#else
    while (pos < length && raw_[pos] != ' ')
#endif
      {
        ++pos;
      }

#if SHELL_UTF8_SUPPORT == true
    while (pos < length && line_[pos] == ' ')
#else
    while (pos < length && raw_[pos] == ' ')
#endif
      {
        ++pos;
      }

    return pos;
  }

  void
  read_line::delete_n (int count)
  {
    if (cur_pos_ < length_ && count)
      {
        int tail = (length_ - cur_pos_);
        if (count > tail)
          {
            count = tail;
          }
#if SHELL_UTF8_SUPPORT == true
        memmove (line_ + cur_pos_, line_ + cur_pos_ + count,
                 (length_ - cur_pos_ - count + 1) * sizeof(rl_glyph_t));
#else
        memmove (raw_ + cur_pos_, raw_ + cur_pos_ + count,
                 (length_ - cur_pos_ - count + 1));
#endif
        length_ -= count;
        update_tail (count);
      }
  }

  //----------------------------------------------------------------------------

  void
  read_line::cursor_home (class read_line* self)
  {
    self->move (-self->cur_pos_);
    self->cur_pos_ = 0;
  }

  void
  read_line::cursor_end (class read_line* self)
  {
    self->write_part (self->cur_pos_, -1);
    self->cur_pos_ = self->length_;
  }

  void
  read_line::cursor_left (class read_line* self)
  {
    if (self->cur_pos_)
      {
        self->move (-1);
        --self->cur_pos_;
      }
  }

  void
  read_line::cursor_right (class read_line* self)
  {
    if (self->cur_pos_ < self->length_)
      {
        self->write_part (self->cur_pos_++, 1);
      }
  }

  void
  read_line::cursor_word_left (class read_line* self)
  {
    if (!self->cur_pos_)
      {
        return;
      }

    unsigned int pos = self->cur_pos_;
#if SHELL_UTF8_SUPPORT == true
    while (pos && self->line_[pos - 1] == ' ')
#else
    while (pos && self->raw_[pos - 1] == ' ')
#endif
      {
        --pos;
      }

#if SHELL_UTF8_SUPPORT == true
    while (pos && self->line_[pos - 1] != ' ')
#else
    while (pos && self->raw_[pos - 1] != ' ')
#endif
      {
        --pos;

      }

    self->move (pos - self->cur_pos_);
    self->cur_pos_ = pos;
  }

  void
  read_line::cursor_word_right (class read_line* self)
  {
    if (self->cur_pos_ >= self->length_)
      {
        return;
      }

    unsigned int pos = self->next_word ();
    self->write_part (self->cur_pos_, pos - self->cur_pos_);
    self->cur_pos_ = pos;
  }

  void
  read_line::delete_one (class read_line* self)
  {
    self->delete_n (1);
  }

  void
  read_line::backspace (class read_line* self)
  {
    if (self->cur_pos_)
      {
        self->move (-1);
        --self->cur_pos_;
        self->delete_n (1);
      }
  }

  void
  read_line::backword (class read_line* self)
  {
    int end = self->cur_pos_;
    cursor_word_left (self);
    self->delete_n (end - self->cur_pos_);
  }

  void
  read_line::delete_word (class read_line* self)
  {
    unsigned int end = self->next_word ();
    self->delete_n (end - self->cur_pos_);
  }

  void
  read_line::delete_to_begin (class read_line* self)
  {
    int len = self->cur_pos_;
    cursor_home (self);
    self->delete_n (len);
  }

  void
  read_line::delete_to_end (class read_line* self)
  {
    self->delete_n (self->length_ - self->cur_pos_);
  }

  void
  read_line::history_back (class read_line* self)
  {
    hh_t hh;

    memcpy (&hh, self->current_, sizeof(hh_t));
    if (hh.next)
      {
        if ((self->current_ + hh.next)
            < (self->history_ + sizeof(self->history_) - sizeof(hh_t)))
          {
            self->current_ += hh.next;
            memcpy (&hh, self->current_, sizeof(hh_t));
            if (hh.next != 0
                && ((self->current_ + hh.next)
                    < (self->history_ + sizeof(self->history_))))
              {
                self->set_text (self->current_ + sizeof(hh_t), 1);
              }
            else
              {
                self->current_ -= hh.prev; // turn back
              }
          }
      }
  }

  void
  read_line::history_forward (class read_line* self)
  {
    hh_t hh;

    memcpy (&hh, self->current_, sizeof(hh_t));

    if (hh.prev)
      {
        if ((self->current_ - hh.prev) >= self->history_)
          {
            self->current_ -= hh.prev;
            self->set_text (self->current_ + sizeof(hh_t), 1);
          }
      }
  }

  void
  read_line::history_begin (class read_line* self)
  {
    hh_t hh;
    char* savep = self->current_;

    while (true)
      {
        memcpy (&hh, self->current_, sizeof(hh_t));
        if (hh.next)
          {
            if ((self->current_ + hh.next)
                < (self->history_ + sizeof(self->history_) - sizeof(hh_t)))
              {
                savep = self->current_;
                self->current_ += hh.next;
                memcpy (&hh, self->current_, sizeof(hh_t));
                if (hh.next == 0
                    || ((self->current_ + hh.next)
                        >= (self->history_ + sizeof(self->history_))))
                  {
                    break;
                  }
              }
          }
      }
    self->current_ = savep;
    self->set_text (self->current_ + sizeof(hh_t), 1);
  }

  void
  read_line::history_end (class read_line* self)
  {
    self->current_ = self->history_;
    self->set_text (self->current_ + sizeof(hh_t), 1);
  }

  void
  read_line::enter (class read_line* self)
  {
    self->finish_ = true;
  }

  void
  read_line::autocomplete (class read_line* self)
  {
    if (!self->get_completion_)
      {
        return;
      }

    char* start = self->raw_;
#if SHELL_UTF8_SUPPORT == true
    char* cur_pos = self->gtoutf8 (start, self->line_, self->cur_pos_);
    self->gtoutf8 (cur_pos, self->line_ + self->cur_pos_,
                   self->length_ - self->cur_pos_);
#else
    char* cur_pos = self->raw_ + self->cur_pos_;
#endif
    char const* insert = (self->get_completion_) (start, cur_pos);
    if (insert)
      {
        self->insert_seq (insert);
      }
  }

}

#pragma GCC diagnostic pop
