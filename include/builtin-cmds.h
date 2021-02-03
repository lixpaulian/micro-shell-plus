/*
 * builtin-cmds.h
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
 * Created on: 3 Feb 2021 (LNP)
 */

#ifndef INCLUDE_BUILTIN_CMDS_H_
#define INCLUDE_BUILTIN_CMDS_H_

#include "ushell.h"

#if defined (__cplusplus)

namespace ushell
{

  //----------------------------------------------------------------------------

  class ush_version : public ushell_cmd
  {
  public:

    ush_version (void);

    virtual
    ~ush_version () noexcept;

    virtual int
    do_cmd (class ushell* ush, int argc, char* argv[]);

  };

  //----------------------------------------------------------------------------

  class ush_ps : public ushell_cmd
  {
  public:

    ush_ps (void);

    virtual
    ~ush_ps () noexcept;

    virtual int
    do_cmd (class ushell* ush, int argc, char* argv[]);

  private:

    void
    iterate_threads (class ushell* ush, os::rtos::thread* th,
                     unsigned int depth);

  };

  //----------------------------------------------------------------------------

  class ush_help : public ushell_cmd
  {
  public:

    ush_help (void);

    virtual
    ~ush_help () noexcept;

    virtual int
    do_cmd (class ushell* ush, int argc, char* argv[]);

  };

  //----------------------------------------------------------------------------

  class ush_quit : public ushell_cmd
  {
  public:

    ush_quit (void);

    virtual
    ~ush_quit () noexcept;

    virtual int
    do_cmd (class ushell* ush, int argc, char* argv[]);

  };

  //----------------------------------------------------------------------------

  class ush_test : public ushell_cmd
  {
  public:

    ush_test (void);

    virtual
    ~ush_test () noexcept;

    virtual int
    do_cmd (class ushell* ush, int argc, char* argv[]);

  };

}

#endif /* __cplusplus */

#endif /* INCLUDE_BUILTIN_CMDS_H_ */
