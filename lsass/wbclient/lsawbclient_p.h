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
 * Copyright (C) Likewise Software. All rights reserved.
 *
 * Module Name:
 *
 *        lsawbclirent_p.h
 *
 * Abstract:
 *
 *        Likewise Security and Authentication Subsystem (LSASS)
 * 
 * Authors: Gerald Carter <gcarter@likewisesoftware.com>
 *
 */

#ifndef _LSAWBCLIENT_P_H
#define _LSAWBCLIENT_P_H

#include "wbclient.h"
#include "lsasystem.h"
#include "lsa/lsa.h"
#include "lsaclient.h"

#define LSA_WBC_LIBRARY_MAJOR_VERSION          0
#define LSA_WBC_LIBRARY_MINOR_VERSION          1
#define LSA_WBC_LIBRARY_VENDOR_STRING          "Likewise Security Authority"

#define LSA_WBC_INTERFACE_VERSION              1001
#define LSA_WBC_WINBIND_VERSION                "5.0.0"

/* Common macros */

#define _WBC_FREE(x)				\
	do {					\
		_wbc_free((void *)x);			\
		(x) = NULL;			\
	} while (0);

#define _WBC_FREE_CONST_DISCARD(x)		\
	do {					\
		_wbc_free((void*)x);		\
		(x) = NULL;			\
	} while (0);

#define BAIL_ON_WBC_ERR(x)			\
	do {					\
		if (x != WBC_ERR_SUCCESS)	\
			goto done;		\
	} while(0);

#define BAIL_ON_LSA_ERR(x)			\
	do {					\
		if (x != LSA_ERROR_SUCCESS)	\
			goto done;		\
	} while(0);

#define BAIL_ON_NETLOGON_ERR(x)			\
	do {					\
	    if (x != LWNET_ERROR_SUCCESS)	\
		    goto done;			\
	} while(0);


#define BAIL_ON_NULL_PTR(x, y)				\
	do {						\
		if (x == NULL) {			\
			y = LSA_ERROR_OUT_OF_MEMORY;	\
			goto done;			\
		}					\
	} while(0);

#define BAIL_ON_NULL_PTR_PARAM(x, y)				\
	do {							\
		if (x == NULL) {				\
			y = LSA_ERROR_INVALID_PARAMETER;	\
			goto done;				\
		}						\
	} while(0);

#define LSA_ERROR_IS_OK(x)         ((x) == LSA_ERROR_SUCCESS)

		
/* local private functions */

DWORD  map_wbc_to_lsa_error(wbcErr);
wbcErr map_error_to_wbc_status(DWORD);

/* memory management */

typedef int (*mem_destructor_t)(void*);

void *_wbc_malloc(size_t size, mem_destructor_t destructor);
void *_wbc_realloc(void *p, size_t new_size);
void *_wbc_malloc_zero(size_t size, mem_destructor_t destructor);
void  _wbc_free(void *p);
char* _wbc_strdup(const char *str);
int   _wbc_free_string_array(void *p);

#endif /* _LSAWBCLIENT_P_H */
