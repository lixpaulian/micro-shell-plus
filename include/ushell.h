/*
 * ushell.h
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
 * Created on: 23 November 2020 (LNP)
 */

#ifndef INCLUDE_USHELL_H_
#define INCLUDE_USHELL_H_

#include <tty-canonical.h> // this is temporary, should be <tty.h>
#include "readline.h"
#include "ushell-opts.h"

#if defined (__cplusplus)

#if !defined SHELL_MAX_LINE_LEN
#define SHELL_MAX_LINE_LEN 256
#endif

#if !defined SHELL_USE_READLINE
#define SHELL_USE_READLINE true
#endif

#if !defined SHELL_MAX_COMMANDS
#define SHELL_MAX_COMMANDS 40
#endif

#if !defined SHELL_FILE_CMDS
#define SHELL_FILE_CMDS true
#endif

#if SHELL_FILE_CMDS == true
#include "path.h"
#endif

namespace ushell
{
  class ushell_cmd;

  typedef enum
  {
    ush_ok = 0,
    ush_cmd_not_found,
    ush_cmd_not_allowed,
    ush_option_invalid,
    unused,
    ush_param_invalid,

    ush_user_timeout = 98,
    ush_exit
  } error_types_t;

  class ushell
  {

  public:

    ushell (const char* char_device);

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

    void*
    do_ushell (void* args);

    static bool
    link_cmd (class ushell_cmd* ucmd);

    int
    printf (const char* format, ...);

    int
    getchar (void);

    static ushell_cmd* ushell_cmds_[SHELL_MAX_COMMANDS];

#if SHELL_FILE_CMDS == true
    path ph
      { };
#endif

    //--------------------------------------------------------------------------

  protected:

    os::posix::tty_canonical* tty;

    //--------------------------------------------------------------------------

  private:

    int
    cmd_parser (char* buff);

#if SHELL_USE_READLINE == true
    read_line rl
      { nullptr };
#endif

    static constexpr uint8_t VERSION_MAJOR = 0;
    static constexpr uint8_t VERSION_MINOR = 2;
    static constexpr uint8_t VERSION_PATCH = 3;

    const char* char_device_;

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

  //----------------------------------------------------------------------------

  class ushell_cmd
  {
  public:

    ushell_cmd (void);

    ushell_cmd (const ushell_cmd&) = delete;

    ushell_cmd (ushell_cmd&&) = delete;

    ushell_cmd&
    operator= (const ushell_cmd&) = delete;

    ushell_cmd&
    operator= (ushell_cmd&&) = delete;

    virtual
    ~ushell_cmd () noexcept;

    virtual int
    do_cmd (class ushell* ush, int argc, char* argv[]) = 0;

  protected:

    friend class ushell;
    friend class ush_help;

    typedef struct
    {
      const char* command;
      const char* help_text;
    } cmd_info_t;

    cmd_info_t*
    get_cmd_info (void);

    cmd_info_t info_;
    class ushell* ush;

  };

}

#endif /* __cplusplus */

#endif /* INCLUDE_USHELL_H_ */
