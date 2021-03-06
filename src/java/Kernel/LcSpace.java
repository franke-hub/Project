//----------------------------------------------------------------------------
//
//       Copyright (C) 2010 Frank Eskesen.
//
//       This file is free content, distributed under the GNU General
//       Public License, version 3.0.
//       (See accompanying file LICENSE.GPL-3.0 or the original
//       contained within https://www.gnu.org/licenses/gpl-3.0.en.html)
//
//----------------------------------------------------------------------------
//
// Title-
//       LcSpace.java
//
// Purpose-
//       LoaderControl: Skip space
//
// Last change date-
//       2010/01/01
//
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
//
// Class-
//       LcSpace
//
// Purpose-
//       LoaderControl: Skip space
//
//----------------------------------------------------------------------------
public class LcSpace extends LcOrigin
{
//----------------------------------------------------------------------------
//
// Method-
//       LcSpace.LcSpace
//
// Purpose-
//       Constructor.
//
//----------------------------------------------------------------------------
public
   LcSpace(                         // Constructor
     int               origin)      // Spacement requirement
{
   super(origin);
}

//----------------------------------------------------------------------------
//
// Method-
//       LcSpace.update
//
// Purpose-
//       Update the origin.
//
//----------------------------------------------------------------------------
public void
   update(                          // Update the origin
     Loader            loader)      // The associated Loader
   throws Exception
{
   loader.offset += origin;         // Skip
}
} // Class LcSpace

