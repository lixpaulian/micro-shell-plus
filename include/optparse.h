/*
 * A C++ equivalent of the Unix getopt() call.
 *
 * This file, as well as the associated code file (optparse.cpp) is based
 * on work done by Christopher Wellons <skeeto>. The original UNLICENSE
 * also applies to this code. See also <https://github.com/skeeto/optparse>.
 *
 * Lix N. Paulian (lix@paulian.net), 6 February 2021
 *
 * The original description follows.
 *
 * Optparse --- portable, reentrant, embeddable, getopt-like option parser
 *
 * This is free and unencumbered software released into the public domain.
 *
 * To get the implementation, define OPTPARSE_IMPLEMENTATION.
 * Optionally define OPTPARSE_API to control the API's visibility
 * and/or linkage (static, __attribute__, __declspec).
 *
 * The POSIX getopt() option parser has three fatal flaws. These flaws
 * are solved by Optparse.
 *
 * 1) Parser state is stored entirely in global variables, some of
 * which are static and inaccessible. This means only one thread can
 * use getopt(). It also means it's not possible to recursively parse
 * nested sub-arguments while in the middle of argument parsing.
 * Optparse fixes this by storing all state on a local struct.
 *
 * 2) The POSIX standard provides no way to properly reset the parser.
 * This means for portable code that getopt() is only good for one
 * run, over one argv with one option string. It also means subcommand
 * options cannot be processed with getopt(). Most implementations
 * provide a method to reset the parser, but it's not portable.
 * Optparse provides an optparse_arg() function for stepping over
 * subcommands and continuing parsing of options with another option
 * string. The Optparse struct itself can be passed around to
 * subcommand handlers for additional subcommand option parsing. A
 * full reset can be achieved by with an additional optparse_init().
 *
 * 3) Error messages are printed to stderr. This can be disabled with
 * opterr, but the messages themselves are still inaccessible.
 * Optparse solves this by writing an error message in its errmsg
 * field. The downside to Optparse is that this error message will
 * always be in English rather than the current locale.
 *
 * Optparse should be familiar with anyone accustomed to getopt(), and
 * it could be a nearly drop-in replacement. The option string is the
 * same and the fields have the same names as the getopt() global
 * variables (optarg, optind, optopt).
 *
 * Optparse also supports GNU-style long options with optparse_long().
 * The interface is slightly different and simpler than getopt_long().
 *
 * By default, argv is permuted as it is parsed, moving non-option
 * arguments to the end. This can be disabled by setting the `permute`
 * field to false after initialization.
 */

#ifndef OPTPARSE_H
#define OPTPARSE_H

#if defined (__cplusplus)

namespace ushell
{

  class opt_parse
  {

  public:

    typedef enum optparse_argtype
    {
      optparse_none, optparse_required, optparse_optional
    } optparse_argtype_t;

    typedef struct optparse_long
    {
      const char* longname;
      int shortname;
      optparse_argtype_t argtype;
    } optparse_long_t;

    opt_parse (int argc, char** argv);

    opt_parse (const opt_parse&) = delete;

    opt_parse (opt_parse&&) = delete;

    opt_parse&
    operator= (const opt_parse&) = delete;

    opt_parse&
    operator= (opt_parse&&) = delete;

    virtual
    ~opt_parse () noexcept;

    /**
     * Read the next option in the argv array.
     * @param optstring a getopt()-formatted option string.
     * @return the next option character, -1 for done, or '?' for error
     *
     * Just like getopt(), a character followed by no colons means no
     * argument. One colon means the option has a required argument. Two
     * colons means the option takes an optional argument.
     */
    int
    optparse (const char* optstring);

    /**
     * Handles GNU-style long options in addition to getopt() options.
     * This works a lot like GNU's getopt_long(). The last option in
     * longopts must be all zeros, marking the end of the array. The
     * longindex argument may be NULL.
     */
    int
    optparse_long (const optparse_long_t* longopts, int* longindex);

    /**
     * Used for stepping over non-option arguments.
     * @return the next non-option argument, or NULL for no more arguments
     *
     * Argument parsing can continue with parse() after using this
     * function. That would be used to parse the options for the
     * subcommand returned by optparse_arg(). This function allows you to
     * ignore the value of optind.
     */
    char*
    optparse_arg (void);

    int argc;
    char** argv;
    bool permute;
    int optind;
    int optopt;
    char* optarg;
    int subopt;
    char errmsg[64];

    //--------------------------------------------------------------------------

  private:

    int
    optparse_error (const char* msg, const char* data);

    bool
    optparse_is_dashdash (const char* arg);

    bool
    optparse_is_shortopt (const char* arg);

    bool
    optparse_is_longopt (const char* arg);

    void
    optparse_permute (int index);

    int
    optparse_argtype (const char* optstring, char c);

    bool
    optparse_longopts_end (const optparse_long_t* longopts, int i);

    void
    optparse_from_long (const optparse_long_t* longopts, char* optstring);

    bool
    optparse_longopts_match (const char* longname, const char* option);

    char*
    optparse_longopts_arg (char* option);

    int
    optparse_long_fallback (const optparse_long_t* longopts, int* longindex);

    static constexpr const char* msg_invalid = "invalid option";
    static constexpr const char* msg_missing = "option requires an argument";
    static constexpr const char* msg_too_many = "option takes no arguments";

  };
}

#endif /* __cplusplus */

#endif /* OPTPARSE_H */
