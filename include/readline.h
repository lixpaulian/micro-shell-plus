/*
 * readline.h
 *
 * Copyright (c) 2021 Lix N. Paulian (lix@paulian.net)
 *
 * This file, as well as the associated code file (readline.cpp) is based
 * on work done by Vladimir Antonov. The original MIT License also applies to
 * this code. See also <https://github.com/Holixus/readline>.
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

#ifndef READLINE_H_
#define READLINE_H_

// TODO: temporary, replace with "tty.h" after termios support is added to µOS++
#include "tty-canonical.h"
#include "ushell-opts.h"

#if defined (__cplusplus)

#if !defined SHELL_UTF8_SUPPORT
#define SHELL_UTF8_SUPPORT true
#endif

#if !defined SHELL_MAX_LINE_LEN
#define SHELL_MAX_LINE_LEN 256
#endif

namespace ushell
{

  class read_line
  {

  public:

    typedef
    char const*
    (rl_get_completion_fn) (char const* start, char const* cur_pos);

    read_line (rl_get_completion_fn gc);

    read_line (const read_line&) = delete;

    read_line (read_line&&) = delete;

    read_line&
    operator= (const read_line&) = delete;

    read_line&
    operator= (read_line&&) = delete;

    virtual
    ~read_line () noexcept;

    void
    init (os::posix::tty_canonical* tty, char const* file);

    int
    readline (char const* prompt, void* buff, size_t len);

    void
    end (void);

  private:

    typedef unsigned int rl_glyph_t;

    bool
    exec_seq (char* seq);

    void
    history_add (char const* string);

    int
    out (const char* data, int size);

    rl_glyph_t
    utf8_to_glyph (char const** utf8);

    rl_glyph_t*
    utf8tog (rl_glyph_t* glyphs, char const* raw);

    int
    utf8_width (char const* raw);

    int
    one_gtoutf8 (char* raw, rl_glyph_t glyph);

    char*
    gtoutf8 (char* raw, rl_glyph_t const* glyphs, int count);

    int
    skip_char_seq (char const* start);

    void
    move (int count);

    void
    update_tail (int afterspace);

    void
    write_part (int start, int length);

    void
    set_text (char const* text, int redraw);

    void
    insert_seq (char const* seq);

    int
    next_word (void);

    void
    delete_n (int count);

    typedef struct hist_header
    {
      int16_t prev;
      int16_t next;
    } hh_t;

    char history_[1024];
    char* current_ = history_;

    char* raw_ = nullptr; // raw buffer, utf-8
    size_t raw_len_ = 0;  // length of the raw buffer

#if SHELL_UTF8_SUPPORT == true
    rl_glyph_t line_[SHELL_MAX_LINE_LEN]; // unicode buffer
#endif
    int length_ = 0;    // length in glyphs
    int cur_pos_ = 0;   // position in glyphs
    bool finish_ = false;

    os::posix::tty_canonical* tty_ = nullptr;

    rl_get_completion_fn* get_completion_;

    static constexpr const char* bs = "\b";

    //--------------------------------------------------------------------------

    static void
    cursor_home (class read_line* rl);

    static void
    cursor_end (class read_line* rl);

    static void
    cursor_left (class read_line* rl);

    static void
    cursor_right (class read_line* rl);

    static void
    cursor_word_left (class read_line* rl);

    static void
    cursor_word_right (class read_line* rl);

    static void
    delete_one (class read_line* rl);

    static void
    backspace (class read_line* rl);

    static void
    backword (class read_line* rl);

    static void
    delete_word (class read_line* rl);

    static void
    delete_to_begin (class read_line* rl);

    static void
    delete_to_end (class read_line* rl);

    static void
    history_back (class read_line* rl);

    static void
    history_forward (class read_line* rl);

    static void
    history_begin (class read_line* rl);

    static void
    history_end (class read_line* rl);

    static void
    enter (class read_line* rl);

    static void
    autocomplete (class read_line* rl);

    typedef struct _rl_command
    {
      char const seq[8];
      void
      (*handler) (class read_line*);
    } rl_command_t;

    static constexpr rl_command_t rl_commands[] =
      {
      // generic
            { "\001", cursor_home },
            { "\002", cursor_left },
            { "\006", cursor_right },
            { "\005", cursor_end },
            { "\033b", cursor_word_left },
            { "\033f", cursor_word_right },
            { "\010", backspace },
            { "\004", delete_one },
            { "\027", backword },
            { "\033d", delete_word },
            { "\013", delete_to_end },
            { "\025", delete_to_begin },
            { "\t", autocomplete },
            { "\020", history_back },
            { "\016", history_forward },
            { "\033<", history_begin },
            { "\033>", history_end },

          // VT100
            { "\033OH", cursor_home },
            { "\033OF", cursor_end },
            { "\033[A", history_back },
            { "\033[B", history_forward },
            { "\033[D", cursor_left },
            { "\033[C", cursor_right },
            { "\033[1;5D", cursor_word_left },
            { "\033[1;5C", cursor_word_right },
            { "\033[3~", delete_one },
            { "\x7F", backspace },

          // PuTTY
            { "\033[1~", cursor_home },
            { "\033[4~", cursor_end },
            { "\033OD", cursor_word_left },
            { "\033OC", cursor_word_right },

          // Hyper Terminal
            { "\033[H", cursor_home },
            { "\033[K", cursor_end },

          // VT52
            { "\033H", cursor_home },
            { "\033A", history_back },
            { "\033B", history_forward },
            { "\033D", cursor_left },
            { "\033C", cursor_right },
            { "\033K", delete_to_end },

            { "\n", enter },
            { "\r", enter },

      };

  };

}

#endif // defined (__cplusplus)

#endif /* READLINE_H_ */
