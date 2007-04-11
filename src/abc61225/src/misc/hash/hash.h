/**CFile****************************************************************

  FileName    [hash.h]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Hash map.]

  Synopsis    [External declarations.]

  Author      [Aaron P. Hurst]
  
  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - May 16, 2005.]

  Revision    [$Id$]

***********************************************************************/
 
#ifndef __HASH_H__
#define __HASH_H__

////////////////////////////////////////////////////////////////////////
///                          INCLUDES                                ///
////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
#define inline __inline // compatible with MS VS 6.0
#endif

#include "hashInt.h"
#include "hashFlt.h"
#include "hashPtr.h"

////////////////////////////////////////////////////////////////////////
///                         PARAMETERS                               ///
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
///                         BASIC TYPES                              ///
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
///                      MACRO DEFINITIONS                           ///
////////////////////////////////////////////////////////////////////////

#ifndef ABS
#define ABS(a)			((a) < 0 ? -(a) : (a))
#endif

////////////////////////////////////////////////////////////////////////
///                    FUNCTION DECLARATIONS                         ///
////////////////////////////////////////////////////////////////////////

int Hash_DefaultHashFunc(int key, int nBins) {
  return ABS( ( (key+11)*(key)*7+3 ) % nBins );
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////

#endif
