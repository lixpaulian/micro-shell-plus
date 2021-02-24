/*
 * file-cmds.h
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
 * Created on: 24 Feb 2021 (LNP)
 */

#ifndef INCLUDE_FILE_CMDS_H_
#define INCLUDE_FILE_CMDS_H_

#include "ushell.h"

#if defined (__cplusplus)

#if !defined SHELL_FILE_CMDS
#define SHELL_FILE_CMDS true
#endif

namespace ushell
{
  const char* months[] =
    { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct",
        "Nov", "Dec" };

  class ush_ls : public ushell_cmd
  {
  public:

    ush_ls (void);

    virtual
    ~ush_ls () noexcept;

    virtual int
    do_cmd (class ushell* ush, int argc, char* argv[]);

  };

  class ush_mkdir : public ushell_cmd
  {
  public:

    ush_mkdir (void);

    virtual
    ~ush_mkdir () noexcept;

    virtual int
    do_cmd (class ushell* ush, int argc, char* argv[]);

  };

  class ush_cd : public ushell_cmd
  {
  public:

    ush_cd (void);

    virtual
    ~ush_cd () noexcept;

    virtual int
    do_cmd (class ushell* ush, int argc, char* argv[]);

  };

}

#endif /* __cplusplus */

#endif /* INCLUDE_FILE_CMDS_H_ */
