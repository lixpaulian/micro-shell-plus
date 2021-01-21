/*
 * getline.h
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

#ifndef INCLUDE_GETLINE_H_
#define INCLUDE_GETLINE_H_

#include <cmsis-plus/rtos/os.h>
#include <cmsis-plus/posix-io/io.h>
#include <cmsis-plus/posix-io/tty.h>

#if defined (__cplusplus)

class getline;

getline*
open (const char* path, int oflag, ...);


class getline : public os::posix::io
{
public:

  getline (os::posix::io_impl& impl, type t);

  getline (const getline&) = delete;

  getline (getline&&) = delete;

  getline&
  operator= (const getline&) = delete;

  getline&
  operator= (getline&&) = delete;

  virtual
  ~getline () noexcept;

  int
  get_line (const char* prompt, bool secret);

  static constexpr uint8_t backspace = 8;       // backspace
  static constexpr uint8_t tab = 9;             // tab
  static constexpr uint8_t ctrl_c = 3;          // ctrl-C
  static constexpr uint8_t ctrl_x = 0x18;       // ctrl-X
  static constexpr uint8_t ctrl_z = 0x1A;       // ctrl-Z
  static constexpr uint8_t esc = 0x1B;          // escape

  // special characters on input return
  static constexpr int timeout = -2;            // timeout
  static constexpr int user_cancel = -3;        // user cancelled
  static constexpr int up_arrow = -4;           // up arrow key
  static constexpr int down_arrow = -5;         // down arrow key
  static constexpr int right_arrow = -6;        // right arrow key
  static constexpr int left_arrow = -7;         // left arrow key

  // --------------------------------------------------------------------

protected:

  bool echo = true; // this flag should reflect the ECHO bit in the c_lflag in termios.

  // --------------------------------------------------------------------

private:

  int
  put_c (char c);

  int
  get_echo (bool secret);

  char buffer_[256];

};

inline int
getline::put_c (char c)
{
  return this->write (&c, 1);
}

#endif /* __cplusplus */

#endif /* INCLUDE_GETLINE_H_ */
