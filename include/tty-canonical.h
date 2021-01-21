/*
 * This file is part of the µOS++ distribution.
 *   (https://github.com/micro-os-plus)
 * Copyright (c) 2017 Liviu Ionescu.
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
 */

/*
 * 7/12/2020: Added support for cooked terminal mode, aka "canonical" (LNP).
 * 4/01/2021: re-implemented the read and write calls to support some of the
 * termios flags (LNP).
 */

#ifndef TTY_CANONICAL_H_
#define TTY_CANONICAL_H_

#if defined (__cplusplus)

// ----------------------------------------------------------------------------

#if defined(OS_USE_OS_APP_CONFIG_H)
#include <cmsis-plus/os-app-config.h>
#endif

#include <cmsis-plus/rtos/os.h>
#include <cmsis-plus/posix-io/tty.h>
#include <cmsis-plus/posix/termios.h>

// ----------------------------------------------------------------------------

namespace os
{
  namespace posix
  {
    // ------------------------------------------------------------------------

    class tty_impl;

    // ========================================================================

    class tty_canonical : public tty
    {
      // ----------------------------------------------------------------------

      /**
       * @name Constructors & Destructor
       * @{
       */

    public:

      tty_canonical (tty_impl& impl, const char* name);

      /**
       * @cond ignore
       */

      // The rule of five.
      tty_canonical (const tty_canonical&) = delete;
      tty_canonical (tty_canonical&&) = delete;
      tty_canonical&
      operator= (const tty_canonical&) = delete;
      tty_canonical&
      operator= (tty_canonical&&) = delete;

      /**
       * @endcond
       */

      virtual
      ~tty_canonical () noexcept;

      /**
       * @}
       */

      // ----------------------------------------------------------------------
      /**
       * @name Public Member Functions
       * @{
       */

    public:

      // http://pubs.opengroup.org/onlinepubs/9699919799/functions/tcgetattr.html
      virtual int
      tcgetattr (struct termios* ptio);

      // http://pubs.opengroup.org/onlinepubs/9699919799/functions/tcsetattr.html
      virtual int
      tcsetattr (int options, const struct termios* ptio);

      // http://pubs.opengroup.org/onlinepubs/9699919799/functions/tcflush.html
      virtual int
      tcflush (int queue_selector);

      // http://pubs.opengroup.org/onlinepubs/9699919799/functions/tcsendbreak.html
      virtual int
      tcsendbreak (int duration);

      // http://pubs.opengroup.org/onlinepubs/9699919799/functions/tcdrain.html
      virtual int
      tcdrain (void);

      virtual ssize_t
      read (void* buf, std::size_t nbyte);

      virtual ssize_t
      write (const void* buf, std::size_t nbyte);

      // ----------------------------------------------------------------------
      // Support functions.

      tty_impl&
      impl (void) const;

    private:

      ssize_t
      get_line (void* buf, std::size_t nbyte);

      ssize_t
      process_input (void* buf, ssize_t nbyte);

      void
      echo_char (char c);

      ssize_t
      put_line (const void* buf, std::size_t nbyte);

      static constexpr uint8_t bell = 7;        // bell
      static constexpr uint8_t ctrl_d = 4;      // end of file
      static constexpr uint8_t ctrl_u = 0x15;   // line kill
      static constexpr uint8_t esc = 0x1B;      // escape

      typedef struct
      {
        bool icanon;    // canonicalize input lines
        bool echo;      // enable echoing
        bool echoe;     // echo erase character as BS-SP-BS
      } lflag_t;

      typedef struct
      {
        bool istrip;    // strip 8th bit off chars
        bool icrnl;     // map CR to NL (ala CRMOD) -> replaces CRs with NLs
        bool igncr;     // ignore CR -> CRs are filtered out
        bool inlcr;     // map NL into CR -> replaces NLs with CRs
        bool imaxbel;   // ring bell on input queue full
      } iflag_t;

      typedef struct
      {
        bool opost;   // enable following output processing
        bool onlcr;   // map NL to CR-NL (ala CRMOD) -> replaces CRs with CR/NLs
        bool ocrnl;   // map CR to NL on output -> replaces CRs with NLs
      } oflag_t;

      typedef struct
      {
        uint8_t veof;   // end of file character, default ^D
        uint8_t veol;   // end of line character (CR)
        uint8_t veol2;  // second end of line character (LF)
        uint8_t verase; // erase character, default backspace
        uint8_t vkill;  // kill character, default ^U
      } ctrlc_t;

