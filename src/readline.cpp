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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include <cmsis-plus/rtos/os.h>
#include <cmsis-plus/diag/trace.h>
#include <cmsis-plus/posix/termios.h>

#include "readline.h"

#ifdef RL_HISTORY_FILE
# include <sys/stat.h>
# include <fcntl.h>
#endif

// TODO: temporary definitions to allow compilation (LNP)

#define RL_MAX_LENGTH 256
#define RL_HISTORY_HEIGHT 32
#define RL_WINDOW_WIDTH 80

/*
 [*] terminal control (init/deinit)
 [*] commands table // do/undo pairs
 [*] commands dispatcher
 [*] commands handlers
 [*] history control
 [ ] undo buffer
 */

#define STATIC static

#define CUR_LEFT   "\b"
#define CUR_HOME   "\r"

#define CUR_UP_N    "\033[%uA"
#define CUR_DOWN_N  "\033[%uB"
#define CUR_RIGHT_N "\033[%uC"
#define CUR_LEFT_N  "\033[%uD"

#define SET_WRAP_MODE "\033[?7h"

#ifndef countof
# define countof(arr)  (sizeof(arr)/sizeof(arr[0]))
#endif

/* -------------------------------------------------------------------------- */
typedef unsigned int rl_glyph_t;

/* -------------------------------------------------------------------------- */
typedef struct rl_history
{
  char const* file;
  char* line;
  char* lines[RL_HISTORY_HEIGHT];
  int size, current;
} rl_history_t;

/* -------------------------------------------------------------------------- */
typedef struct _rl_state
{
  char* raw; /* utf-8 */
  size_t raw_len;
  rl_glyph_t line[RL_MAX_LENGTH]; /* unicode */
  int length, cur_pos; /* length and position in glyphs */

  int finish;
  rl_history_t history;

  char const* prompt;
  int prompt_width;
  rl_get_completion_fn* _get_completion;

  os::posix::tty_canonical* tty;
} rl_state_t;

/* -------------------------------------------------------------------------- */
struct _rl_command
{
  char const seq[8];
  void
  (*handler) ();
};

/* -------------------------------------------------------------------------- */
static rl_state_t* rl_state;

/* -------------------------------------------------------------------------- */
STATIC void
rl_insert_seq (char const* seq);

/* -------------------------------------------------------------------------- */

int
rl_out (const char* data, int size)
{
  return rl_state->tty->write (data, size);
}

/* -------------------------------------------------------------------------- */
int
rl_printf (const char* fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  int result = vdprintf (rl_state->tty->file_descriptor (), fmt, ap);
  va_end(ap);

  return result;
}

/* -------------------------------------------------------------------------- */
static inline rl_glyph_t
utf8_to_glyph (char const** utf8)
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
STATIC rl_glyph_t*
utf8tog (rl_glyph_t* glyphs, char const* raw)
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
          ++raw; /* skip a char of wrong utf8 sequence */
        }
    }
  *glyphs = 0;

  return glyphs;
}

/* -------------------------------------------------------------------------- */
STATIC int
utf8_width (char const* raw)
{
  int count = 0;
  while (*raw)
    {
      if (!utf8_to_glyph (&raw))
        {
          ++raw; /* skip a char of wrong utf8 sequence */
        }
      ++count;
    }

  return count;
}

/* ----------------------------<--------------------------------------------- */
STATIC char*
gtoutf8 (char* raw, rl_glyph_t const* glyphs, int count)
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
STATIC int
skip_char_seq (char const* start)
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

  return 0; /* not closed sequence */
}

/* -------------------------------------------------------------------------- */
STATIC void
rl_move (int count)
{
  if (count < 0)
    {
      while (count++)
        {
          rl_out (CUR_LEFT, strlen (CUR_LEFT));
        }
    }
  else if (count > 0)
    {
      char buf[RL_MAX_LENGTH * 3];
      char* to = gtoutf8 (buf, rl_state->line + rl_state->cur_pos, count);
      if (to != buf)
        {
          rl_out (buf, to - buf);
        }
    }
  return;
}

