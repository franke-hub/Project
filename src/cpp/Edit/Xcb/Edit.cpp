//----------------------------------------------------------------------------
//
//       Copyright (C) 2020-2021 Frank Eskesen.
//
//       This file is free content, distributed under the GNU General
//       Public License, version 3.0.
//       (See accompanying file LICENSE.GPL-3.0 or the original
//       contained within https://www.gnu.org/licenses/gpl-3.0.en.html)
//
//----------------------------------------------------------------------------
//
// Title-
//       Edit.cpp
//
// Purpose-
//       Editor: Command line processor
//
// Last change date-
//       2021/03/02
//
//----------------------------------------------------------------------------
#include <exception>                // For std::exception
#include <string>                   // For std::string

#include <ctype.h>                  // For isprint, toupper
#include <errno.h>                  // For errno
#include <fcntl.h>                  // For O_* constants
#include <getopt.h>                 // For getopt_long
#include <limits.h>                 // For INT_MAX, INT_MIN
#include <semaphore.h>              // For sem_open, sem_close
#include <stdarg.h>                 // For va_list
#include <stdio.h>                  // For printf
#include <stdlib.h>                 // For various
#include <unistd.h>                 // For close, ftruncate
#include <sys/mman.h>               // For mmap, shm_open, ...
#include <sys/stat.h>               // For S_* constants
#include <sys/signal.h>             // For signal, SIGINT, SIGSEGV
#include <sys/types.h>              // For type definitions
#include <xcb/xcb.h>                // For XCB interfaces
#include <xcb/xproto.h>             // For XCB types

#include <gui/Global.h>             // For namespace gui utilities
#include <pub/Debug.h>              // For Debug object
#include <pub/Exception.h>          // For Exception object
#include <pub/Trace.h>              // For Trace object

#include <Config.h>                 // For namespace config
#include <Editor.h>                 // For namespace editor

using pub::Debug;                   // For Debug object
using pub::Trace;                   // For Trace object

using namespace pub::debugging;     // For debugging

//----------------------------------------------------------------------------
// Constants for parameterization
//----------------------------------------------------------------------------
enum // Compilation controls
{  HCDM= false                      // Hard Core Debug Mode?
,  USE_BRINGUP= false               // Extra bringup diagnostics?
,  TRACE_SIZE= 0x01000000           // Default trace table size
}; // Compilation controls

//----------------------------------------------------------------------------
// Internal data areas and constants
//----------------------------------------------------------------------------
static sem_t*          edit_lock= nullptr; // Global semaphore

// Constants
static const char*     const SEM_ID= "/e743e3ac-6816-4878-81a2-b47c9bbc2d37";

//----------------------------------------------------------------------------
// Options
//----------------------------------------------------------------------------
static int             opt_help= false; // --help (or error)
static int             opt_hcdm= false; // Hard Core Debug Mode
static int             opt_index;   // Option index

static int             opt_force= false; // Force editor start?
static const char*     opt_test= nullptr; // The test, if specified
static int             opt_verbose= -1; // Verbosity

static const char*     OSTR= ":";   // The getopt_long optstring parameter
static struct option   OPTS[]=      // The getopt_long longopts parameter
{  {"help",    no_argument,       &opt_help,    true} // --help
,  {"hcdm",    no_argument,       &opt_hcdm,    true} // --hcdm

,  {"force",   no_argument,       &opt_force,   true} // --force
,  {"test",    required_argument, nullptr,      0} // --test {required}
,  {"verbose", optional_argument, &opt_verbose, 0} // --verbose {optional}
,  {0, 0, 0, 0}                     // (End of option list)
};

enum OPT_INDEX                      // Must match OPTS[]
{  OPT_HELP
,  OPT_HCDM

,  OPT_FORCE
,  OPT_TEST
,  OPT_VERBOSE
};

