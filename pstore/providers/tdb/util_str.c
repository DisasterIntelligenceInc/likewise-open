/* Editor Settings: expandtabs and use 4 spaces for indentation
 * ex: set softtabstop=4 tabstop=8 expandtab shiftwidth=4: *
 * -*- mode: c, c-basic-offset: 4 -*- */

/*
 * Copyright Likewise Software    2004-2008
 * All rights reserved.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the license, or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
 * General Public License for more details.  You should have received a copy
 * of the GNU Lesser General Public License along with this program.  If
 * not, see <http://www.gnu.org/licenses/>.
 *
 * LIKEWISE SOFTWARE MAKES THIS SOFTWARE AVAILABLE UNDER OTHER LICENSING
 * TERMS AS WELL.  IF YOU HAVE ENTERED INTO A SEPARATE LICENSE AGREEMENT
 * WITH LIKEWISE SOFTWARE, THEN YOU MAY ELECT TO USE THE SOFTWARE UNDER THE
 * TERMS OF THAT SOFTWARE LICENSE AGREEMENT INSTEAD OF THE TERMS OF THE GNU
 * LESSER GENERAL PUBLIC LICENSE, NOTWITHSTANDING THE ABOVE NOTICE.  IF YOU
 * HAVE QUESTIONS, OR WISH TO REQUEST A COPY OF THE ALTERNATE LICENSING
 * TERMS OFFERED BY LIKEWISE SOFTWARE, PLEASE CONTACT LIKEWISE SOFTWARE AT
 * license@likewisesoftware.com
 */

/*
 *  Copyright (C) Likewise Software. All rights reserved.
 *  
 *  Module Name:
 *
 *     provider-main.c
 *
 *  Abstract:
 *
 *        Likewise Password Storage (LWPS)
 *
 *        API to support TDB Password Storage
 *
 *  Authors: Gerald Carter <gcarter@likewisesoftware.com>
 *
 */

#include <config.h>

#include "util_str.h"

/**********************************************************
 * Convert a string to upper case.  modify the input string
 */

VOID
StrUpper(
	PSTR pszString
	)
{
	CHAR *p = pszString;
	
	while (p && *p) {
		*p = toupper(*p);
		p++;		
	}
	
	return;	
}


/***********************************************************
 * Is pszStr1 a substring of pszStr2 ?
 */

BOOLEAN
StrEqual(
	PCSTR pszStr1,
	PCSTR pszStr2
	)
{
	DWORD dwError = LWPS_ERROR_INTERNAL;	
	PSTR pszCopy1 = NULL;
	PSTR pszCopy2 = NULL;
	BOOLEAN bEqual = FALSE;	
	
	/* If same pointer, then must be equal */

	if (pszStr1 == pszStr2)
		return TRUE;

	/* If either is NULL, cannot be substrings */

	if (!pszStr1 || !pszStr2)
		return FALSE;

	/* Check lengths */

	if (strlen(pszStr1) != strlen(pszStr2))
		return FALSE;	

	/* Now copy, convert to upper case, and compare */

	dwError = LwpsAllocateString(pszStr1, &pszCopy1);
	BAIL_ON_LWPS_ERROR(dwError);

	dwError = LwpsAllocateString(pszStr2, &pszCopy2);
	BAIL_ON_LWPS_ERROR(dwError);

	StrUpper(pszCopy1);
	StrUpper(pszCopy2);
	
	if (strcmp(pszCopy1, pszCopy2) == 0) {		
		bEqual = TRUE;		
	}
	
error:
	if (pszCopy1)
		LwpsFreeMemory(pszCopy1);
	if (pszCopy2)	
		LwpsFreeMemory(pszCopy2);

	return bEqual;	
}

/***********************************************************
 */

BOOLEAN
StrnEqual(
	PCSTR pszStr1,
	PCSTR pszStr2,
	DWORD dwChars
	)
{
	DWORD dwLen1, dwLen2;
	DWORD dwError = LWPS_ERROR_INTERNAL;	
	PSTR pszCopy1 = NULL;	
	PSTR pszCopy2 = NULL;
	BOOLEAN bResult = FALSE;	
	
	/* If same pointer, then must be equal */

	if (pszStr1 == pszStr2)
		return TRUE;

	/* If either is NULL, cannot be substrings */

	if (!pszStr1 || !pszStr2)
		return FALSE;

	dwLen1 = strlen(pszStr1);
	dwLen2 = strlen(pszStr2);

	dwError = LwpsAllocateString(pszStr1, &pszCopy1);
	BAIL_ON_LWPS_ERROR(dwError);

	dwError = LwpsAllocateString(pszStr2, &pszCopy2);
	BAIL_ON_LWPS_ERROR(dwError);

	if (dwLen1 > dwChars) {
		*(pszCopy1 + dwChars) = '\0';		
	}
	if (dwLen2 > dwChars) {
		*(pszCopy2 + dwChars) = '\0';		
	}

	bResult = StrEqual(pszCopy1, pszCopy2);

error:
	if (pszCopy1)
		LwpsFreeMemory(pszCopy1);
	if (pszCopy2)
		LwpsFreeMemory(pszCopy2);

	return bResult;	
}



/*
local variables:
mode: c
c-basic-offset: 4
indent-tabs-mode: nil
tab-width: 4
end:
*/

