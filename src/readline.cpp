
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
 * systems, keeping calls to malloc/free to a reasonable minimum with the option
 * to even completely eliminate them.
 *
 * Following changes/additions were made to the original code:
 *
 * - Wrapped the code into a C++ class. With proper use, the code is fully
 * re-entrant and thread safe.
 *
 * - Removed the "windowing" code.
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

#ifdef RL_HISTORY_FILE
# include <sys/stat.h>
# include <fcntl.h>
#endif

#define CUR_LEFT   "\b"

#ifndef countof
# define countof(arr)  (sizeof(arr)/sizeof(arr[0]))
#endif

namespace ushell
{

  read_line::read_line (os::posix::tty_canonical* tty, rl_get_completion_fn gc) :
      tty_
        { tty }, //
      get_completion_
        { gc }
  {
    memset ((void*) &history_, 0, sizeof(rl_history_t));
  }

  read_line::~read_line ()
  {
  }

  /* -------------------------------------------------------------------------- */

  int
  read_line::out (const char* data, int size)
  {
    return tty_->write (data, size);
  }

  /* -------------------------------------------------------------------------- */
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

  /* ------------------------------------<------------------------------------- */
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
            ++raw; // skip a char of wrong utf8 sequence
          }
      }
    *glyphs = 0;

    return glyphs;
  }

  /* -------------------------------------------------------------------------- */
  int
  read_line::utf8_width (char const* raw)
  {
    int count = 0;
    while (*raw)
      {
        if (!utf8_to_glyph (&raw))
          {
            ++raw; // skip a char of wrong utf8 sequence
          }
        ++count;
      }

    return count;
  }

  /* -------------------------------------------------------------------------- */
  char*
  read_line::gtoutf8 (char* raw, rl_glyph_t const* glyphs, int count)
  {
    while (*glyphs && count--)
      {
        unsigned int uc = *glyphs++;
        if (!(uc & 0xFF80))
          {
            *raw++ = uc;
          }
        else if (!(uc & 0xF800))
          {
            *raw++ = 0xC0 | (uc >> 6);
            *raw++ = 0x80 | (uc & 0x3F);
          }
        else
          {
            *raw++ = 0xE0 | (uc >> 12);
            *raw++ = 0x80 | (0x3F & uc >> 6);
            *raw++ = 0x80 | (0x3F & uc);
          }
      }
    *raw = 0;
    return raw;
  }

  /* -------------------------------------------------------------------------- */
  int
  read_line::skip_char_seq (char const* start)
  {
    char const* raw = start;
    rl_glyph_t glyph = utf8_to_glyph (&raw);

    if (glyph != '\033')
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

  /* -------------------------------------------------------------------------- */
  void
  read_line::move (int count)
  {
    if (count < 0)
      {
        while (count++)
          {
            out (CUR_LEFT, strlen (CUR_LEFT));
          }
      }
    else if (count > 0)
      {
        char buf[RL_MAX_LENGTH * 3];
        char* to = gtoutf8 (buf, (rl_glyph_t*) (line_ + cur_pos_), count);
        if (to != buf)
          {
            out (buf, to - buf);
          }
      }
    return;
  }

  /* -------------------------------------------------------------------------- */
  void
  read_line::update_tail (int afterspace)
  {
    int c = afterspace;
    char buf[RL_MAX_LENGTH * 3];
    char* to = gtoutf8 (buf, (rl_glyph_t*) (line_ + cur_pos_), -1);

    while (c--)
      {
        *to++ = ' ';
      }
    *to = 0;
    if (to != buf)
      {
        out (buf, to - buf);
      }
    c = afterspace + length_ - cur_pos_;
    if (c > 0)
      {
        cur_pos_ += c;
        move (-c);
        cur_pos_ -= c;
      }
  }

  /* -------------------------------------------------------------------------- */
  void
  read_line::write_part (int start, int length)
  {
    char buf[RL_MAX_LENGTH * 2];
    char* to = gtoutf8 (buf, (rl_glyph_t*) (line_ + start), length);

    if (to != buf)
      {
        out (buf, to - buf);
      }
  }

  /* -------------------------------------------------------------------------- */
  void
  read_line::history_pop (int idx)
  {
    rl_history_t* h = &history_;

    if (idx == h->size)
      {
        if (h->line)
          {
            set_text (h->line, 1);
            free (h->line);
            h->line = NULL;
            return;
          }
      }

    if (idx >= h->size)
      {
        return;
      }

    if (!h->line)
      {
        gtoutf8 (raw_, line_, -1);
        h->line = strdup (raw_);
      }

    set_text (h->lines[idx], 1);
  }

  /* -------------------------------------------------------------------------- */
  void
  read_line::set_text (char const* text, int redraw)
  {
    if (redraw)
      {
        cursor_home (this);
      }

    int oldlen = length_;

    strncpy (raw_, text, raw_len_ - 1);
    raw_[strlen(raw_) - 1] = '\0';      // remove the ending '\n'
    rl_glyph_t* end = utf8tog ((rl_glyph_t*) line_, raw_);
    length_ = cur_pos_ = end - line_;

    if (redraw)
      {
        write_part (0, length_);
        if (oldlen > length_)
          {
            update_tail (oldlen - length_);
          }
      }
  }

  /* -------------------------------------------------------------------------- */
  void
  read_line::insert_seq (char const* seq)
  {
    rl_glyph_t uc[RL_MAX_LENGTH * 2];
    rl_glyph_t* end = utf8tog (uc, seq);
    int count = end - uc;
    int max_count = countof(line_) - length_ - 1;

    if (count > max_count)
      {
        count = max_count;
      }

    if (!count)
      {
        return;
      }

    if (length_ - cur_pos_)
      {
        memmove (line_ + cur_pos_ + count, line_ + cur_pos_,
                 sizeof(line_[0]) * (length_ - cur_pos_));
      }

    memcpy (line_ + cur_pos_, uc, sizeof(uc[0]) * count);

    length_ += count;
    line_[length_] = 0;

    write_part (cur_pos_, count);
    cur_pos_ += count;
    update_tail (0);
  }

  /* -------------------------------------------------------------------------- */
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

  /* -------------------------------------------------------------------------- */
  int
  read_line::next_word (void)
  {
    unsigned int pos = cur_pos_, length = length_;

    while (pos < length && line_[pos] != ' ')
      {
        ++pos;
      }

    while (pos < length && line_[pos] == ' ')
      {
        ++pos;
      }

    return pos;
  }

  /* -------------------------------------------------------------------------- */
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
        memmove (line_ + cur_pos_, line_ + cur_pos_ + count,
                 (length_ - cur_pos_ - count + 1) * sizeof(rl_glyph_t));
        length_ -= count;
        update_tail (count);
      }
  }

  /* -------------------------------------------------------------------------- */
  void
  read_line::history_save (void)
  {
    char const* file = history_.file;

    if (!file)
      {
        return;
      }

    int fd = open (file, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0)
      return;

    rl_history_t* h = &history_;
    int c = h->size;
    char** pos = h->lines;
    while (c--)
      {
        if (write (fd, pos[0], strlen (pos[0])) < 0 || write (fd, "\n", 1) < 0)
          break;
        ++pos;
      }

    close (fd);
  }

  /* -------------------------------------------------------------------------- */
  void
  read_line::history_empty (void)
  {
    rl_history_t* h = &history_;
    int c = h->size;
    char** pos = h->lines;

    while (c--)
      {
        free (*pos);
        *pos++ = NULL;
      }
    h->size = 0;
  }

  /* -------------------------------------------------------------------------- */
  void
  read_line::history_add (char const* string)
  {
    rl_history_t* h = &history_;

    free (h->line);
    h->line = NULL;
    if (!string[0])
      {
        return;
      }

    if (h->size && !strcmp (h->lines[h->size - 1], string))
      {
        h->current = h->size;
        return;
      }

    if (h->size >= (int) countof(h->lines))
      {
        free (h->lines[0]);
        memmove (h->lines, h->lines + 1, sizeof(h->lines[0]) * --(h->size));
      }
    h->lines[h->size++] = strdup (string);
    h->current = h->size;
  }

  /* -------------------------------------------------------------------------- */
  void
  read_line::history_load (char const* file)
  {
    if (!file)
      {
        file = RL_HISTORY_FILE;
      }

    history_.file = file;

    history_empty ();

    int fd = open (file, O_RDONLY);
    if (fd < 0)
      return;

    char line[RL_MAX_LENGTH * 2 + 4];
    char* in = line, * iend = line + sizeof(line) - 1;
    int count;

    while ((count = read (fd, in, iend - in)) > 0)
      {
        char* eoln, * end = in + count;
        for (eoln = in; eoln < end; ++eoln)
          {
            if (*eoln == '\n')
              {
                *eoln = 0;
                history_add (in);
                in = eoln + 1;
              }
          }
        if (in < end)
          {
            memmove (line, in, iend - in);
          }
        in = line + (iend - in);
      }

    close (fd);
  }

  /* -------------------------------------------------------------------------- */
  void
  read_line::history_free (void)
  {
    history_save ();
    history_empty ();
  }

  /* -------------------------------------------------------------------------- */
  int
  read_line::readline (char const* prompt, void* buff, size_t len)
  {
    raw_[0] = 0;
    line_[0] = 0;
    length_ = cur_pos_ = finish_ = 0;
    raw_ = (char*) buff;
    raw_len_ = len;

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
    gtoutf8 (raw_, line_, -1);
    history_add (raw_);
    return strlen (raw_);
  }

  //----------------------------------------------------------------------------

  /* -------------------------------------------------------------------------- */
  void
  read_line::cursor_home (class read_line* self)
  {
    self->move (-self->cur_pos_);
    self->cur_pos_ = 0;
  }

  /* -------------------------------------------------------------------------- */
  void
  read_line::cursor_end (class read_line* self)
  {
    self->write_part (self->cur_pos_, -1);
    self->cur_pos_ = self->length_;
  }

  /* -------------------------------------------------------------------------- */
  void
  read_line::cursor_left (class read_line* self)
  {
    if (self->cur_pos_)
      {
        self->move (-1);
        --self->cur_pos_;
      }
  }

  /* -------------------------------------------------------------------------- */
  void
  read_line::cursor_right (class read_line* self)
  {
    if (self->cur_pos_ < self->length_)
      {
        self->write_part (self->cur_pos_++, 1);
      }
  }

  /* -------------------------------------------------------------------------- */
  void
  read_line::cursor_word_left (class read_line* self)
  {
    if (!self->cur_pos_)
      {
        return;
      }

    unsigned int pos = self->cur_pos_;
    while (pos && self->line_[pos - 1] == ' ')
      {
        --pos;
      }

    while (pos && self->line_[pos - 1] != ' ')
      {
        --pos;

      }

    self->move (pos - self->cur_pos_);
    self->cur_pos_ = pos;
  }

  /* -------------------------------------------------------------------------- */
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

  /* -------------------------------------------------------------------------- */
  void
  read_line::delete_one (class read_line* self)
  {
    self->delete_n (1);
  }

  /* -------------------------------------------------------------------------- */
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

  /* -------------------------------------------------------------------------- */
  void
  read_line::backword (class read_line* self)
  {
    int end = self->cur_pos_;
    cursor_word_left (self);
    self->delete_n (end - self->cur_pos_);
  }

  /* -------------------------------------------------------------------------- */
  void
  read_line::delete_word (class read_line* self)
  {
    unsigned int end = self->next_word ();
    self->delete_n (end - self->cur_pos_);
  }

  /* -------------------------------------------------------------------------- */
  void
  read_line::delete_to_begin (class read_line* self)
  {
    int len = self->cur_pos_;
    cursor_home (self);
    self->delete_n (len);
  }

  /* -------------------------------------------------------------------------- */
  void
  read_line::delete_to_end (class read_line* self)
  {
    self->delete_n (self->length_ - self->cur_pos_);
  }

  /* -------------------------------------------------------------------------- */
  void
  read_line::history_back (class read_line* self)
  {
    rl_history_t* h = &self->history_;
    if (h->current)
      {
        self->history_pop (--(h->current));
      }
  }

  /* -------------------------------------------------------------------------- */
  void
  read_line::history_forward (class read_line* self)
  {
    rl_history_t* h = &self->history_;
    if (h->current < h->size)
      {
        self->history_pop (++(h->current));
      }
  }

  /* -------------------------------------------------------------------------- */
  void
  read_line::history_begin (class read_line* self)
  {
    rl_history_t* h = &self->history_;
    if (h->current)
      {
        self->history_pop ((h->current = 0));
      }
  }

  /* -------------------------------------------------------------------------- */
  void
  read_line::history_end (class read_line* self)
  {
    rl_history_t* h = &self->history_;
    if (h->current < h->size)
      {
        self->history_pop ((h->current = h->size));
      }
  }

  /* -------------------------------------------------------------------------- */
  void
  read_line::enter (class read_line* self)
  {
    const char nl[] =
      { "\n" };

    self->insert_seq (nl);
    self->finish_ = true;
  }

  /* -------------------------------------------------------------------------- */
  void
  read_line::autocomplete (class read_line* self)
  {
    if (!self->get_completion_)
      {
        return;
      }

    char* start = self->raw_;
    char* cur_pos = self->gtoutf8 (start, self->line_, self->cur_pos_);
    self->gtoutf8 (cur_pos, self->line_ + self->cur_pos_,
                   self->length_ - self->cur_pos_);

    char const* insert = (self->get_completion_) (start, cur_pos);
    if (insert)
      {
        self->insert_seq (insert);
      }
  }

}
