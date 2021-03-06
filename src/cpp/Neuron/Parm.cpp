//----------------------------------------------------------------------------
//
//       Copyright (c) 2007 Frank Eskesen.
//
//       This file is free content, distributed under the GNU General
//       Public License, version 3.0.
//       (See accompanying file LICENSE.GPL-3.0 or the original
//       contained within https://www.gnu.org/licenses/gpl-3.0.en.html)
//
//----------------------------------------------------------------------------
//
// Title-
//       Parm.cpp
//
// Purpose-
//       Parameter control.
//
// Last change date-
//       2007/01/01
//
//----------------------------------------------------------------------------
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "Dendrite.h"
#include "Master.h"
#include "Neuron.h"

//----------------------------------------------------------------------------
// Constants for parameterization
//----------------------------------------------------------------------------
#ifndef FALSE
  #define FALSE 0
#endif

#ifndef TRUE
  #define TRUE  1
#endif

//----------------------------------------------------------------------------
//
// Subroutine-
//       parseDouble
//
// Purpose-
//       Parse a string, extracting a double value.
//
//----------------------------------------------------------------------------
static double                       // Return value
   parseDouble(                     // Extract double value from string
     const char*       C)           // -> String
{
   double              result;      // Resultant
   double              sign;        // Sign of resultant
   double              divisor;     // Divisor
   int                 c;           // Current character
   int                 decimal;     // TRUE if decimal point found
   int                 exponent;    // Exponent value
   int                 expsign;     // Sign of exponent

   // Handle sign of mantissa
   sign= 1.0;
   if( *C == '-' )
   {
     sign= -1.0;
     C++;
   }
   else if( *C == '+' )
     C++;

   // Insure some number is present
   if( *C == '\0' )
     errno= EINVAL;

   // Extract mantissa
   decimal= FALSE;
   divisor= 1.0;
   result=  0.0;
   for(;;)
   {
     c= *C;
     if( c == '.' )                 // If decimal point
     {
       if( decimal )                // If this is the second one
       {
         errno= EINVAL;
         break;
       }

       decimal= TRUE;
       C++;
       continue;
     }

     if( c < '0' || c > '9' )
       break;

     if( decimal )
       divisor *= 10.0;

     result *= 10.0;
     result += c - '0';
     C++;
   }

   // Extract exponent
   if( toupper(c) == 'E' )
   {
     C++;
     expsign= 1;
     if( *C == '-' )
     {
       expsign= -1;
       C++;
     }
     else if( *C == '+' )
       C++;

     exponent= 0;
     for(;;)
     {
       c= *C;
       if( c < '0' || c > '9' )
         break;

       exponent *= 10;
       exponent += c - '0';
       C++;
     }

     if( expsign < 0 )
       divisor *= pow(10.0, exponent);
     else
       divisor /= pow(10.0, exponent);
   }
   if( c != '\0' )
     errno= EINVAL;

   return (sign*result)/divisor;
}

//----------------------------------------------------------------------------
//
// Subroutine-
//       parseHex
//
// Purpose-
//       Parse a string, extracting a hexidecimal value.
//
//----------------------------------------------------------------------------
static long                         // Resultant value
   parseHex(                        // Convert hexidecimal to long
     const char*       C)           // -> String
{
   unsigned long       result;      // Resultant
   int                 c;           // Current character

   // Insure some number is present
   if( *C == '\0' )
     errno= EINVAL;

   // Skip over 0x prefix, if present
   if( C[0] == '0' && toupper(C[1]) == 'X' )
     C += 2;

   // Extract value
   result= 0;
   for(;;)
   {
     c= toupper(*C);
     if( c >= '0' && c <= '9' )
       c -= '0';

     else if( c >= 'A' && c <= 'F' )
       c= 10 + (c - 'A');

     else if( c == '\0' )
       break;

     else
     {
       errno= EINVAL;
       break;
     }

     if( (result & 0xf0000000) != 0 )
       errno= ERANGE;
     result <<= 4;
     result |= c;

     C++;
   }

   return result;
}