/* -------------------------------------------------------------------------- */
STATIC void
rl_update_tail (int afterspace)
{
  int c = afterspace;
  char buf[RL_MAX_LENGTH * 3];
  char* to = gtoutf8 (buf, rl_state->line + rl_state->cur_pos, -1);

  while (c--)
    {
      *to++ = ' ';
    }
  *to = 0;
  if (to != buf)
    {
      rl_out (buf, to - buf);
    }
  c = afterspace + rl_state->length - rl_state->cur_pos;
  if (c > 0)
    {
      rl_state->cur_pos += c;
      rl_move (-c);
      rl_state->cur_pos -= c;
    }
}

/* -------------------------------------------------------------------------- */
STATIC void
rl_write_part (int start, int length)
{
  char buf[RL_MAX_LENGTH * 2];
  char* to = gtoutf8 (buf, rl_state->line + start, length);

  if (to != buf)
    {
      rl_out (buf, to - buf);
    }
}

/* -------------------------------------------------------------------------- */
STATIC void
rl_redraw (int inplace, int tail)
{
  if (inplace)
    {
      rl_move (-(rl_state->cur_pos + rl_state->prompt_width));
    }

  rl_out (rl_state->prompt, strlen (rl_state->prompt));
  rl_write_part (0, rl_state->cur_pos);
  rl_update_tail (tail);
}

/* -------------------------------------------------------------------------- */
STATIC void
rlc_cursor_home ()
{
  rl_move (-rl_state->cur_pos);
  rl_state->cur_pos = 0;
}

/* -------------------------------------------------------------------------- */
STATIC void
rl_set_text (char const* text, int redraw)
{
  if (redraw)
    {
      rlc_cursor_home ();
    }

  int oldlen = rl_state->length;

  strncpy (rl_state->raw, text, rl_state->raw_len - 1);
  rl_state->raw[rl_state->raw_len - 1] = 0;
  rl_glyph_t* end = utf8tog (rl_state->line, rl_state->raw);
  rl_state->length = rl_state->cur_pos = end - rl_state->line;

  if (redraw)
    {
      rl_write_part (0, rl_state->length);
      if (oldlen > rl_state->length)
        {
          rl_update_tail (oldlen - rl_state->length);
        }
    }
}

