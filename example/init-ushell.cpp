/*
 * init-ushell.cpp
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
 * Created on: 22 Jan. 2021 (LNP)
 *
 * This is an example that describes how to launch the ushell, both statically
 * and dynamically. If the definition STATIC_USHELL is set to true, then a
 * ushell will be statically instantiated and launched, otherwise both the
 * ushell class and its thread will be dynamically instantiated/launched.
 * Obviously, a combination of both methods can be also used.
 *
 */

#include <cmsis-plus/rtos/os.h>
#include <cmsis-plus/posix-io/io.h>

#include "init-ushell.h"

#define STATIC_USHELL false

using namespace os;
using namespace os::rtos;

// forward declaration
static void*
ush_th (void* args);

// set ushell's thread's stack size
static constexpr std::size_t th_stack_size = 4096;

#if STATIC_USHELL == true

ushell::ushell ush_cdc0
  { "/dev/cdc0" };

thread_inclusive<th_stack_size> ush_cdc0_th
  { "ush_cdc0", ush_th, nullptr };

#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

void
init_ush (void)
{
#if STATIC_USHELL == false
  new thread_inclusive<th_stack_size>
    { "ush_cdc0", ush_th, (void*) "/dev/cdc0" };
#endif
}

static void*
ush_th (void* args)
{
#if STATIC_USHELL == true
  while (true)
    {
      ush_cdc0.do_ushell (nullptr);
    }
#else
  ushell::ushell* ushc = new ushell::ushell
    { (char*) args };

  void* ret = ushc->do_ushell (nullptr);
  delete ushc;

  return ret;
#endif
}

#pragma GCC diagnostic pop

