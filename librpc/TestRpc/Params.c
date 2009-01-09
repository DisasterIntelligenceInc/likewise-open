/* Editor Settings: expandtabs and use 4 spaces for indentation
 * ex: set softtabstop=4 tabstop=8 expandtab shiftwidth=4: *
 */

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

#include <stdlib.h>
#include <string.h>

#include <config.h>
#include <lwrpc/types.h>
#include <wc16str.h>
#include <lwrpc/security.h>
#include <lw/ntstatus.h>

#include "Params.h"


#if !defined(HAVE_STRNDUP)
char* strndup(const char *s, size_t maxlen)
{
    char *ret;
    size_t len, size;

    len = strlen(s);
    len = (len > maxlen) ? maxlen : len;
    size = (len + 1) * sizeof(char);
    ret = (char*) malloc(size);
    strncpy(ret, s, len);

    /* ensure null termination */
    ret[len] = '\0';

    return ret;
}
#endif


/*
 * Handles "escaped" separator character. It allows separator character
 * to be treated as part of value string.
 */
static char* cleanup_sep(char *s, char sep)
{
    char *seppos;
    char sepstr[3] = {0};
    int i = 0;

    if (s == NULL) return s;

    sepstr[0] = '\\';
    sepstr[1] = sep;

    seppos = strstr(s, sepstr);
    while (seppos) {
        while (*seppos) seppos[0] = (seppos++)[1];
        seppos = strstr(s, sepstr);
    }

    return s;
}


/*
 * Splits a string into array of strings given separator character
 */
char** get_string_list(char *list, const char sep)
{
    char **ret;
    char *start, *end = NULL;
    int i, count = 1;

    /* count the elements */
    start = list;
    end = strchr(list, sep);
    while (end++) {
        end = strchr(end, sep);

        /* skip any "escaped" separator */
        if (end > start && *(end-1) == '\\') continue;
        count++;
    }

    ret = (char**) malloc(sizeof(char*) * (count + 1));
    if (!ret) return NULL;

    memset((void*)ret, 0, sizeof(char*) * (count + 1));

    /* copy elements to the array */
    start = list;
    for (i = 0; i < count; i++) {
        end = strchr(start, sep);

        /* skip any "escaped" separator */
        while (start && end > start && *(end-1) == '\\') {
            char *pos = (char*)(end+1);
            end = strchr(pos,sep);
        }

        if (end) {
            ret[i] = strndup(start, (size_t)(end - start));
            start = &end[1];

        } else if (strlen(start)) {
            ret[i] = strdup(start);
        }

        ret[i] = cleanup_sep(ret[i], sep);
    }

    return ret;
}


/*
 * Returns array of strings for multi-value parameters
 */
char** get_value_list(const char *list)
{
    const char start_list = '[';
    const char end_list = ']';
    const char element_sep = ':';

    size_t input_str_len = 0;
    char *str_list = NULL;
    const char *start = NULL;
    char **ret = NULL;

    /* List has to start with '[' char ... */
    if (list[0] != start_list) return NULL;

    /* ... and has to end with ']' char */
    input_str_len = strlen(list);
    if (list[input_str_len - 1] != end_list) return NULL;

    start = &(list[1]);
    str_list = strndup(start, strlen(start) - 1);
    if (str_list == NULL) return NULL;

    /* Split the list to array of values */
    ret = get_string_list(str_list, element_sep);

    free(str_list);
    return ret;
}


/*
 * Parses string of optional parameters (key=value pairs) into array
 * of structures.
 */
struct parameter* get_optional_params(char *opt, int *count)
{
    const char separator = ',';
    const char equal = '=';
    struct parameter *params;
    char **opts;
    int i;

    if (!opt) {
        if (count) *count = 0;
        return NULL;
    }

    if (!count) return NULL;

    *count = 0;
    i = 0;

    opts = get_string_list(opt, separator);
    if (opts == NULL) return NULL;

    while (opts[(*count)]) (*count)++;

    params = (struct parameter*) malloc(sizeof(struct parameter) * (*count));
    if (params == NULL) return NULL;

    while (opts[i]) {
        size_t key_size, val_size;
        char *param = opts[i];
        char *value = strchr(param, equal);

        if (param) {
            key_size = (size_t)(value - param);
            params[i].key = strndup(param, key_size);

            if (value) {
                val_size = strlen(param) - key_size - 1; /* equal char doesn't count */
                params[i].val = strndup((char*)(value + sizeof(char)), val_size);

            } else {
                /* if param is specified but does not equal to anything we can
                   assume it's a boolean flag set */
                params[i].val = strdup("1");
            }

            i++;
        }
    }

    for (i = 0; i < (*count); i++) {
        free(opts[i]);
    }
    free(opts);

    return params;
}


const char* find_value(struct parameter *params, int count, const char *key)
{
    int i;

    for (i = 0; i < count; i++) {
        if (strstr(params[i].key, key)) return params[i].val;
    }

    return NULL;
}


/*
 * Converts array of strings to array of 2-byte unicode strings
 */
