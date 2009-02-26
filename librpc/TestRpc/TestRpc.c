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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <compat/rpcstatus.h>
#include <dce/dce_error.h>
#include <wc16str.h>
#include <lwrdr/lwrdr.h>

#include <lwrpc/types.h>
#include <lwrpc/security.h>
#include <lwrpc/winerror.h>
#include <lwrpc/allocate.h>

#include "Params.h"
#include "TestRpc.h"


void AddTest(struct test *ft, const char *name, test_fn function)
{
    struct test *nt, *lt;

    if (ft == NULL) return;

    lt = ft;
    while (lt && lt->next) lt = lt->next;

    if (lt->name && lt->function) {
	/* allocate the new test */
	nt = TNEW(ft, struct test);
	if (nt == NULL) return;

	/* append the test to the list */
	lt->next = nt;

    } else {
	/* this is the very first test so use already
	   allocated node */
	nt = lt;
    }

    /* init the new test */
    nt->name     = name;
    nt->function = function;
    nt->next     = NULL;
}


struct test* FindTest(struct test *ft, const char *name)
{
    struct test *t = ft;

    /* search through the tests and try to find
       a matching name */
    while (t) {
        if (strcasecmp(t->name, name) == 0) {
            return t;
        }

        t = t->next;
    }

    return NULL;
}


int StartTest(struct test *t, const wchar16_t *hostname,
	      const wchar16_t *username, const wchar16_t *password,
	      struct parameter *options, int optcount)
{
    int ret;

    if (t == NULL || hostname == NULL) return -1;

    ret = t->function(t, hostname, username, password,
		      options, optcount);
    printf("%s\n", (ret) ? "SUCCEEDED" : "FAILED");
    return ret;
}



void display_usage()
{
    printf("Usage: testrpc -h hostname [-u username] [-p password] [-o options] testname\n");
    printf("\thostname - host to connect when performing a test\n");
    printf("\tusername - user name to use when connecting the host (leave blank to use\n"
	   "\t           kerberos credentials or anonynous session if available)\n");
    printf("\tpassword - user name to use when connecting the host (leave blank to use\n"
	   "\t           kerberos credentials or anonynous session if available)\n");
}


extern char *optarg;
int verbose_mode;


int main(int argc, char *argv[])
{
    int i, opt, ret;
    char *testname, *host, *optional_args, *user, *pass;
    struct test *tests, *runtest;
    wchar16_t *hostname = NULL;
    wchar16_t *username = NULL;
    wchar16_t *password = NULL;
    size_t hostname_size;
    struct parameter *params = NULL;
    int params_len;

    host          = NULL;
    user          = NULL;
    pass          = NULL;
    tests         = NULL;
    runtest       = NULL;
    optional_args = NULL;

    verbose_mode = false;

    while ((opt = getopt(argc, argv, "h:o:vu:p:")) != -1) {
        switch (opt) {
        case 'h':
            host = optarg;
            break;

        case 'o':
            optional_args = optarg;
            break;

        case 'v':
            verbose_mode = true;
            break;

        case 'u':
            user = optarg;
            break;

        case 'p':
            pass = optarg;
            break;

        default:
            display_usage();
            return -1;
        }
    }

    tests = TNEW(NULL, struct test);
    if (tests == NULL) {
        printf("Failed to allocate tests\n");
        return -1;
    }

    tests->name     = NULL;
    tests->function = NULL;
    tests->next     = NULL;

    if (host == NULL) {
        printf("Error: no hostname specified\n\n");
        display_usage();
        return -1;
    }

    hostname_size = strlen(host) + 1;
    hostname = (wchar16_t*) talloc(tests, hostname_size * sizeof(wchar16_t), NULL);
    mbstowc16s(hostname, host, hostname_size);

    username = ambstowc16s(user);
    if (user && username == NULL) {
        printf("Failed to allocate username\n");
        goto done;
    }

    password = ambstowc16s(pass);
    if (pass && password == NULL) {
        printf("Failed to allocate password\n");
        goto done;
    }

    params = get_optional_params(optional_args, &params_len);
    if ((params != NULL && params_len == 0) ||
        (params == NULL && params_len != 0)) {
        printf("Error while parsing optional parameters [%s]\n", optional_args);
        goto done;
    }

    SetupSamrTests(tests);
    SetupLsaTests(tests);
    SetupNetlogonTests(tests);
    SetupNetApiTests(tests);
    SetupMprTests(tests);
    
    for (i = 1; i < argc; i++) {
        testname = argv[i];
        runtest = FindTest(tests, testname);

        if (runtest) {
            ret = StartTest(runtest, hostname, username, password,
                            params, params_len);
            goto done;
        }
    }

    printf("No test name specified. Available tests:\n");
    runtest = tests;
    while (runtest) {
        printf("%s\n", runtest->name);
        runtest = runtest->next;
    }
    printf("\n");
    

done:
    tfree(hostname);
    tfree(tests);

    SAFE_FREE(username);
    SAFE_FREE(password);

    for (i = 0; i < params_len; i++) {
        SAFE_FREE(params[i].key);
        SAFE_FREE(params[i].val);
    }
    SAFE_FREE(params);

    return 0;
}


/*
local variables:
mode: c
c-basic-offset: 4
indent-tabs-mode: nil
tab-width: 4
end:
*/
