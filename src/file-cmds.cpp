/*
 * file-cmds.cpp
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

#include <cmsis-plus/rtos/os.h>
#include <cmsis-plus/diag/trace.h>
#include <cmsis-plus/posix-io/file-system.h>

#include "file-cmds.h"
#include "optparse.h"
#include "path.h"

using namespace os;
using namespace os::rtos;

namespace ushell
{

#if SHELL_FILE_CMDS == true

#define CWD_BUF_LEN 260

  /**
   * @brief Constructor for the "list files" class.
   */
  ush_ls::ush_ls (void)
  {
    trace::printf ("%s() %p\n", __func__, this);
    info_.command = "ls";
    info_.help_text = "List files";
  }

  /**
   * Destructor.
   */
  ush_ls::~ush_ls ()
  {
    trace::printf ("%s() %p\n", __func__, this);
  }

  /**
   * @brief Implementation of the "list files" command.
   * @param ush: pointer to the ushell class.
   * @param argc: arguments count.
   * @param argv: arguments.
   * @return Result of the command's execution.
   */
  int
  ush_ls::do_cmd (class ushell* ush, int argc, char* argv[])
  {
    char path[CWD_BUF_LEN + 1] =
      { '\0' };
    int res;
    uint32_t free_b, total_b;
    int result = ush_ok;

    opt_parse getopt
      { argc, argv };
    int ch;

    while ((ch = getopt.optparse ("h")) != -1)
      {
        switch (ch)
          {
          case 'h':
            ush->printf ("Usage: %s [path]\n", argv[0]);
            break;

          case '?':
            ush->printf ("%s\n", getopt.errmsg);
            result = ush_option_invalid;
            break;
          }
      }

    if (result == ush_ok)
      {
        // handle the case with no options
        argc -= getopt.optind;
        argv += getopt.optind;

        if (getopt.optind == 1 && argc == 0)
          {
            // no arguments at all, get current path
            strcat (path, ush->ph.get ());
          }
        else if (argc)
          {
            // get the path from user input
            ush->ph.to_absolute (argv[0], path, CWD_BUF_LEN);
          }

        posix::directory* dir = posix::opendir (path);

        if (dir == nullptr)
          {
            ush->printf ("Could not open %s directory\n", path);
          }
        else
          {
            struct dirent* dp;
            struct stat st_buf;
            struct tm tmdata;

            // pointer on end of path; if path ends with no "/", then add one
            char* pp = path + strlen (path) - 1;
            if (*pp != '/')
              {
                strcat (path, "/");
                pp++;
              }
            pp++;

            // iterate through directory
            while (true)
              {
                errno = 0;
                dp = dir->read ();

                if (dp == nullptr)
                  {
                    break;
                  }
                strncat (path, dp->d_name, CWD_BUF_LEN - strlen (path));
                posix::stat (path, &st_buf);
                localtime_r (&st_buf.st_mtime, &tmdata);
                ush->printf ("%c%s %8lu %s %2d %4d %02d:%02d - %s\n",
                             st_buf.st_mode & S_IFDIR ? 'd' : '-',
                             st_buf.st_mode & S_IWUSR ? "r-" : "rw",
                             st_buf.st_size, months[tmdata.tm_mon],
                             tmdata.tm_mday, tmdata.tm_year + 1900,
                             tmdata.tm_hour, tmdata.tm_min, dp->d_name);
                *pp = '\0';
              };

            if ((res = dir->close ()))
              {
                ush->printf ("Error closing the directory\n");
              }

            // retrieve the drive name
            char* p = path + 1; // pointer behind the leading "/"
            while (*p != '/' && *p != '\0')
              {
                p++;
              }
            if (*p == '/')
              {
                p++;
                *p = '\0';
              }

            struct statvfs sfs;
            res = posix::statvfs (path, &sfs);
            if (res < 0)
              {
                ush->printf ("Error getting drive info\n");
              }
            else
              {
                // disk stats
                total_b = sfs.f_blocks * sfs.f_bsize / 1024; // KBytes
                free_b = sfs.f_bavail * sfs.f_bsize / 1024;
                ush->printf (
                    "Capacity: %0.2f GB (%lu KB), available: %0.2f GB (%lu KB).\n",
                    (float) total_b / (1024 * 1024), total_b,
                    (float) free_b / (1024 * 1024), free_b);
              }
          }
      }

    return result;
  }

  /**
   * @brief Constructor for the "make directory" class.
   */
  ush_mkdir::ush_mkdir (void)
  {
    trace::printf ("%s() %p\n", __func__, this);
    info_.command = "mkdir";
    info_.help_text = "Create a new directory";
  }

  /**
   * Destructor.
   */
  ush_mkdir::~ush_mkdir ()
  {
    trace::printf ("%s() %p\n", __func__, this);
  }

  /**
   * @brief Implementation of the "make directory" command.
   * @param ush: pointer to the ushell class.
   * @param argc: arguments count.
   * @param argv: arguments.
   * @return Result of the command's execution.
   */
  int
  ush_mkdir::do_cmd (class ushell* ush, int argc, char* argv[])
  {
    char path[CWD_BUF_LEN + 1] =
      { '\0' };
    int result = ush_ok;

    opt_parse getopt
      { argc, argv };
    int ch;

    while ((ch = getopt.optparse ("h")) != -1)
      {
        switch (ch)
          {
          case 'h':
            ush->printf ("Usage: mkdir <path-to-dir>\n");
            break;

          case '?':
            ush->printf ("%s\n", getopt.errmsg);
            result = ush_option_invalid;
            break;
          }
      }

    if (result == ush_ok)
      {
        // handle the case with no options
        argc -= getopt.optind;
        argv += getopt.optind;

        if (getopt.optind == 1 && argc == 0)
          {
            // no arguments at all, show usage
            ush->printf ("Usage: mkdir <path-to-dir>\n");
          }
        else if (argc)
          {
            // handle positional arguments: get the directory name
            ush->ph.to_absolute (argv[0], path, CWD_BUF_LEN);
            if ((posix::mkdir (path, 0)) < 0)
              {
                ush->printf ("Failed to create the %s directory\n", path);
              }
          }
      }

    return result;
  }

  /**
   * @brief Constructor for the "change directory" class.
   */
  ush_cd::ush_cd (void)
  {
    trace::printf ("%s() %p\n", __func__, this);
    info_.command = "cd";
    info_.help_text = "Change directory";
  }

  /**
   * Destructor.
   */
  ush_cd::~ush_cd ()
  {
    trace::printf ("%s() %p\n", __func__, this);
  }

  /**
   * @brief Implementation of the "change directory" command.
   * @param ush: pointer to the ushell class.
   * @param argc: arguments count.
   * @param argv: arguments.
   * @return Result of the command's execution.
   */
  int
  ush_cd::do_cmd (class ushell* ush, int argc, char* argv[])
  {
    char path[CWD_BUF_LEN + 1] =
      { '\0' };
    int result = ush_ok;

    opt_parse getopt
      { argc, argv };
    int ch;

    while ((ch = getopt.optparse ("h")) != -1)
      {
        switch (ch)
          {
          case 'h':
            ush->printf ("Usage: cd [path]\n");
            break;

          case '?':
            ush->printf ("%s\n", getopt.errmsg);
            result = ush_option_invalid;
            break;
          }
      }

    if (result == ush_ok)
      {
        // handle the case with no options
        argc -= getopt.optind;
        argv += getopt.optind;

        if (getopt.optind == 1 && argc == 0)
          {
            // no arguments at all, change to home directory
            ush->ph.to_absolute (nullptr, nullptr, 0);
          }
        else if (argc)
          {
            ush->ph.to_absolute (argv[0], path, CWD_BUF_LEN);
            if (posix::opendir (path) != nullptr)
              {
                // found, set to new path
                ush->ph.to_absolute (argv[0], nullptr, 0);
              }
            else
              {
                ush->printf ("Path not found\n");
              }
          }
      }

    return result;
  }

  //----------------------------------------------------------------------------

  // instantiate command classes

  ush_ls ls
    { };

  ush_mkdir mkdir
    { };

  ush_cd cd
    { };

#endif

}