wchar16_t **create_wc16str_list(char **strlist)
{
    int list_len = 0;
    wchar16_t **wc16str_list = NULL;
    int i = 0;

    if (strlist == NULL) return NULL;

    /* count the elements (including terminating zero) */
    while (strlist[list_len++]);

    /* allocate the wchar16_t strings array */
    wc16str_list = (wchar16_t**) malloc(sizeof(wchar16_t*) * list_len);
    if (wc16str_list == NULL) return NULL;

    memset((void*)wc16str_list, 0, sizeof(wchar16_t*) * list_len);

    /* copy mbs strings to wchar16_t strings */
    for (i = 0; strlist[i] && i < list_len; i++) {
        wc16str_list[i] = ambstowc16s(strlist[i]);
        if (wc16str_list[i] == NULL) {
            i--;
            while (i >= 0) {
                free(wc16str_list[i--]);
            }
            free(wc16str_list);

            return NULL;
        }

    }

    return wc16str_list;
}


/*
 * Converts array of sid strings to array of sids
 */
DomSid **create_sid_list(char **strlist)
{
    int list_len = 0;
    DomSid **sid_list = NULL;
    int i = 0;

    if (strlist == NULL) return NULL;

    /* count the elements (including terminating zero) */
    while (strlist[list_len++]);

    /* allocate the wchar16_t strings array */
    sid_list = (DomSid**) malloc(sizeof(DomSid*) * list_len);
    if (sid_list == NULL) return NULL;

    memset((void*)sid_list, 0, sizeof(DomSid*) * list_len);

    /* copy mbs strings to wchar16_t strings */
    for (i = 0; strlist[i] && i < list_len; i++) {
        ParseSidString(&(sid_list[i]), strlist[i]);
        if (sid_list[i] == NULL) {
            i--;
            while (i >= 0) {
                SidFree(sid_list[i--]);
            }
            free(sid_list);

            return NULL;
        }
    }

    return sid_list;
}


enum param_err fetch_value(struct parameter *params, int count,
                           const char *key, enum param_type type,
                           void *val, const void *def)
{
    const char *value;
    NTSTATUS status;
    char **valstr, **defstr;
    char *valchar, *defchar;
    wchar16_t **valw16str, **defw16str;
    wchar16_t ***valw16str_list, **defw16str_list;
    int *valint, *defint;
    unsigned int *valuint, *defuint;
    DomSid **valsid = NULL;
    DomSid ***valsid_list = NULL;
    char **strlist = NULL;
    enum param_err ret = perr_success;
    int i = 0;

    if (params && !key) return perr_nullptr_passed;
    if (!val) return perr_invalid_out_param;

    value = find_value(params, count, key);
    if (!value && !def) return perr_not_found;

    switch (type) {
    case pt_string:
        valstr = (char**)val;
        defstr = (char**)def;
        *valstr = (value) ? strdup(value) : strdup(*defstr);
        break;

    case pt_w16string:
        valw16str = (wchar16_t**)val;
        defstr = (char**)def;
        *valw16str = (value) ? ambstowc16s(value) : ambstowc16s(*defstr);
        break;

    case pt_w16string_list:
        valw16str_list = (wchar16_t***)val;
        defstr = (char**)def;
        strlist = (value) ? get_value_list(value) : get_value_list(*defstr);
        *valw16str_list = create_wc16str_list(strlist);
        if (*valw16str_list == NULL) ret = perr_invalid_out_param;
        break;

    case pt_char:
        valchar = (char*)val;
        defchar = (char*)def;
        *valchar = (value) ? value[0] : *defchar;
        break;

    case pt_int32:
        valint = (int*)val;
        defint = (int*)def;
        *valint = (value) ? atoi(value) : *defint;
        break;

    case pt_uint32:
        valuint = (unsigned int*)val;
        defuint = (unsigned int*)def;
        *valuint = (unsigned int)((value) ? atol(value) : *defuint);
        break;

    case pt_sid:
        valsid = (DomSid**)val;
        defstr = (char**)def;
        status = ParseSidString(valsid,
                                ((value) ? (const char*)value : *defstr));
        if (status != STATUS_SUCCESS) ret = perr_invalid_out_param;
        break;

    case pt_sid_list:
        valsid_list = (DomSid***)val;
        defstr = (char**)def;
        strlist = get_value_list((value) ? (const char*)value : *defstr);
        *valsid_list = create_sid_list(strlist);
        if (*valsid_list == NULL) ret = perr_invalid_out_param;
        break;

    default:
        ret = perr_unknown_type;
        break;
    }

    if (strlist) {
        i = 0;
        while (strlist[i]) {
            free(strlist[i++]);
        }

        free(strlist);
    }

    return ret;
}


const char *param_errstr(enum param_err perr)
{
    const errcount = sizeof(param_errstr_maps)/sizeof(struct param_errstr_map);
    int i = 0;

    while (i < errcount && perr != param_errstr_maps[i].perr) i++;
    return param_errstr_maps[i].desc;
}


/*
local variables:
mode: c
c-basic-offset: 4
indent-tabs-mode: nil
tab-width: 4
end:
*/