//----------------------------------------------------------------------------
//
// Subroutine-
//       parseLong
//
// Purpose-
//       Parse a string, extracting a long value.
//
//----------------------------------------------------------------------------
static long                         // Resultant value
   parseLong(                       // Convert to long
     const char*       C)           // -> String
{
   long                result;      // Resultant
   int                 c;           // Current character
   int                 sign;        // Sign(result)

   sign= 0;
   if( *C == '+' || *C == '-' )
   {
     sign= 1;
     if( *C == '-' )
       sign= (-1);
     C++;
   }

   // Insure some number is present
   if( *C == '\0' )
     errno= EINVAL;

   // If 0x prefix, extract hexidecimal value
   if( C[0] == '0' && toupper(C[1]) == 'X' )
   {
     if( sign != 0 )                // Hex values cannot be signed
       errno= EINVAL;

     return parseHex(C);
   }

   // Extract value
   result= 0;
   for(;;)
   {
     c= *C;
     if( c < '0' || c > '9' )
     {
       if( c != '\0' )
         errno= EINVAL;
       break;
     }

     result *= 10;
     result += (c - '0');
     if( result < 0 )
       errno= ERANGE;

     C++;
   }

   // Account for sign
   if( sign < 0 )
     result= (-result);

   return result;
}

//----------------------------------------------------------------------------
//
// Subroutine-
//       parmLhex
//
// Purpose-
//       Get long hexidecimal parameter.
//
//----------------------------------------------------------------------------
static inline int                   // TRUE if set
   parmLhex(                        // Get long hexidecimal parameter
     const char*       argument,    // The parameter argument
     const char*       parmName,    // The parameter name
     long*             result)      // Resultant
{
   unsigned            const L= strlen(parmName);

   if( memcmp(argument, parmName, L) != 0 )
     return FALSE;

   errno= 0;
   *result= parseHex(argument+L);
   if( errno != 0 )
     perror(argument);

   return TRUE;
}

//----------------------------------------------------------------------------
//
// Subroutine-
//       parmLdec
//
// Purpose-
//       Get long parameter.
//
//----------------------------------------------------------------------------
static int                          // TRUE if set
   parmLdec(                        // Get long parameter
     const char*       argument,    // The parameter argument
     const char*       parmName,    // The parameter name
     long*             result)      // Resultant
{
   unsigned            const L= strlen(parmName);

   if( memcmp(argument, parmName, L) != 0 )
     return FALSE;

   errno= 0;
   *result= parseLong(argument+L);
   if( errno != 0 )
     perror(argument);

   return TRUE;
}

static inline int                   // TRUE if set
   parmLdec(                        // Get long parameter
     const char*       argument,    // The parameter argument
     const char*       parmName,    // The parameter name
     unsigned long*    result)      // Resultant
{
   return parmLdec(argument, parmName, (long*)result);
}

//----------------------------------------------------------------------------
//
// Subroutine-
//       parmReal
//
// Purpose-
//       Get double parameter.
//
//----------------------------------------------------------------------------
static inline int                   // TRUE if set
   parmReal(                        // Get long parameter
     const char*       argument,    // The parameter argument
     const char*       parmName,    // The parameter name
     double*           result)      // Resultant
{
   unsigned            const L= strlen(parmName);

   if( memcmp(argument, parmName, L) != 0 )
     return FALSE;

   errno= 0;
   *result= parseDouble(argument+L);
   if( errno != 0 )
     perror(argument);

   return TRUE;
}

//----------------------------------------------------------------------------
//
// Subroutine-
//       info
//
// Purpose-
//       Parameter fault exit.
//
//----------------------------------------------------------------------------
static void
   info( void )                     // Parameter fault exit
{
   fprintf(stderr, "Main <options>\n");
   fprintf(stderr, "\tNeuron tester.\n");
   fprintf(stderr, "\n");
   fprintf(stderr, "Options\n");
   fprintf(stderr, "-dendrites:value\n");
   fprintf(stderr, "\tNumber of dendrites.\n");
   fprintf(stderr, "\n");
   fprintf(stderr, "-inputs:value\n");
   fprintf(stderr, "\tNumber of inputs.\n");
   fprintf(stderr, "\n");
   fprintf(stderr, "-neurons:value\n");
   fprintf(stderr, "\tNumber of neurons.\n");
   fprintf(stderr, "\n");
   fprintf(stderr, "-mintrig:value\n");
   fprintf(stderr, "\tMinimum trigger cycle.\n");
   fprintf(stderr, "\n");
   fprintf(stderr, "-maxtrig:value\n");
   fprintf(stderr, "\tMaximum trigger cycle.\n");
   fprintf(stderr, "\n");
   fprintf(stderr, "-load:value\n");
   fprintf(stderr, "\tTrigger load.\n");
   exit(EXIT_FAILURE);
}

