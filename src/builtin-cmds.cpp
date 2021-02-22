/*
 * builtin-cmds.cpp
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

#include <cmsis-plus/rtos/os.h>
#include <cmsis-plus/diag/trace.h>

#include "builtin-cmds.h"
#include "rtc-drv.h"
#include "optparse.h"

extern rtc my_rtc;

using namespace os;
using namespace os::rtos;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

namespace ushell
{

  /**
   * @brief Constructor for the "show version" class.
   */
  ush_version::ush_version (void)
  {
    trace::printf ("%s() %p\n", __func__, this);
    info_.command = "ver";
    info_.help_text = "Show the ushell version";
  }

  /**
   * Destructor.
   */
  ush_version::~ush_version ()
  {
    trace::printf ("%s() %p\n", __func__, this);
  }

  /**
   * @brief Implementation of the "show version" command.
   * @param ush: pointer to the ushell class.
   * @param argc: arguments count.
   * @param argv: arguments.
   * @return Result of the command's execution.
   */
  int
  ush_version::do_cmd (class ushell* ush, int argc, char* argv[])
  {
    uint8_t version_major, version_minor, version_patch;

    ush->get_version (version_major, version_minor, version_patch);
    ush->printf ("Version %d.%d.%d\n", version_major, version_minor,
                 version_patch);

    return ush_ok;
  }

  //----------------------------------------------------------------------------

  /**
   * @brief Constructor for the "ps" class.
   */
  ush_ps::ush_ps (void)
  {
    trace::printf ("%s() %p\n", __func__, this);
    info_.command = "ps";
    info_.help_text = "List µOS++ threads";
  }

  /**
   * Destructor.
   */
  ush_ps::~ush_ps ()
  {
    trace::printf ("%s() %p\n", __func__, this);
  }

  /**
   * @brief Implementation of the "ps" (print threads) command.
   * @param ush: pointer to the ushell class.
   * @param argc: arguments count.
   * @param argv: arguments.
   * @return Result of the command's execution.
   */
  int
  ush_ps::do_cmd (class ushell* ush, int argc, char* argv[])
  {
    ush->printf ("%-10.20s\t%s\r\n%-10.20s\t%s\r\n", "Thread Name",
                 "\tState\tPrio\t%CPU\tStack", "===========",
                 "\t=====\t====\t====\t=====");
    iterate_threads (ush, nullptr, 0);

    size_t m_total = memory::default_resource->total_bytes ();
    size_t m_free = memory::default_resource->free_bytes ();

    ush->printf ("Heap: %dKB free out of %dKB available\n", m_free / 1024,
                 m_total / 1024);

    return ush_ok;
  }

  const char* thread_state[] =
    { "Undef", "Ready", "Run", "Wait", "Term", "Dead" };

  /**
   * @brief Dumps to a stream the thread statistics.
   * @param ush: pointer on the parent ushell class.
   * @param th: parent thread, initially nullptr.
   * @param depth: depth to iterate.
   */
  void
  ush_ps::iterate_threads (class ushell* ush, thread* th, unsigned int depth)
  {
    for (auto&& p : scheduler::children_threads (th))
      {
        class thread::stack& stk = p.stack ();
        unsigned int used = static_cast<unsigned int> (stk.size ()
            - stk.available ());
        unsigned int st = static_cast<unsigned int> (p.state ());

        statistics::duration_t thread_cpu_cycles = p.statistics ().cpu_cycles ()
            * 100 / scheduler::statistics::cpu_cycles ();

        ush->printf (
            "%-20s\t%s\t%3u\t%s%2u%%\t%5u\r\n", p.name (), thread_state[st],
            p.priority (), thread_cpu_cycles > 0 ? " " : "<",
            (unsigned int) (thread_cpu_cycles > 0 ? thread_cpu_cycles : 1),
            stk.size () - used);

        iterate_threads (ush, &p, depth + 1);
      }
  }

  //----------------------------------------------------------------------------

  /**
   * @brief Constructor for the "quit" class.
   */
  ush_date::ush_date (void)
  {
    trace::printf ("%s() %p\n", __func__, this);
    info_.command = "date";
    info_.help_text = "Show/set the system date and time";
  }

  /**
   * Destructor.
   */
  ush_date::~ush_date ()
  {
    trace::printf ("%s() %p\n", __func__, this);
  }

  /**
   * @brief Implementation of the "date" command.
   * @param ush: pointer to the ushell class.
   * @param argc: arguments count.
   * @param argv: arguments.
   * @return Result of the command's execution.
   */
  int
  ush_date::do_cmd (class ushell* ush, int argc, char* argv[])
  {
    int result = ush_ok;

    /* get current date and time */
    time_t u_time = time (nullptr);
    struct tm* tdata = localtime (&u_time);

    opt_parse getopt
      { argc, argv };
    int ch;

    while ((ch = getopt.optparse ("c::thu")) != -1)
      {
        switch (ch)
          {
          case 'c':
            if (getopt.optarg == nullptr)
              {
                ush->printf ("Calibration register %d\r\n",
                             my_rtc.get_cal_factor ());
              }
            else
              {
                char* p = getopt.optarg;
                long arg = strtol (getopt.optarg, &p, 0);
                if (arg == 0 && getopt.optarg == p)
                  {
                    result = ush_param_invalid;  // conversion error, exit
                  }
                else
                  {
                    my_rtc.set_cal_factor (atoi (getopt.optarg));
                  }
              }
            break;

          case 't':
            ush->printf ("%s\r\n", getenv ("TZ"));
            break;

          case 'h':
            ush->printf (
                "Usage:\t%s {[-u] | [hh:mm:ss [dd/mm/yy]]}\r\n"
                "\t%s -c [+/-ppm] to show/set the RTC calibration factor\r\n"
                "\t%s -t to show the system time zone\r\n",
                argv[0], argv[0], argv[0]);
            break;

          case 'u':
            tdata = gmtime (&u_time);
            ush->printf ("UTC time:\t%s\r", asctime (tdata));
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
            // no arguments at all, show date/time
            ush->printf ("Local time:\t%s\r", asctime (tdata));
          }
        else if (argc)
          {
            // handle positional arguments: set the time and possibly the date
            sscanf (argv[0], "%d:%d:%d", &tdata->tm_hour, &tdata->tm_min,
                    &tdata->tm_sec);

            if (argc == 2)
              {
                sscanf (argv[1], "%d/%d/%d", &tdata->tm_mday, &tdata->tm_mon,
                        &tdata->tm_year);
                if (tdata->tm_year < 100)
                  {
                    tdata->tm_year += 100;
                  }
                if (tdata->tm_year > 1000)
                  {
                    tdata->tm_year -= 1900;
                  }
                tdata->tm_mon -= 1;
              }
            tdata->tm_isdst = -1;
            u_time = mktime (tdata);

            // time before clock update
            time_t diff = time (nullptr);
            diff = u_time - diff;

            // set the hardware RTC with the new value
            if (my_rtc.set_time (&u_time) == rtc::ok)
              {
                os::rtos::rtclock.offset (
                    u_time - os::rtos::rtclock.steady_now ());
                // up.update (diff); // update uptime counter
              }
          }
      }

    return result;
  }

  //----------------------------------------------------------------------------

  /**
   * @brief Constructor for the "quit" class.
   */
  ush_quit::ush_quit (void)
  {
    trace::printf ("%s() %p\n", __func__, this);
    info_.command = "exit";
    info_.help_text = "Exit (or restart) the ushell";
  }

  /**
   * Destructor.
   */
  ush_quit::~ush_quit ()
  {
    trace::printf ("%s() %p\n", __func__, this);
  }

  int
  ush_quit::do_cmd (class ushell* ush, int argc, char* argv[])
  {
    return ush_exit;
  }

  //----------------------------------------------------------------------------

  /**
   * @brief Constructor for the "help" class.
   */
  ush_help::ush_help (void)
  {
    trace::printf ("%s() %p\n", __func__, this);
    info_.command = "help";
    info_.help_text = "List available commands";
  }

  /**
   * Destructor.
   */
  ush_help::~ush_help ()
  {
    trace::printf ("%s() %p\n", __func__, this);
  }

  int
  ush_help::do_cmd (class ushell* ush, int argc, char* argv[])
  {
    ush->printf ("Following commands are available:\n");
    class ushell_cmd** pclasses;
    for (pclasses = ushell::ushell_cmds_; *pclasses != nullptr; pclasses++)
      {
        ush->printf ("  %s\t%s\n", (*pclasses)->get_cmd_info ()->command,
                     (*pclasses)->get_cmd_info ()->help_text);
      }
    ush->printf ("For help on a specific command, type \"<cmd> -h\"\n");

    return ush_ok;
  }

  //----------------------------------------------------------------------------

#if !defined USH_VERSION_CMD
#define USH_VERSION_CMD true
#endif

#if !defined USH_PS_CMD
#define USH_PS_CMD true
#endif

#if !defined USH_DATE_CMD
#define USH_DATE_CMD true
#endif

#if !defined USH_QUIT_CMD
#define USH_QUIT_CMD true
#endif

#if !defined USH_HELP_CMD
#define USH_HELP_CMD true
#endif

  // instantiate command classes

#if USH_VERSION_CMD == true
  ush_version version
    { };
#endif

#if USH_PS_CMD == true
  ush_ps ps
    { };
#endif

#if USH_DATE_CMD == true
  ush_date date
    { };
#endif

#if USH_QUIT_CMD == true
  ush_quit quit
    { };
#endif

#if USH_HELP_CMD == true
  ush_help help
    { };
#endif

}

#pragma GCC diagnostic pop
