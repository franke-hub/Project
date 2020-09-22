//----------------------------------------------------------------------------
//
//       Copyright (C) 2020 Frank Eskesen.
//
//       This file is free content, distributed under the GNU General
//       Public License, version 3.0.
//       (See accompanying file LICENSE.GPL-3.0 or the original
//       contained within https://www.gnu.org/licenses/gpl-3.0.en.html)
//
//----------------------------------------------------------------------------
//
// Title-
//       Xcb/Active.cpp
//
// Purpose-
//       Implement Active.h
//
// Last change date-
//       2020/09/06
//
//----------------------------------------------------------------------------
#include <string.h>                 // For memcpy, memmove, strlen

#include <pub/Debug.h>              // For Debug object
#include <pub/Must.h>               // For Must methods

#include "Xcb/Global.h"             // For opt_hcdm, opt_verbose
#include "Xcb/Active.h"             // Implementation class

using namespace pub::debugging;     // For debugging subroutines
using namespace pub;                // For Must::

//----------------------------------------------------------------------------
// Constants for parameterization
//----------------------------------------------------------------------------
enum { BUFFER_SIZE= 2048 };         // Default buffer/expansion size (2**N)

namespace xcb {
//----------------------------------------------------------------------------
//
// Method-
//       xcb::Active::~Active
//
// Purpose-
//       Destructor.
//
//----------------------------------------------------------------------------
   Active::~Active( void )          // Destructor
{
   if( opt_hcdm )
     debugh("Active(%p)::~Active\n", this);

   Must::free(buffer);
}

//----------------------------------------------------------------------------
//
// Method-
//       xcb::Active::Active
//
// Purpose-
//       Constructor.
//
//----------------------------------------------------------------------------
   Active::Active( void )           // Constructor
{
   if( opt_hcdm )
     debugh("Active(%p)::Active\n", this);

   source= "";                      // Initial text
   buffer_size= BUFFER_SIZE;        // Default buffer size
   buffer_used= 0;
   buffer= (char*)Must::malloc(buffer_size);
   fsm= FSM_RESET;
}

//----------------------------------------------------------------------------
//
// Method-
//       xcb::Active::debug
//
// Purpose-
//       Debugging display.
//
//----------------------------------------------------------------------------
void
   Active::debug(                   // Debugging display
     const char*       info) const  // Associated text
{
   debugf("Active(%p)::debug(%s) fsm(%d)\n", this, info ? info : "", fsm);
   debugf("..source(%s)\n", source);
   if( fsm != FSM_RESET ) {
     debugf("..buffer_size(%zd)\n", buffer_size);
     debugf("..buffer_used(%zd)\n", buffer_used);
     buffer[buffer_used]= '\0';     // (Buffer is mutable)
     debugf("..buffer(%s)\n", buffer);
   }
}

//----------------------------------------------------------------------------
//
// Method-
//       xcb::Active::expand
//
// Purpose-
//       Expand the buffer to the appropriate size. (Does not fetch)
//
//----------------------------------------------------------------------------
void
   Active::expand(                  // Expand the buffer
     size_t            column)      // To this length (+1)
{
   if( opt_hcdm )
     debugh("Active(%p)::expand(%zd) [%zd,%zd]\n", this, column, buffer_used, buffer_size);

   if( column >= buffer_size ) {    // If expansion required
     size_t replace_size= column + BUFFER_SIZE + BUFFER_SIZE;
     replace_size &= ~(BUFFER_SIZE - 1);
     char* replace= (char*)Must::malloc(replace_size);
     if( fsm != FSM_RESET )
       memcpy(replace, buffer, buffer_used);
     Must::free(buffer);
     buffer= replace;
     buffer_size= replace_size;
   }
}

//----------------------------------------------------------------------------
//
// Method-
//       xcb::Active::fetch
//
// Purpose-
//       Fetch the line and/or expand the buffer to the appropriate size.
//
//----------------------------------------------------------------------------
void
   Active::fetch(                   // Fetch the text
     size_t            column)      // With blank fill to this length
{
   if( fsm == FSM_RESET )
     buffer_used= strlen(source);

   size_t buffer_need= buffer_used + 1;
   if( column >= buffer_need )
     buffer_need= column + 1;
   if( buffer_need >= buffer_size ) // If expansion required
     expand(buffer_need);

   if( fsm == FSM_RESET ) {         // If not fetched yet
     fsm= FSM_FETCHED;              // Fetch it now
     memcpy(buffer, source, buffer_used);
   }

   if( buffer_used < column ) {     // If expansion required
//   fsm= FSM_CHANGED;              // (Blank fill DOES NOT imply change)
     memset(buffer + buffer_used, ' ', column - buffer_used);
     buffer_used= column;
   }

   if( opt_hcdm )
     debugh("Active(%p)::fetch(%zd) [%zd/%zd]\n", this, column, buffer_used, buffer_size);
}

//----------------------------------------------------------------------------
//
// Method-
//       xcb::Active::append_text
//
// Purpose-
//       Concatenate text substring.
//
//----------------------------------------------------------------------------
void
   Active::append_text(             // Concatenate text
     const char*       join,        // The join substring
     size_t            size)        // The substring size
{
   if( size == 0 )                  // If nothing to insert
     return;                        // (Line unchanged)

   fetch();
   fetch(buffer_used + size + 1);   // Insure room for concatenation
   memcpy(buffer + buffer_used, join, size); // Concatenate
   buffer_used += size;
   fsm= FSM_CHANGED;
}

void
   Active::append_text(             // Concatenate text
     const char*       join)        // The join string
{  size_t L= strlen(join); append_text(join, L); }

//----------------------------------------------------------------------------
//
// Method-
//       xcb::Active::get_buffer
//
// Purpose-
//       (Unconditionally) access the buffer
//
//----------------------------------------------------------------------------
const char*                         // The current buffer
   Active::get_buffer( void ) const // Get '\0' delimited buffer
{
   if( fsm == FSM_RESET )
     return source;

   buffer[buffer_used]= '\0';       // Set string delimiter (buffer mutable)
   return buffer;
}

//----------------------------------------------------------------------------
//
// Method-
//       xcb::Active::get_changed
//
// Purpose-
//       Access the buffer if changed. If unchanged, return nullptr.
//
//----------------------------------------------------------------------------
const char*                         // The changed text, nullptr if unchanged
   Active::get_changed( void ) const // Get changed text string
{
   if( fsm != FSM_CHANGED )         // If unchanged
     return nullptr;

   size_t buffer_used= this->buffer_used; // (For const)
   while( buffer_used > 0 ) {       // Remove trailing blanks
     if( buffer[buffer_used - 1] != ' ' )
       break;

     buffer_used--;
   }

   buffer[buffer_used]= '\0';       // Set string delimiter (buffer mutable)
   return buffer;
}

//----------------------------------------------------------------------------
//
// Method-
//       xcb::Active::get_used
//
// Purpose-
//       Return the buffer length (including trailing blanks, if present)
//
//----------------------------------------------------------------------------
size_t                              // The current buffer used length
   Active::get_used( void )         // Get current buffer used length
{  fetch(); return buffer_used; }

//----------------------------------------------------------------------------
//
// Method-
//       xcb::Active::insert_char
//
// Purpose-
//       Insert a character.
//
//----------------------------------------------------------------------------
void
   Active::insert_char(             // Insert character
     size_t            column,      // The current column
     int               code)        // The insert character
{
   fetch(column + 1);
   memmove(buffer + column + 1, buffer + column, buffer_used - column + 1);
   buffer[column]= code;
   buffer_used++;
   fsm= FSM_CHANGED;
}

//----------------------------------------------------------------------------
//
// Method-
//       xcb::Active::insert_text
//
// Purpose-
//       Insert a text string.
//
//----------------------------------------------------------------------------
void
   Active::insert_text(             // Insert text
     size_t            column,      // The insert column
     const char*       text)        // The insert text
{  replace_text(column, 0, text); }

//----------------------------------------------------------------------------
//
// Method-
//       xcb::Active::remove_char
//
// Purpose-
//       Remove a character.
//
//----------------------------------------------------------------------------
void
   Active::remove_char(             // Remove the character
     size_t            column)      // At this column
{
   fetch();
   if( column > buffer_used )
     return;

   memmove(buffer + column, buffer + column + 1, buffer_used - column + 1);
   if( buffer_used )
     buffer_used--;
   fsm= FSM_CHANGED;
}

//----------------------------------------------------------------------------
//
// Method-
//       xcb::Active::replace_char
//
// Purpose-
//       Replace a character.
//
//----------------------------------------------------------------------------
void
   Active::replace_char(            // Replace the character
     size_t            column,      // At this column
     int               code)        // With this character
{
   fetch(column + 1);
   buffer[column]= code;
   fsm= FSM_CHANGED;
}

//----------------------------------------------------------------------------
//
// Method-
//       xcb::Active::replace_text
//
// Purpose-
//       Replace (or insert) a text string.
//
//----------------------------------------------------------------------------
void
   Active::replace_text(            // Replace (or insert) text
     size_t            column,      // The replacement column
     size_t            length,      // The replacement (delete) length
     const char*       text)        // The replacement (insert) text
{
   fetch(column);                   // Initialize with blank fill

   size_t L= strlen(text);          // The inserted text length
   size_t remain_size= 0;           // Text remaining in buffer
   if( buffer_used - column > length ) // If text will remain
     remain_size= buffer_used - column - length;
   expand(column + L + remain_size); // If necessary, expand the buffer

   if( length && remain_size ) {    // If removing part of text
     memmove(buffer + column + length + remain_size
           , buffer + column + length
           , remain_size);          // Preserve remaining text
     fsm= FSM_CHANGED;
   }

   if( L ) {                        // If inserting text
     memmove(buffer + column, text, L); // Insert the text
     fsm= FSM_CHANGED;
   }

   buffer_used= column + L + remain_size;
}

//----------------------------------------------------------------------------
//
// Method-
//       xcb::Active::reset
//
// Purpose-
//       Reset the Active source string.
//
//----------------------------------------------------------------------------
void
   Active::reset(                   // Set source
     const char*       text)        // To this (immutable) text
{
   source= text;
   fsm= FSM_RESET;
}

//----------------------------------------------------------------------------
//
// Method-
//       xcb::Active::undo
//
// Purpose-
//       Undo any changes.
//
//----------------------------------------------------------------------------
int                                 // Return code: 0 if state changed
   Active::undo( void )             // Undo any changes
{
   int rc= 1;                       // Default, no change
   if( fsm != FSM_RESET ) {         // If something to undo
     fsm= FSM_RESET;
     rc= 0;
   }

   return rc;
}
}  // namespace xcb