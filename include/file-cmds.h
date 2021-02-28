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

  //----------------------------------------------------------------------------

  class ush_mkdir : public ushell_cmd
  {
  public:

    ush_mkdir (void);

    virtual
    ~ush_mkdir () noexcept;

    virtual int
    do_cmd (class ushell* ush, int argc, char* argv[]);

  };

  //----------------------------------------------------------------------------

  class ush_cd : public ushell_cmd
  {
  public:

    ush_cd (void);

    virtual
    ~ush_cd () noexcept;

    virtual int
    do_cmd (class ushell* ush, int argc, char* argv[]);

  };

  //----------------------------------------------------------------------------

  class ush_cp : public ushell_cmd
  {
  public:

    ush_cp (void);

    virtual
    ~ush_cp () noexcept;

    virtual int
    do_cmd (class ushell* ush, int argc, char* argv[]);

  private:

    int
    copy_file (char* src_path, char* dst_path);

  };

  //----------------------------------------------------------------------------

  class ush_pwd : public ushell_cmd
  {
  public:

    ush_pwd (void);

    virtual
    ~ush_pwd () noexcept;

    virtual int
    do_cmd (class ushell* ush, int argc, char* argv[]);

  };

  //----------------------------------------------------------------------------

  class ush_rm : public ushell_cmd
  {
  public:

    ush_rm (void);

    virtual
    ~ush_rm () noexcept;

    virtual int
    do_cmd (class ushell* ush, int argc, char* argv[]);

  private:

    int
    empty_dir (char* path, size_t len);

  };

  //----------------------------------------------------------------------------

  class ush_cat : public ushell_cmd
  {
  public:

    ush_cat (void);

    virtual
    ~ush_cat () noexcept;

    virtual int
    do_cmd (class ushell* ush, int argc, char* argv[]);

  };

  //----------------------------------------------------------------------------

  class ush_fdisk : public ushell_cmd
  {
  public:

    ush_fdisk (void);

    virtual
    ~ush_fdisk () noexcept;

    virtual int
    do_cmd (class ushell* ush, int argc, char* argv[]);

  };

}

#endif /* __cplusplus */

#endif /* INCLUDE_FILE_CMDS_H_ */
