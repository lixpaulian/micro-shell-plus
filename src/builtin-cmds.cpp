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
#include <cstdint>
#include <cstdarg>

#include "builtin-cmds.h"

using namespace os;
using namespace os::rtos;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

namespace ushell
{

  ush_version::ush_version (void)
  {
    trace::printf ("%s() %p\n", __func__, this);
    info_.command = "ver";
    info_.help_text = "Show the ushell version";
  }

  ush_version::~ush_version ()
  {
    trace::printf ("%s() %p\n", __func__, this);
  }

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

  ush_ps::ush_ps (void)
  {
    trace::printf ("%s() %p\n", __func__, this);
    info_.command = "ps";
    info_.help_text = "List µOS++ threads";
  }

  ush_ps::~ush_ps ()
  {
    trace::printf ("%s() %p\n", __func__, this);
  }

  /**
   * @brief Dumps to a stream a nice table showing the uOS++ tasks.
   * @param ush: pointer on the parent ushell class.
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
   * @brief Dumps to a stream the thread stats.
   * @param ush: pointer on the parent ushell class.
   * @param th: iterator thread, initially nullptr.
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

  ush_help::ush_help (void)
  {
    trace::printf ("%s() %p\n", __func__, this);
    info_.command = "help";
    info_.help_text = "List available commands";
  }

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
        ush->printf ("  %-10s%s\n", (*pclasses)->get_cmd_info ()->command,
                     (*pclasses)->get_cmd_info ()->help_text);
      }
    ush->printf ("For help on a specific command, type \"<cmd> -h\"\n");

    return ush_ok;
  }

  //----------------------------------------------------------------------------

  ush_quit::ush_quit (void)
  {
    trace::printf ("%s() %p\n", __func__, this);
    info_.command = "exit";
    info_.help_text = "Exit (or restart) the ushell";
  }

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

  ush_test::ush_test (void)
  {
    trace::printf ("%s() %p\n", __func__, this);
    info_.command = "test";
    info_.help_text = "This is a test command";
  }

  ush_test::~ush_test ()
  {
    trace::printf ("%s() %p\n", __func__, this);
  }

  int
  ush_test::do_cmd (class ushell* ush, int argc, char* argv[])
  {
    ush->printf ("Got %d arguments:\n", argc);

    for (int i = 0; i < argc; i++)
      {
        ush->printf ("%s\n", argv[i]);
      }

    return ush_ok;
  }

  //----------------------------------------------------------------------------

  // instantiate all command classes

  ush_version version
    { };

  ush_ps ps
    { };

  ush_quit quit
    { };

  ush_help help
    { };

  ush_test test
    { };

}

#pragma GCC diagnostic pop
