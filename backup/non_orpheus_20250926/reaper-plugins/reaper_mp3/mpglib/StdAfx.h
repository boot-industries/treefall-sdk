// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(AFX_STDAFX_H__10BCB4FD_A010_4249_A3B0_2F4E2102C712__INCLUDED_)
#define AFX_STDAFX_H__10BCB4FD_A010_4249_A3B0_2F4E2102C712__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mpglib.h"

#include "common.h"
#include "dct64_i386.h"
#include "decode_i386.h"
#include "l2tables.h"
#include "layer2.h"
#include "layer3.h"
#include "tabinit.h"

#if 0
#include "../../../pfc/profiler.h"
#include <windows.h>
#else
#define profiler(x)
#endif

// TODO: reference additional headers your program requires here

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__10BCB4FD_A010_4249_A3B0_2F4E2102C712__INCLUDED_)