/* -------------------------------------------------------------------------- */
STATIC void
history_pop (int idx)
{
  rl_history_t* h = &rl_state->history;

  if (idx == h->size)
    {
      if (h->line)
        {
          rl_set_text (h->line, 1);
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
      gtoutf8 (rl_state->raw, rl_state->line, -1);
      h->line = strdup (rl_state->raw);
    }

  rl_set_text (h->lines[idx], 1);
}

/* -------------------------------------------------------------------------- */
STATIC void
rlc_cursor_end ()
{
  rl_write_part (rl_state->cur_pos, -1);
  rl_state->cur_pos = rl_state->length;
}

/* -------------------------------------------------------------------------- */
STATIC void
rlc_cursor_left ()
{
  if (rl_state->cur_pos)
    {
      rl_move (-1);
      --rl_state->cur_pos;
    }
}

/* -------------------------------------------------------------------------- */
STATIC void
rlc_cursor_right ()
{
  if (rl_state->cur_pos < rl_state->length)
    {
      rl_write_part (rl_state->cur_pos++, 1);
    }
}

/* -------------------------------------------------------------------------- */
STATIC void
rlc_cursor_word_left ()
{
  if (!rl_state->cur_pos)
    {
      return;
    }

  unsigned int pos = rl_state->cur_pos;
  while (pos && rl_state->line[pos - 1] == ' ')
    {
      --pos;
    }

  while (pos && rl_state->line[pos - 1] != ' ')
    {
      --pos;

    }

  rl_move (pos - rl_state->cur_pos);
  rl_state->cur_pos = pos;
}

/* -------------------------------------------------------------------------- */
STATIC int
rlc_next_word ()
{
  unsigned int pos = rl_state->cur_pos, length = rl_state->length;

  while (pos < length && rl_state->line[pos] != ' ')
    {
      ++pos;
    }

  while (pos < length && rl_state->line[pos] == ' ')
    {
      ++pos;
    }

  return pos;
}

/* -------------------------------------------------------------------------- */
STATIC void
rlc_cursor_word_right ()
{
  if (rl_state->cur_pos >= rl_state->length)
    {
      return;
    }

  unsigned int pos = rlc_next_word ();
  rl_write_part (rl_state->cur_pos, pos - rl_state->cur_pos);
  rl_state->cur_pos = pos;
}

/* -------------------------------------------------------------------------- */
STATIC void
rlc_delete_n (int count)
{
  if (rl_state->cur_pos < rl_state->length && count)
    {
      int tail = (rl_state->length - rl_state->cur_pos);
      if (count > tail)
        {
          count = tail;
        }
      memmove (
          rl_state->line + rl_state->cur_pos,
          rl_state->line + rl_state->cur_pos + count,
          (rl_state->length - rl_state->cur_pos - count + 1)
              * sizeof(rl_glyph_t));
      rl_state->length -= count;
      rl_update_tail (count);
    }
}

/* -------------------------------------------------------------------------- */
STATIC void
rlc_delete ()
{
  rlc_delete_n (1);
}

/* -------------------------------------------------------------------------- */
STATIC void
rlc_backspace ()
{
  if (rl_state->cur_pos)
    {
      rl_move (-1);
      --rl_state->cur_pos;
      rlc_delete_n (1);
    }
}

/* -------------------------------------------------------------------------- */
STATIC void
rlc_backword ()
{
  int end = rl_state->cur_pos;
  rlc_cursor_word_left ();
  rlc_delete_n (end - rl_state->cur_pos);
}

/* -------------------------------------------------------------------------- */
STATIC void
rlc_delete_word ()
{
  unsigned int end = rlc_next_word ();
  rlc_delete_n (end - rl_state->cur_pos);
}

/* -------------------------------------------------------------------------- */
STATIC void
rlc_delete_to_begin ()
{
  int len = rl_state->cur_pos;
  rlc_cursor_home ();
  rlc_delete_n (len);
}

/* -------------------------------------------------------------------------- */
STATIC void
rlc_delete_to_end ()
{
  rlc_delete_n (rl_state->length - rl_state->cur_pos);
}

/* -------------------------------------------------------------------------- */
STATIC void
rlc_history_back ()
{
  rl_history_t* h = &rl_state->history;
  if (h->current)
    {
      history_pop (--(h->current));
    }
}

/* -------------------------------------------------------------------------- */
STATIC void
rlc_history_forward ()
{
  rl_history_t* h = &rl_state->history;
  if (h->current < h->size)
    {
      history_pop (++(h->current));
    }
}

/* -------------------------------------------------------------------------- */
STATIC void
rlc_history_begin ()
{
  rl_history_t* h = &rl_state->history;
  if (h->current)
    {
      history_pop ((h->current = 0));
    }
}

/* -------------------------------------------------------------------------- */
STATIC void
rlc_history_end ()
{
  rl_history_t* h = &rl_state->history;
  if (h->current < h->size)
    {
      history_pop ((h->current = h->size));
    }
}

/* -------------------------------------------------------------------------- */
STATIC void
rlc_enter ()
{
  rl_state->finish = 1;
}

/* -------------------------------------------------------------------------- */
STATIC void
rlc_autocomplete ()
{
  if (!rl_state->_get_completion)
    {
      return;
    }

  char* start = rl_state->raw;
  char* cur_pos = gtoutf8 (start, rl_state->line, rl_state->cur_pos);
  gtoutf8 (cur_pos, rl_state->line + rl_state->cur_pos,
           rl_state->length - rl_state->cur_pos);

  char const* insert = (rl_state->_get_completion) (start, cur_pos);
  if (insert)
    {
      rl_insert_seq (insert);
    }
}

/* -------------------------------------------------------------------------- */
static const struct _rl_command rl_commands[] =
  {
    { "\001", rlc_cursor_home },
    { "\002", rlc_cursor_left },
    { "\006", rlc_cursor_right },
    { "\005", rlc_cursor_end },
    { "\033b", rlc_cursor_word_left },
    { "\033f", rlc_cursor_word_right },
    { "\010", rlc_backspace },
    { "\004", rlc_delete },
    { "\027", rlc_backword },
    { "\033d", rlc_delete_word },
    { "\013", rlc_delete_to_end },
    { "\025", rlc_delete_to_begin },
    { "\t", rlc_autocomplete },
    { "\020", rlc_history_back },
    { "\016", rlc_history_forward },
    { "\033<", rlc_history_begin },
    { "\033>", rlc_history_end },

  /* VT100 */
    { "\033OH", rlc_cursor_home },
    { "\033OF", rlc_cursor_end },
    { "\033[A", rlc_history_back },
    { "\033[B", rlc_history_forward },
    { "\033[D", rlc_cursor_left },
    { "\033[C", rlc_cursor_right },
    { "\033[1;5D", rlc_cursor_word_left },
    { "\033[1;5C", rlc_cursor_word_right },
    { "\033[3~", rlc_delete },
    { "\x7F", rlc_backspace },

  /* PuTTY */
    { "\033[1~", rlc_cursor_home },
    { "\033[4~", rlc_cursor_end },
    { "\033OD", rlc_cursor_word_left },
    { "\033OC", rlc_cursor_word_right },

  /* Hyper Terminal */
    { "\033[H", rlc_cursor_home },
    { "\033[K", rlc_cursor_end },

  /* VT52 */
    { "\033H", rlc_cursor_home },
    { "\033A", rlc_history_back },
    { "\033B", rlc_history_forward },
    { "\033D", rlc_cursor_left },
    { "\033C", rlc_cursor_right },
    { "\033K", rlc_delete_to_end },

    { "\n", rlc_enter },
    { "\r", rlc_enter } };

/* -------------------------------------------------------------------------- */
void
rl_insert_seq (char const* seq)
{
  rl_glyph_t uc[RL_MAX_LENGTH * 2];
  rl_glyph_t* end = utf8tog (uc, seq);
  int count = end - uc;
  int max_count = countof(rl_state->line) - rl_state->length - 1;

  if (count > max_count)
    {
      count = max_count;
    }

  if (!count)
    {
      return;
    }

  if (rl_state->length - rl_state->cur_pos)
    {
      memmove (
          rl_state->line + rl_state->cur_pos + count,
          rl_state->line + rl_state->cur_pos,
          sizeof(rl_state->line[0]) * (rl_state->length - rl_state->cur_pos));
    }

  memcpy (rl_state->line + rl_state->cur_pos, uc, sizeof(uc[0]) * count);

  rl_state->length += count;
  rl_state->line[rl_state->length] = 0;

  rl_write_part (rl_state->cur_pos, count);
  rl_state->cur_pos += count;
  rl_update_tail (0);
}

/* -------------------------------------------------------------------------- */
STATIC int
rl_exec_seq (char* seq)
{
  const struct _rl_command* cmd = rl_commands, * end = rl_commands
      + countof(rl_commands);

  while (cmd < end)
    {
      if (!strcmp (cmd->seq, seq))
        {
          cmd->handler ();
          goto _exit;
        }
      ++cmd;
    }

  if (seq[0] & 0xE0)
    {
      rl_insert_seq (seq);
    }

_exit: return rl_state->finish;
}

/* -------------------------------------------------------------------------- */
STATIC void
history_save ()
{
  char const* file = rl_state->history.file;

  if (!file)
    {
      return;
    }

  int fd = open (file, O_CREAT | O_WRONLY | O_TRUNC, 0644);
  if (fd < 0)
    return;

  rl_history_t* h = &rl_state->history;
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
STATIC void
history_empty ()
{
  rl_history_t* h = &rl_state->history;
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
STATIC void
history_add (char const* string)
{
  rl_history_t* h = &rl_state->history;

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
readline_history_load (char const* file)
{
  if (!file)
    {
      file = RL_HISTORY_FILE;
    }

  rl_state->history.file = file;

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
readline_init (rl_get_completion_fn* gc)
{
  if (rl_state)
    {
      readline_free ();
    }

  rl_state = (rl_state_t*) malloc (sizeof(rl_state_t));
  memset (rl_state, 0, sizeof(*rl_state));
  rl_state->_get_completion = gc;
}

/* -------------------------------------------------------------------------- */
void
readline_free ()
{
  if (!rl_state)
    {
      return;
    }

  history_save ();
  history_empty ();
  free (rl_state);
  rl_state = NULL;
}

#ifdef RL_SORT_HINTS
static int rl_strscmp(void const *l, void const *r)
{
	return strcmp(*(char const *const*)l, *(char const *const*)r);
}
#endif

/* -------------------------------------------------------------------------- */
void
rl_dump_options (char const*const* options)
{
  if (!*options) /* empty list */
    return;

  char const*const* opt = options;
  int col_width = 0, width;
  while (*opt)
    if ((width = strlen (*opt++)) > col_width)
      col_width = width;

#ifdef RL_SORT_HINTS
	qsort((void *)options, opt - options, sizeof(char *), rl_strscmp);
#endif

  col_width += 2;
  int cols = RL_WINDOW_WIDTH / col_width;
  if (!cols)
    cols = 1;

  int cur_pos = rl_state->cur_pos;
  rlc_cursor_end ();
  rl_printf ("\r\n");

  opt = options;
  while (*opt)
    {
      int c = cols;
      do
        {
          rl_printf ("%-*s", col_width, *opt++);
        }
      while (*opt && --c);
      rl_printf ("\r\n");
    }
  rl_state->cur_pos = cur_pos;
  rl_redraw (0, 0); /* not in place */
}

/* -------------------------------------------------------------------------- */
void
rl_dump_hint (char const* fmt, ...)
{
  va_list va;
  va_start(va, fmt);
  char outbuf[4096];
  int length = vsnprintf (outbuf, sizeof(outbuf), fmt, va);
  va_end(va);

  int cur_pos = rl_state->cur_pos;
  rlc_cursor_end ();
  rl_printf ("\n\r");

  rl_out (outbuf, length);
  rl_printf ("\n\r");
  rl_state->cur_pos = cur_pos;
  rl_redraw (0, 0); /* not in place */
}

/* -------------------------------------------------------------------------- */
int
readline (os::posix::tty_canonical* tty, char const* prompt, void* buff,
          size_t len)
{
  rl_state->raw[0] = 0;
  rl_state->line[0] = 0;
  rl_state->length = rl_state->cur_pos = rl_state->finish = 0;
  rl_state->prompt = prompt;
  rl_state->prompt_width = utf8_width (prompt);
  rl_state->tty = tty;
  rl_state->raw = (char*) buff;
  rl_state->raw_len = len;

  rl_printf ("%s", prompt);

  char ch, seq[12], * seqpos = seq;
  while (tty->read (&ch, 1) > 0)
    {
      if (seqpos >= seq + sizeof(seq))
        {
          seqpos = seq; // wrong sequence -- wrong reaction :)
        }
      *seqpos++ = ch;
      *seqpos = 0;
      if (!skip_char_seq (seq))
        {
          continue; /* wrong seq */
        }
      if (rl_exec_seq (seq))
        {
          break; /* finish */
        }
      seqpos = seq;
    }

  rlc_cursor_end ();
  gtoutf8 (rl_state->raw, rl_state->line, -1);

  history_add (rl_state->raw);
  rl_printf ("\n");
  return strlen (rl_state->raw);
}