//----------------------------------------------------------------------------
//
// Subroutine-
//       parm
//
// Purpose-
//       Parameter analysis
//
//----------------------------------------------------------------------------
extern void parm(int,char*[]);      // (Called from Main.cpp)
extern void
   parm(                            // Parameter analysis
     int               argc,        // Argument count
     char*             argv[])      // Argument array
{
// char*               ptrC;        // Generic pointer to character
   char*               argp;        // Argument pointer
   int                 argi;        // Argument index
// int                 args;        // Argument string length

   int                 error;       // Error count
   int                 verify;      // Verification control

   //-------------------------------------------------------------------------
   // Defaults
   //-------------------------------------------------------------------------
   error=  0;                       // Default, no errors found
   verify= 0;                       // Default, no verification
   memset(master, 0, sizeof(Master));
   master->dPerN=  MASTER_D_PER_N;
   master->iCount= MASTER_ICOUNT;
   master->nCount= MASTER_NCOUNT;

   //-------------------------------------------------------------------------
   // Argument analysis
   //-------------------------------------------------------------------------
   for( argi=1; argi<argc; argi++ ) // Analyze variable controls
   {
     argp= argv[argi];              // Address the parameter
     if( *argp == '-' )             // If this parameter is in switch format
     {
       argp++;                      // Skip over the switch char
       errno= 0;                    // Default, no error
       if( strcmp("verify", argp) == 0 ) // If verify switch
         verify= TRUE;              // Get switch value

       else if( parmLdec(argp, "dendrites:", &master->dPerN) )
         ;

       else if( parmLdec(argp, "inputs:",    &master->iCount) )
         ;

       else if( parmLdec(argp, "neurons:",   &master->nCount) )
         ;

       else if( parmLdec(argp, "mintrig:",   &Neuron::minTrigger) )
         ;

       else if( parmLdec(argp, "maxtrig:",   &Neuron::maxTrigger) )
         ;

       else if( parmLdec(argp, "load:",      &Neuron::load) )
         ;

       else if( parmLdec(argp, "maxcycle:",  &Neuron::maxCycle) )
         ;

       else if( strcmp(argp, "help") == 0 )
         error++;
       else
       {
         error++;
         fprintf(stderr, "Invalid control '%s'\n", argv[argi]);
       }

       if( errno != 0 )
         error++;
     }
     else                           // If not a switch
     {
       error++;
       fprintf(stderr, "Invalid parameter '%s'\n", argv[argi]);
     }
   }

   //-------------------------------------------------------------------------
   // Completion analysis
   //-------------------------------------------------------------------------
   master->aCount= master->nCount + master->iCount;
   if( master->aCount < master->nCount
       || master->aCount < master->iCount )
   {
     error= TRUE;
     fprintf(stderr, "Overflow: aCount(%lu)= nCount(%lu)+iCount(%lu)\n",
             master->aCount, master->nCount, master->iCount);
   }

   master->dCount= master->nCount * master->dPerN;
   if( master->dCount < master->nCount
       || master->dCount < master->dPerN )
   {
     error= TRUE;
     fprintf(stderr, "Overflow: dCount(%lu)= nCount(%lu)*dPerN(%lu)\n",
             master->dCount, master->nCount, master->dPerN);
   }

   if( verify )                     // If verify
   {
     printf("     dPerN: %10lu 0x%.8lx\n", master->dPerN , master->dPerN );
     printf("    iCount: %10lu 0x%.8lx\n", master->iCount, master->iCount);
     printf("    nCount: %10lu 0x%.8lx\n", master->nCount, master->nCount);
     printf("    aCount: %10lu 0x%.8lx\n", master->aCount, master->aCount);
     printf("    dCount: %10lu 0x%.8lx\n", master->dCount, master->dCount);
     printf("\n");
     printf("minTrigger: %10lu\n", Neuron::minTrigger);
     printf("maxTrigger: %10lu\n", Neuron::maxTrigger);
     printf("      load: %10lu\n", Neuron::load);
   }

   if( error != 0 )                 // If error encountered
     info();
}