      lflag_t lf_ =
        { .icanon = false, .echo = false, .echoe = false };

      iflag_t if_ =
        { .istrip = false, .icrnl = false, .igncr = false, .inlcr = false,
            .imaxbel = false };

      oflag_t of_ =
        { .opost = false, .onlcr = false, .ocrnl = false };

      ctrlc_t cc_ =
        { .veof = ctrl_d, .veol = '\r', .veol2 = '\n', .verase = '\b', .vkill =
            ctrl_u };

    };

    // ========================================================================

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"
#if 0
    class tty_impl : public char_device_impl
    {
      // ----------------------------------------------------------------------

      friend class tty_canonical;

      /**
       * @name Constructors & Destructor
       * @{
       */

    public:

      tty_impl (void);

      /**
       * @cond ignore
       */

      // The rule of five.
      tty_impl (const tty_impl&) = delete;
      tty_impl (tty_impl&&) = delete;
      tty_impl&
      operator= (const tty_impl&) = delete;
      tty_impl&
      operator= (tty_impl&&) = delete;

      /**
       * @endcond
       */

      virtual
      ~tty_impl ();

      /**
       * @}
       */

      // ----------------------------------------------------------------------
      /**
       * @name Public Member Functions
       * @{
       */

    public:

      virtual int
      do_tcgetattr (struct termios *ptio) = 0;

      virtual int
      do_tcsetattr (int options, const struct termios *ptio) = 0;

      virtual int
      do_tcflush (int queue_selector) = 0;

      virtual int
      do_tcsendbreak (int duration) = 0;

      virtual int
      do_tcdrain (void) = 0;

      virtual int
      do_isatty (void) final;

      /**
       * @}
       */
    };
#endif
#pragma GCC diagnostic pop

    // ========================================================================

    template<typename T>
      class tty_canonical_implementable : public tty_canonical
      {
        // --------------------------------------------------------------------

      public:

        using value_type = T;

        // --------------------------------------------------------------------

        /**
         * @name Constructors & Destructor
         * @{
         */

      public:

        template<typename ... Args>
          tty_canonical_implementable (const char* name, Args&& ... args);

        /**
         * @cond ignore
         */

        // The rule of five.
        tty_canonical_implementable (const tty_canonical_implementable&) = delete;
        tty_canonical_implementable (tty_canonical_implementable&&) = delete;
        tty_canonical_implementable&
        operator= (const tty_canonical_implementable&) = delete;
        tty_canonical_implementable&
        operator= (tty_canonical_implementable&&) = delete;

        /**
         * @endcond
         */

        virtual
        ~tty_canonical_implementable ();

        /**
         * @}
         */

        // --------------------------------------------------------------------
        /**
         * @name Public Member Functions
         * @{
         */

      public:

        // Support functions.

        value_type&
        impl (void) const;

        /**
         * @}
         */

        // --------------------------------------------------------------------
      protected:

        /**
         * @cond ignore
         */

        value_type impl_instance_;

        /**
         * @endcond
         */
      };

  // ==========================================================================
  } /* namespace posix */
} /* namespace os */

// ===== Inline & template implementations ====================================

namespace os
{
  namespace posix
  {
    // ========================================================================

    inline tty_impl&
    tty_canonical::impl (void) const
    {
      return static_cast<tty_impl&> (impl_);
    }

    // ========================================================================

    template<typename T>
      template<typename ... Args>
        tty_canonical_implementable<T>::tty_canonical_implementable (
            const char* name, Args&& ... args) :
            tty_canonical
              { impl_instance_, name }, //
            impl_instance_
              { std::forward<Args>(args)... }
        {
#if defined(OS_TRACE_POSIX_IO_TTY)
          trace::printf ("tty_canonical_implementable::%s(\"%s\")=@%p\n", __func__, name_,
                         this);
#endif
        }

    template<typename T>
      tty_canonical_implementable<T>::~tty_canonical_implementable ()
      {
#if defined(OS_TRACE_POSIX_IO_TTY)
        trace::printf ("tty_canonical_implementable::%s() @%p %s\n", __func__, this,
                       name_);
#endif
      }

    template<typename T>
      typename tty_canonical_implementable<T>::value_type&
      tty_canonical_implementable<T>::impl (void) const
      {
        return static_cast<value_type&> (impl_);
      }

  // ==========================================================================
  } /* namespace posix */
} /* namespace os */

#endif /* __cplusplus */

#endif /* TTY_CANONICAL_H_ */