//----------------------------------------------------------------------------
//
// Subroutine-
//       init
//
// Purpose-
//       Initialize
//
//----------------------------------------------------------------------------
static int                          // Return code (0 OK)
   init(int, char**)                // Initialize
//   int               argc,        // Argument count (Unused)
//   char*             argv[])      // Argument array (Unused)
{
   //-------------------------------------------------------------------------
   // Insure that no other Editor instance is running
   if( opt_force )                  // (Error recovery)
     sem_unlink(SEM_ID);            // (Allow exclusive create)

// edit_lock= sem_open(SEM_ID, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 0);
   edit_lock= sem_open(SEM_ID, O_CREAT, S_IRUSR | S_IWUSR, 0);
   if( edit_lock == SEM_FAILED ) {
     fprintf(stderr, "Editor already running, or use --force\n");
     exit(EXIT_FAILURE);
   }

   //-------------------------------------------------------------------------
   // Initialize globals
   setlocale(LC_NUMERIC, "");       // Allows printf("%'d\n", 123456789);
// fprintf(stderr, "%4d Edit::init(%'d) ?commas?\n", __LINE__, 123456789);

   gui::opt_hcdm= opt_hcdm;         // Expose options
   gui::opt_test= opt_test;
   gui::opt_verbose= opt_verbose;

   config::opt_hcdm= opt_hcdm;
   config::opt_test= opt_test;
   config::opt_verbose= opt_verbose;

   return 0;                        // Placeholder
}

//----------------------------------------------------------------------------
//
// Subroutine-
//       term
//
// Purpose-
//       terminate
//
//----------------------------------------------------------------------------
static void
   term( void )                     // Terminate
{
   if( edit_lock != SEM_FAILED ) {
     sem_close(edit_lock);
     sem_unlink(SEM_ID);
   }
}

//----------------------------------------------------------------------------
//
// Subroutine-
//       info
//
// Purpose-
//       Parameter description.
//
//----------------------------------------------------------------------------
static int                          // Return code (Always 1)
   info( void)                      // Parameter description
{
   fprintf(stderr, "%s <options> filename ...\n"
                   "File editor\n\n"
                   "Options:\n"
                   "  --help\tThis help message\n"
                   "  --hcdm\tHard Core Debug Mode\n"

                   "  --verbose\t{=n} Verbosity, default 0\n"
                   , __FILE__
          );

   return 1;
}

//----------------------------------------------------------------------------
//
// Subroutine-
//       to_integer
//
// Purpose-
//       Convert string to integer, handling error cases
//
// Implementation note-
//       Leading or trailing blanks are NOT allowed.
//
//----------------------------------------------------------------------------
static int                          // The integer value
   to_integer(                      // Extract and verify integer value
     const char*       inp)         // From this string
{
   errno= 0;
   char* strend;                    // Ending character
   long value= strtol(inp, &strend, 0);
   if( strend == inp || *inp == ' ' || *strend != '\0' )
     errno= EINVAL;
   else if( value < INT_MIN || value > INT_MAX )
     errno= ERANGE;

   return int(value);
}

//----------------------------------------------------------------------------
//
// Subroutine-
//       parm_int
//
// Purpose-
//       Convert parameter to integer, handling error cases
//
// Implementation note-
//       optarg: The argument string
//       opt_index: The argument index
//
//----------------------------------------------------------------------------
static int                          // The integer value
   parm_int( void )                 // Extract and verify integer value
{
   int value= to_integer(optarg);
   if( errno ) {
     opt_help= true;
     if( errno == ERANGE )
       fprintf(stderr, "--%s, range error: '%s'\n", OPTS[opt_index].name, optarg);
     else if( *optarg == '\0' )
       fprintf(stderr, "--%s, no value specified\n", OPTS[opt_index].name);
     else
       fprintf(stderr, "--%s, format error: '%s'\n", OPTS[opt_index].name, optarg);
   }

   return value;
}

