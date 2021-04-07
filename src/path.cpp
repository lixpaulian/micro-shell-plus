/*
 * path.cpp
 *
 * Copyright (c) 2018, 2021 Lix N. Paulian (lix@paulian.net)
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
 * Created on: 21 Apr 2018 (LNP)
 */

#include <cmsis-plus/rtos/os.h>
#include <cmsis-plus/posix-io/file-system.h>
#include <cmsis-plus/diag/trace.h>

#include "path.h"

#define PATH_DEBUG false

using namespace os;

namespace ushell
{

  /**
   * @brief Constructor.
   */
  path::path (void)
  {
    trace::printf ("%s() %p\n", __func__, this);
    path_[0] = '\0';
  }

  /**
   * @brief Destructor.
   */
  path::~path ()
  {
    trace::printf ("%s() %p\n", __func__, this);
  }

  /**
   * @brief Set default path.
   * @param path: default path.
   */
  void
  path::set_default (const char* path)
  {
    strncpy (home_path_, path, sizeof(home_path_) - 1);
    strcpy (path_, path);
  }

  /**
   * @brief Get the current path.
   * @return Pointer on the current path.
   */
  char*
  path::get (void)
  {
    return path_;
  }

  /**
   * @brief convert a path to absolute path.
   * @param path: path to convert to absolute; if null, path will be set
   *      to default. If next param is null pointer, the result path will
   *      be set as current path.
   * @param result: if not null pointer, the result path will not replace
   *      the current path, rather will be returned at this pointer.
   * @param len: length of the result buffer.
   */
  void
  path::to_absolute (const char* path, char* result, size_t len)
  {
    char* p;
    size_t l;

    if (path == nullptr)
      {
        // set to home directory (default)
        strcpy (path_, home_path_);
      }
    else
      {
        size_t path_len = strlen (path);
        if (result == nullptr)
          {
            p = path_;
            l = path_max_len;
          }
        else
          {
            p = result;
            l = len;
            strncpy (p, path_, l);
          }

        while (path_len > 0)
          {
            if (!strncmp (path, "../", 3))
              {
                // one level up
                path::back (p);
                path += 3;
                path_len -= 3;
              }
            else if (!strncmp (path, "..", 2))
              {
                // one level up and exit
                path::back (p);
                break;
              }
            else if (!strncmp (path, ".", 1) || !strncmp (path, "./", 2))
              {
                break;
              }
            else if (*path == '/')
              {
                // absolute path
                strncpy (p, path, l);
                break;
              }
            else
              {
                // relative path
                path::forward (path, p, l);
                break;
              }
          }
      }
#if PATH_DEBUG == true
  if (result != nullptr)
    {
      trace::printf ("result: %s\n", result);
    }
  trace::printf ("path_: %s\n", path_);
#endif // PATH_DEBUG
  }

  /**
   * @brief Back a directory level up.
   * @param path: path to back one directory level up.
   */
  void
  path::back (const char* path)
  {
    char* p = (char*) &path[strlen (path) - 1];

    if (p > path && *p == '/')
      {
        p--;
      }

    while (*p != '/' && p > path)
      {
        p--;
      }

    if (p != path)        // root?
      {
        char* q = p - 1;  // no
        while (*q != '/' && q > path)
          {
            q--;
          }
        if (q != path)    // block device name?
          {
            *p = '\0';    // no
          }
        else
          {
            *++p = '\0';  // yes, leave terminating slash
          }
      }
    else
      {
        *++p = '\0';      // yes, set to root directory
      }
  }

  /**
   * @brief Concatenate a path with directory and normalize the resulting path.
   * @param level: current path [in].
   * @param result: resulting path [out].
   * @param len: length of the result buffer.
   */
  void
  path::forward (const char* level, char* result,
                 size_t len __attribute__ ((unused)))
  {
    assert(strlen (result) + strlen (level) < (len - 1));

    if (result[strlen (result) - 1] != '/')
      {
        strcat (result, "/");
      }
    strcat (result, level);

    // if the path ends in a slash ('/'), remove it
    if (result[strlen (result) - 1] == '/')
      {
        result[strlen (result) - 1] = '\0';
      }
  }

  /**
   * @brief Check if a path points to a file or a directory.
   * @param path: path to the file or directory to check.
   * @return 1 if directory, 0 if file, -1 if error (e.g. invalid path).
   */
  int
  path::is_dir (const char* path)
  {
    int result;
    struct stat st_buf;

    result = posix::stat (path, &st_buf);
    if (result >= 0)
      {
        result = (st_buf.st_mode & S_IFDIR) ? 1 : 0;
      }

    return result;
  }

  /**
   * @brief Extract the file name from a path.
   * @param path: path including the file name.
   * @return a pointer on the begin of the file name.
   */
  const char*
  path::file_from_path (const char* path)
  {
    const char* p;

    p = path + strlen (path);
    while (*p != '/' && p > path)
      {
        p--;
      }
    if (*p == '/')
      {
        p++;
      }

    return p;
  }

}
