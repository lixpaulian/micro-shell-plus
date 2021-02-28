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
#include <cmsis-plus/posix-io/block-device-partition.h>
#include <cmsis-plus/posix-io/chan-fatfs-file-system.h>

#include <fcntl.h>

#include "file-cmds.h"
#include "optparse.h"
#include "path.h"

using namespace os;
using namespace os::rtos;

extern posix::chan_fatfs_file_system_lockable<rtos::mutex> fat_fs;

namespace ushell
{

#if SHELL_FILE_CMDS == true

#define CWD_BUF_LEN 260
#define FILE_BUFFER 4096

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
            ush->printf ("Usage:\t%s [path]\n", argv[0]);
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

  //----------------------------------------------------------------------------

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
            ush->printf ("Usage:\t%s <path-to-dir>\n", argv[0]);
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

        if (getopt.optind == 1 && argc == 0)
          {
            // no arguments at all, show usage
            ush->printf ("Usage:\t%s <path-to-dir>\n", argv[0]);
          }
        else if (argc)
          {
            // handle positional arguments: get the directory name
            ush->ph.to_absolute (argv[1], path, CWD_BUF_LEN);
            if ((posix::mkdir (path, 0)) < 0)
              {
                ush->printf ("Failed to create the %s directory\n", path);
              }
          }
      }

    return result;
  }

  //----------------------------------------------------------------------------

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
            ush->printf ("Usage:\t%s [path]\n", argv[0]);
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

  /**
   * @brief Constructor for the "copy" class.
   */
  ush_cp::ush_cp (void)
  {
    trace::printf ("%s() %p\n", __func__, this);
    info_.command = "cp";
    info_.help_text = "Copy file";
  }

  /**
   * Destructor.
   */
  ush_cp::~ush_cp ()
  {
    trace::printf ("%s() %p\n", __func__, this);
  }

  /**
   * @brief Implementation of the "copy file(s)" command.
   * @param ush: pointer to the ushell class.
   * @param argc: arguments count.
   * @param argv: arguments.
   * @return Result of the command's execution.
   */
  int
  ush_cp::do_cmd (class ushell* ush, int argc, char* argv[])
  {
    char src[CWD_BUF_LEN + 1] =
      { '\0' };
    char dst[CWD_BUF_LEN + 1] =
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
            ush->printf ("Usage:\t%s <source_file> <target_file>\n", argv[0]);
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
        char* p = argv[0];
        argc -= getopt.optind;
        argv += getopt.optind;

        if (getopt.optind == 1 && argc == 0)
          {
            // not enough arguments, print help again
            ush->printf ("Usage:\t%s <source_file> <target_file>\n", p);
          }
        else if (argc)
          {
            if (argc != 2)
              {
                result = ush_param_invalid;
              }
            else
              {
                // get absolute paths
                ush->ph.to_absolute (argv[0], src, CWD_BUF_LEN);
                ush->ph.to_absolute (argv[1], dst, CWD_BUF_LEN);
                if (ush->ph.is_dir (dst) == 1)
                  {
                    const char* p = ush->ph.file_from_path (src);
                    if ((strlen (dst) + strlen (p) + 1) < CWD_BUF_LEN)
                      {
                        strcat (dst, "/");
                        strcat (dst, p);
                      }
                  }

                // copy file
                if ((copy_file (src, dst)) != 0)
                  {
                    ush->printf ("File copy failed\n");
                  }
              }
          }
      }

    return result;
  }

  /**
   * @brief: Helper function for copy command.
   * @param src_path: source file.
   * @param dst_path: destination.
   * @return 0 if succeeds, negative/positive number otherwise.
   */
  int
  ush_cp::copy_file (char* src_path, char* dst_path)
  {
    static constexpr size_t file_copy_block = 4096;

    int res = -1;
    posix::io* fsrc, * fdst;    // file pointers
    uint8_t* buffer;            // file copy buffer

    if ((buffer = new uint8_t[file_copy_block]))
      {
        // open source file
        fsrc = posix::open (src_path, O_RDONLY);
        if (fsrc)
          {
            // open destination file
            fdst = posix::open (dst_path, O_WRONLY | O_CREAT);
            if (fdst)
              {
                // copy source to destination
                for (;;)
                  {
                    // read a chunk of source file
                    int count = fsrc->read (buffer, file_copy_block);
                    if (count <= 0)
                      {
                        res = count;
                        break;    // EOF or error
                      }
                    // write it to the destination file
                    res = fdst->write (buffer, count);
                    if (res != count)
                      {
                        break; // error or disk full
                      }
                  }
                fdst->close ();
              }
            // close open files
            fsrc->close ();
          }
        delete (buffer);
      }

    return res;
  }

  //----------------------------------------------------------------------------

  /**
   * @brief Constructor for the "pwd" class.
   */
  ush_pwd::ush_pwd (void)
  {
    trace::printf ("%s() %p\n", __func__, this);
    info_.command = "pwd";
    info_.help_text = "Print working directory path";
  }

  /**
   * Destructor.
   */
  ush_pwd::~ush_pwd ()
  {
    trace::printf ("%s() %p\n", __func__, this);
  }

  /**
   * @brief Implementation of the "pwd" command.
   * @param ush: pointer to the ushell class.
   * @param argc: arguments count.
   * @param argv: arguments.
   * @return Result of the command's execution.
   */
  int
  ush_pwd::do_cmd (class ushell* ush, int argc, char* argv[])
  {
    int result = ush_ok;

    opt_parse getopt
      { argc, argv };
    int ch;

    while ((ch = getopt.optparse ("h")) != -1)
      {
        switch (ch)
          {
          case 'h':
            ush->printf ("Usage:\t%s\n", argv[0]);
            break;

          case '?':
            ush->printf ("%s\n", getopt.errmsg);
            result = ush_option_invalid;
            break;
          }
      }

    if (result == ush_ok)
      {
        argc -= getopt.optind;
        argv += getopt.optind;

        if (getopt.optind == 1 && argc == 0)
          {
            ush->printf ("%s\n", ush->ph.get ());
          }
        else if (argc)
          {
            result = ush_param_invalid;
          }
      }

    return result;
  }

  //----------------------------------------------------------------------------

  /**
   * @brief Constructor for the "rm" class.
   */
  ush_rm::ush_rm (void)
  {
    trace::printf ("%s() %p\n", __func__, this);
    info_.command = "rm";
    info_.help_text = "Remove directory entries";
  }

  /**
   * Destructor.
   */
  ush_rm::~ush_rm ()
  {
    trace::printf ("%s() %p\n", __func__, this);
  }

  /**
   * @brief Implementation of the "rm" command.
   * @param ush: pointer to the ushell class.
   * @param argc: arguments count.
   * @param argv: arguments.
   * @return Result of the command's execution.
   */
  int
  ush_rm::do_cmd (class ushell* ush, int argc, char* argv[])
  {
    char path[CWD_BUF_LEN + 1] =
      { '\0' };
    int res = -1;
    int result = ush_ok;

    opt_parse getopt
      { argc, argv };
    int ch;

    while ((ch = getopt.optparse ("h")) != -1)
      {
        switch (ch)
          {
          case 'h':
            ush->printf ("Usage:\t%s <path>\n", argv[0]);
            break;

          case '?':
            ush->printf ("%s\n", getopt.errmsg);
            result = ush_option_invalid;
            break;
          }
      }

    if (result == ush_ok)
      {
        argc -= getopt.optind;
        argv += getopt.optind;

        if (getopt.optind == 1 && argc == 0)
          {
            result = ush_param_invalid;
          }
        else if (argc)
          {
            // get absolute path
            ush->ph.to_absolute (argv[0], path, CWD_BUF_LEN);

            struct stat st_buf;
            if (posix::stat (path, &st_buf) == 0)
              {
                if (st_buf.st_mode & S_IFDIR)
                  {
                    // directory
                    res = empty_dir (path, CWD_BUF_LEN);
                    if (res == 0)
                      {
                        res = posix::rmdir (path);
                      }
                  }
                else
                  {
                    // single file
                    res = posix::unlink (path);
                  }
              }
            if (res < 0)
              {
                ush->printf ("Could not delete file(s)\n");
              }
          }
      }

    return result;
  }

  /**
   * @brief Helper function. It deletes recursively all the files in a directory.
   * @param path: pointer to a buffer containing the directory's path.
   * @param len: length of the buffer.
   * @return 0 if successful, non-zero if it fails.
   */
  int
  ush_rm::empty_dir (char* path, size_t len)
  {
    int res = -1;
    posix::directory* fr;
    struct dirent* dp;
    struct stat st_buf;

    if ((fr = posix::opendir (path)))
      {
        // pointer on end of path; if path ends with no "/", then add one
        char* pp = path + strlen (path);
        if (*(pp - 1) != '/')
          {
            strcat (path, "/");
            pp++;
          }

        for (;;)
          {
            dp = fr->read ();
            if (dp == nullptr)
              {
                *--pp = '\0';
                break;
              }

            strncat (path, dp->d_name, len - strlen (path));
            posix::stat (path, &st_buf);
            if (st_buf.st_mode & S_IFDIR)
              {
                res = empty_dir (path, len);
                if (res == 0)
                  {
                    res = posix::rmdir (path);
                  }
              }
            else
              {
                res = posix::unlink (path);
              }
            if (res < 0)
              {
                break;
              }
            *pp = '\0';
          }

        res = fr->close ();
      }

    return res;
  }

  //----------------------------------------------------------------------------

  /**
   * @brief Constructor for the "cat" class.
   */
  ush_cat::ush_cat (void)
  {
    trace::printf ("%s() %p\n", __func__, this);
    info_.command = "cat";
    info_.help_text = "Dump a file to the console";
  }

  /**
   * Destructor.
   */
  ush_cat::~ush_cat ()
  {
    trace::printf ("%s() %p\n", __func__, this);
  }

  /**
   * @brief Implementation of the "cat" command.
   * @param ush: pointer to the ushell class.
   * @param argc: arguments count.
   * @param argv: arguments.
   * @return Result of the command's execution.
   */
  int
  ush_cat::do_cmd (class ushell* ush, int argc, char* argv[])
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
            ush->printf ("Usage:\t%s <path>\n", argv[0]);
            break;

          case '?':
            ush->printf ("%s\n", getopt.errmsg);
            result = ush_option_invalid;
            break;
          }
      }

    if (result == ush_ok)
      {
        argc -= getopt.optind;
        argv += getopt.optind;

        if (getopt.optind == 1 && argc == 0)
          {
            result = ush_param_invalid;
          }
        else if (argc)
          {
            // convert to absolute path
            ush->ph.to_absolute (argv[0], path, CWD_BUF_LEN);

            posix::io* f;
            if ((f = posix::open (path, O_RDONLY)) == nullptr)
              {
                ush->printf ("File not found\n");
              }
            else
              {
                char* buff;
                if ((buff = new char[FILE_BUFFER]))
                  {
                    size_t count;
                    while ((count = f->read (buff, FILE_BUFFER)) > 0)
                      {
                        ush->printf ("%.*s", count, buff);
                      }
                    ush->printf ("\n");
                    delete (buff);
                  }
                f->close ();
              }
          }
      }

    return result;
  }

  //----------------------------------------------------------------------------

  /**
   * @brief Constructor for the "fdisk" class.
   */
  ush_fdisk::ush_fdisk (void)
  {
    trace::printf ("%s() %p\n", __func__, this);
    info_.command = "fdisk";
    info_.help_text = "Format disk";
  }

  /**
   * Destructor.
   */
  ush_fdisk::~ush_fdisk ()
  {
    trace::printf ("%s() %p\n", __func__, this);
  }

  /**
   * @brief Implementation of the "cat" command.
   * @param ush: pointer to the ushell class.
   * @param argc: arguments count.
   * @param argv: arguments.
   * @return Result of the command's execution.
   */
  int
  ush_fdisk::do_cmd (class ushell* ush, int argc, char* argv[])
  {
    int result = ush_ok;
    bool format = false, done = false;

    opt_parse getopt
      { argc, argv };
    int ch;

    while ((ch = getopt.optparse ("hy")) != -1)
      {
        switch (ch)
          {
          case 'h':
            ush->printf ("Usage:\t%s <path> (currently only \"flash\")\n"
                         "\tuse -y to skip the confirmation prompt\n",
                         argv[0]);
            done = true;
            break;

          case 'y':
            format = true;
            break;

          case '?':
            ush->printf ("%s\n", getopt.errmsg);
            result = ush_option_invalid;
            break;
          }
      }

    if (result == ush_ok && !done)
      {
        argc -= getopt.optind;
        argv += getopt.optind;

        if (argc == 0)
          {
            result = ush_param_invalid;
          }
        else if (argc)
          {
            if (format == false)
              {
                ush->printf ("The data on the disk %s will be lost; "
                             "are you sure you want to proceed? (y/n): ",
                             argv[0]);

                int c = ush->getchar ();
                ush->printf ("%c\n", c);

                if (toupper (c) == 'Y')
                  {
                    format = true;
                  }
              }
            if (format)
              {
                int res = 0;
                if (!strcasecmp (argv[0], "flash"))
                  {
                    fat_fs.umount ();

                    fat_fs.device ().open ();
                    size_t bs = fat_fs.device ().block_logical_size_bytes ();
                    uint8_t* buff = new uint8_t[bs];
                    res = fat_fs.mkfs (FM_FAT | FM_SFD, 0, 0, buff, bs);
                    fat_fs.device ().close ();
                    delete buff;

                    if (res >= 0)
                      {
                        if (fat_fs.mount ("/flash/") < 0)
                          {
                            ush->printf ("Failed to mount /%s\n", argv[0]);
                          }
                        else
                          {
                            ush->printf ("Disk /%s erased\n", argv[0]);
                          }
                      }
                    else
                      {
                        ush->printf ("Failed to format the /%s disk\n",
                                     argv[0]);
                      }
                  }
//                else if ()
//                  {
//                    //...    for other file systems
//                  }
                else
                  {
                    result = ush_param_invalid;
                  }
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

  ush_cp cp
    { };

  ush_pwd pwd
    { };

  ush_rm rm
    { };

  ush_cat cat
    { };

  ush_fdisk fdisk
    { };

#endif

}