//----------------------------------------------------------------------------
//
// Subroutine-
//       parm
//
// Purpose-
//       Parameter analysis.
//
//----------------------------------------------------------------------------
static int                          // Return code (0 if OK)
   parm(                            // Parameter analysis
     int               argc,        // Argument count
     char*             argv[])      // Argument array
{
   //-------------------------------------------------------------------------
   // Parameter analysis
   //-------------------------------------------------------------------------
   int C;                           // The option character
   while( (C= getopt_long(argc, argv, OSTR, OPTS, &opt_index)) != -1 )
   {
     switch( C )
     {
       case 0:
       {{{{
         switch( opt_index )
         {
           case OPT_HELP:           // These options handled by getopt
           case OPT_HCDM:
           case OPT_FORCE:
             break;

           case OPT_TEST:
             opt_test= optarg;
             break;

           case OPT_VERBOSE:
             if( optarg )
               opt_verbose= parm_int();
             break;

           default:
             fprintf(stderr, "%4d Unexpected opt_index(%d)\n", __LINE__,
                             opt_index);
             break;
         }
         break;
       }}}}

       case ':':
         opt_help= true;
         if( optopt == 0 )
           fprintf(stderr, "%4d Option requires an argument '%s'.\n", __LINE__,
                           argv[optind-1]);
         else
           fprintf(stderr, "%4d Option requires an argument '-%c'.\n", __LINE__,
                           optopt);
         break;

       case '?':
         opt_help= true;
         if( optopt == 0 )
           fprintf(stderr, "%4d Unknown option '%s'.\n", __LINE__,
                           argv[optind-1]);
         else if( isprint(optopt) )
           fprintf(stderr, "%4d Unknown option '-%c'.\n", __LINE__, optopt);
         else
           fprintf(stderr, "%4d Unknown option character '0x%x'.\n", __LINE__,
                           (optopt & 0x00ff));
         break;

       default:
         fprintf(stderr, "%4d ShouldNotOccur ('%c',0x%x).\n", __LINE__,
                         C, (C & 0x00ff));
         break;
     }
   }

   // Return sequence
   int rc= 0;
   if( opt_help )
     rc= info();
   return rc;
}

//----------------------------------------------------------------------------
//
// Subroutine-
//       main
//
// Purpose-
//       Mainline code.
//
//----------------------------------------------------------------------------
extern int                          // Return code
   main(                            // Mainline code
     int               argc,        // Argument count
     char*             argv[])      // Argument array
{
   //-------------------------------------------------------------------------
   // Initialize
   //-------------------------------------------------------------------------
   int rc= parm(argc, argv);        // Argument analysis
   if( rc ) return rc;              // Return if invalid

   rc= init(argc, argv);            // Initialize
   if( rc ) return rc;              // Return if invalid

   Config config(argc, argv);       // Configure
   if( opt_hcdm || opt_verbose >= 0 ) {
     Config::errorf("%s: %s %s\n", __FILE__, __DATE__, __TIME__);
     Config::errorf("--hcdm(%d) --verbose(%d) --force(%d)\n"
                   , opt_hcdm, opt_verbose, opt_force);
   }

   //-------------------------------------------------------------------------
   // Mainline code: Load files
   //-------------------------------------------------------------------------
   try {
     Editor editor(optind, argc, argv);
     editor::start();
     editor::join();
   } catch(pub::Exception& X) {
     debugf("%s\n", std::string(X).c_str());
   } catch(std::exception& X) {
     printf("std::exception.what(%s))\n", X.what());
   } catch(const char* X) {
     printf("catch(const char* '%s')\n", X);
   } catch(...) {
     printf("catch(...)\n");
   }

   //-------------------------------------------------------------------------
   // Terminate
   //-------------------------------------------------------------------------
   term();                          // Termination cleanup
   if( USE_BRINGUP || opt_hcdm || opt_verbose >= 0 )
     printf("Edit completed\n");

   return rc;
}
