/*
 * ushell.h
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
 * Created on: 23 November 2020 (LNP)
 */

#ifndef INCLUDE_USHELL_H_
#define INCLUDE_USHELL_H_

#include <cmsis-plus/rtos/os.h>

#include <tty-canonical.h>


#if defined (__cplusplus)

class ushell
{
public:

  ushell (const char* name, const char* char_device);

  ushell (const ushell&) = delete;

  ushell (ushell&&) = delete;

  ushell&
  operator= (const ushell&) = delete;

  ushell&
  operator= (ushell&&) = delete;

  virtual
  ~ushell () noexcept;

  void
  get_version (uint8_t& version_major, uint8_t& version_minor,
               uint8_t& version_patch);


  // --------------------------------------------------------------------

protected:

  os::posix::tty_canonical* tty;

  // --------------------------------------------------------------------

private:

  int
  put_c (char c);

  int
  printf (const char* format, ...);

  int
  cmdExec (char* buff);

  static void*
  ushell_thread (void* args);

  static constexpr uint8_t VERSION_MAJOR = 0;
  static constexpr uint8_t VERSION_MINOR = 0;
  static constexpr uint8_t VERSION_PATCH = 1;

  const char* th_name_;
  const char* char_device_;

  static constexpr std::size_t th_stack_size = 4096;

  os::rtos::thread_inclusive<th_stack_size> th_
    { th_name_, ushell_thread, static_cast<void*> (this) };

};

/**
 * @brief Return the shell's version.
 * @param version_major: major version.
 * @param version_minor: minor version.
 * @param version_patch: patch version.
 */
inline void
ushell::get_version (uint8_t& version_major, uint8_t& version_minor,
                     uint8_t& version_patch)
{
  version_major = VERSION_MAJOR;
  version_minor = VERSION_MINOR;
  version_patch = VERSION_PATCH;
}

inline int
ushell::put_c (char c)
{
  return tty->write (&c, 1);
}

#endif /* __cplusplus */

#endif /* INCLUDE_USHELL_H_ */
