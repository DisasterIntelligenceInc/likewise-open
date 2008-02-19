/* 
   Unix SMB/Netbios implementation.
   SMB client library implementation
   Copyright (C) Andrew Tridgell 1998
   Copyright (C) Richard Sharpe 2000, 2002
   Copyright (C) John Terpstra 2000
   Copyright (C) Tom Jansen (Ninja ISD) 2002 
   Copyright (C) Derrell Lipman 2003, 2004
   Copyright (C) Jeremy Allison 2007, 2008
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"

#include "include/libsmb_internal.h"

struct smbc_dirent *smbc_readdir_ctx(SMBCCTX *context, SMBCFILE *dir);
struct smbc_dir_list *smbc_check_dir_ent(struct smbc_dir_list *list, 
					 struct smbc_dirent *dirent);

/*
 * DOS Attribute values (used internally)
 */
typedef struct DOS_ATTR_DESC {
	int mode;
	SMB_OFF_T size;
	time_t create_time;
	time_t access_time;
	time_t write_time;
	time_t change_time;
	SMB_INO_T inode;
} DOS_ATTR_DESC;


/*
 * Internal flags for extended attributes
 */

/* internal mode values */
#define SMBC_XATTR_MODE_ADD          1
#define SMBC_XATTR_MODE_REMOVE       2
#define SMBC_XATTR_MODE_REMOVE_ALL   3
#define SMBC_XATTR_MODE_SET          4
#define SMBC_XATTR_MODE_CHOWN        5
#define SMBC_XATTR_MODE_CHGRP        6

#define CREATE_ACCESS_READ      READ_CONTROL_ACCESS

/*We should test for this in configure ... */
#ifndef ENOTSUP
#define ENOTSUP EOPNOTSUPP
#endif

/*
 * Functions exported by libsmb_cache.c that we need here
 */
int smbc_default_cache_functions(SMBCCTX *context);

/* 
 * check if an element is part of the list. 
 * FIXME: Does not belong here !  
 * Can anyone put this in a macro in dlinklist.h ?
 * -- Tom
 */
static int DLIST_CONTAINS(SMBCFILE * list, SMBCFILE *p) {
	if (!p || !list) return False;
	do {
		if (p == list) return True;
		list = list->next;
	} while (list);
	return False;
}

/*
 * Find an lsa pipe handle associated with a cli struct.
 */
static struct rpc_pipe_client *
find_lsa_pipe_hnd(struct cli_state *ipc_cli)
{
	struct rpc_pipe_client *pipe_hnd;

	for (pipe_hnd = ipc_cli->pipe_list;
             pipe_hnd;
             pipe_hnd = pipe_hnd->next) {
            
		if (pipe_hnd->pipe_idx == PI_LSARPC) {
			return pipe_hnd;
		}
	}

	return NULL;
}

static int
smbc_close_ctx(SMBCCTX *context,
               SMBCFILE *file);
static off_t
smbc_lseek_ctx(SMBCCTX *context,
               SMBCFILE *file,
               off_t offset,
               int whence);

extern bool in_client;

/*
 * Is the logging working / configfile read ? 
 */
static int smbc_initialized = 0;

static int 
hex2int( unsigned int _char )
{
    if ( _char >= 'A' && _char <='F')
	return _char - 'A' + 10;
    if ( _char >= 'a' && _char <='f')
	return _char - 'a' + 10;
    if ( _char >= '0' && _char <='9')
	return _char - '0';
    return -1;
}

/*
 * smbc_urldecode()
 * and smbc_urldecode_talloc() (internal fn.)
 *
 * Convert strings of %xx to their single character equivalent.  Each 'x' must
 * be a valid hexadecimal digit, or that % sequence is left undecoded.
 *
 * dest may, but need not be, the same pointer as src.
 *
 * Returns the number of % sequences which could not be converted due to lack
 * of two following hexadecimal digits.
 */
static int
smbc_urldecode_talloc(TALLOC_CTX *ctx, char **pp_dest, const char *src)
{
	int old_length = strlen(src);
	int i = 0;
	int err_count = 0;
	size_t newlen = 1;
	char *p, *dest;

	if (old_length == 0) {
		return 0;
	}

	*pp_dest = NULL;
	for (i = 0; i < old_length; ) {
		unsigned char character = src[i++];

		if (character == '%') {
			int a = i+1 < old_length ? hex2int(src[i]) : -1;
			int b = i+1 < old_length ? hex2int(src[i+1]) : -1;

			/* Replace valid sequence */
			if (a != -1 && b != -1) {
				/* Replace valid %xx sequence with %dd */
				character = (a * 16) + b;
				if (character == '\0') {
					break; /* Stop at %00 */
				}
				i += 2;
			} else {
				err_count++;
			}
		}
		newlen++;
	}

	dest = TALLOC_ARRAY(ctx, char, newlen);
	if (!dest) {
		return err_count;
	}

	err_count = 0;
	for (p = dest, i = 0; i < old_length; ) {
                unsigned char character = src[i++];

                if (character == '%') {
                        int a = i+1 < old_length ? hex2int(src[i]) : -1;
                        int b = i+1 < old_length ? hex2int(src[i+1]) : -1;

                        /* Replace valid sequence */
                        if (a != -1 && b != -1) {
                                /* Replace valid %xx sequence with %dd */
                                character = (a * 16) + b;
                                if (character == '\0') {
                                        break; /* Stop at %00 */
                                }
                                i += 2;
                        } else {
                                err_count++;
                        }
                }
                *p++ = character;
        }

        *p = '\0';
	*pp_dest = dest;
        return err_count;
}

int
smbc_urldecode(char *dest, char *src, size_t max_dest_len)
{
	TALLOC_CTX *frame = talloc_stackframe();
	char *pdest;
	int ret = smbc_urldecode_talloc(frame, &pdest, src);

	if (pdest) {
		strlcpy(dest, pdest, max_dest_len);
	}
	TALLOC_FREE(frame);
	return ret;
}

/*
 * smbc_urlencode()
 *
 * Convert any characters not specifically allowed in a URL into their %xx
 * equivalent.
 *
 * Returns the remaining buffer length.
 */
int
smbc_urlencode(char *dest, char *src, int max_dest_len)
{
        char hex[] = "0123456789ABCDEF";

        for (; *src != '\0' && max_dest_len >= 3; src++) {

                if ((*src < '0' &&
                     *src != '-' &&
                     *src != '.') ||
                    (*src > '9' &&
                     *src < 'A') ||
                    (*src > 'Z' &&
                     *src < 'a' &&
                     *src != '_') ||
                    (*src > 'z')) {
                        *dest++ = '%';
                        *dest++ = hex[(*src >> 4) & 0x0f];
                        *dest++ = hex[*src & 0x0f];
                        max_dest_len -= 3;
                } else {
                        *dest++ = *src;
                        max_dest_len--;
                }
        }

        *dest++ = '\0';
        max_dest_len--;

        return max_dest_len;
}

/*
 * Function to parse a path and turn it into components
 *
 * The general format of an SMB URI is explain in Christopher Hertel's CIFS
 * book, at http://ubiqx.org/cifs/Appendix-D.html.  We accept a subset of the
 * general format ("smb:" only; we do not look for "cifs:").
 *
 *
 * We accept:
 *  smb://[[[domain;]user[:password]@]server[/share[/path[/file]]]][?options]
 *
 * Meaning of URLs:
 *
 * smb://           Show all workgroups.
 *
 *                  The method of locating the list of workgroups varies
 *                  depending upon the setting of the context variable
 *                  context->options.browse_max_lmb_count.  This value
 *                  determine the maximum number of local master browsers to
 *                  query for the list of workgroups.  In order to ensure that
 *                  a complete list of workgroups is obtained, all master
 *                  browsers must be queried, but if there are many
 *                  workgroups, the time spent querying can begin to add up.
 *                  For small networks (not many workgroups), it is suggested
 *                  that this variable be set to 0, indicating query all local
 *                  master browsers.  When the network has many workgroups, a
 *                  reasonable setting for this variable might be around 3.
 *
 * smb://name/      if name<1D> or name<1B> exists, list servers in
 *                  workgroup, else, if name<20> exists, list all shares
 *                  for server ...
 *
 * If "options" are provided, this function returns the entire option list as a
 * string, for later parsing by the caller.  Note that currently, no options
 * are supported.
 */

static const char *smbc_prefix = "smb:";

static int
smbc_parse_path(TALLOC_CTX *ctx,
		SMBCCTX *context,
                const char *fname,
                char **pp_workgroup,
                char **pp_server,
                char **pp_share,
                char **pp_path,
		char **pp_user,
                char **pp_password,
                char **pp_options)
{
	char *s;
	const char *p;
	char *q, *r;
	int len;

	/* Ensure these returns are at least valid pointers. */
	*pp_server = talloc_strdup(ctx, "");
	*pp_share = talloc_strdup(ctx, "");
	*pp_path = talloc_strdup(ctx, "");
	*pp_user = talloc_strdup(ctx, "");
	*pp_password = talloc_strdup(ctx, "");

	if (!*pp_server || !*pp_share || !*pp_path ||
			!*pp_user || !*pp_password) {
		return -1;
	}

        /*
         * Assume we wont find an authentication domain to parse, so default
         * to the workgroup in the provided context.
         */
	if (pp_workgroup != NULL) {
		*pp_workgroup = talloc_strdup(ctx, context->workgroup);
	}

	if (pp_options) {
		*pp_options = talloc_strdup(ctx, "");
	}
	s = talloc_strdup(ctx, fname);

	/* see if it has the right prefix */
	len = strlen(smbc_prefix);
	if (strncmp(s,smbc_prefix,len) || (s[len] != '/' && s[len] != 0)) {
                return -1; /* What about no smb: ? */
        }

	p = s + len;

	/* Watch the test below, we are testing to see if we should exit */

	if (strncmp(p, "//", 2) && strncmp(p, "\\\\", 2)) {
                DEBUG(1, ("Invalid path (does not begin with smb://"));
		return -1;
	}

	p += 2;  /* Skip the double slash */

        /* See if any options were specified */
        if ((q = strrchr(p, '?')) != NULL ) {
                /* There are options.  Null terminate here and point to them */
                *q++ = '\0';

                DEBUG(4, ("Found options '%s'", q));

		/* Copy the options */
		if (*pp_options != NULL) {
			TALLOC_FREE(*pp_options);
			*pp_options = talloc_strdup(ctx, q);
		}
	}

	if (*p == '\0') {
		goto decoding;
	}

	if (*p == '/') {
		int wl = strlen(context->workgroup);

		if (wl > 16) {
			wl = 16;
		}

		*pp_server = talloc_strdup(ctx, context->workgroup);
		if (!*pp_server) {
			return -1;
		}
               	*pp_server[wl] = '\0';
		return 0;
	}

	/*
	 * ok, its for us. Now parse out the server, share etc.
	 *
	 * However, we want to parse out [[domain;]user[:password]@] if it
	 * exists ...
	 */

	/* check that '@' occurs before '/', if '/' exists at all */
	q = strchr_m(p, '@');
	r = strchr_m(p, '/');
	if (q && (!r || q < r)) {
		char *userinfo = NULL;
		const char *u;

		next_token_no_ltrim_talloc(ctx, &p, &userinfo, "@");
		if (!userinfo) {
			return -1;
		}
		u = userinfo;

		if (strchr_m(u, ';')) {
			char *workgroup;
			next_token_no_ltrim_talloc(ctx, &u, &workgroup, ";");
			if (!workgroup) {
				return -1;
			}
			if (pp_workgroup) {
				*pp_workgroup = workgroup;
			}
		}

		if (strchr_m(u, ':')) {
			next_token_no_ltrim_talloc(ctx, &u, pp_user, ":");
			if (!*pp_user) {
				return -1;
			}
			*pp_password = talloc_strdup(ctx, u);
			if (!*pp_password) {
				return -1;
			}
		} else {
			*pp_user = talloc_strdup(ctx, u);
			if (!*pp_user) {
				return -1;
			}
		}
	}

	if (!next_token_talloc(ctx, &p, pp_server, "/")) {
		return -1;
	}

	if (*p == (char)0) {
		goto decoding;  /* That's it ... */
	}

	if (!next_token_talloc(ctx, &p, pp_share, "/")) {
		return -1;
	}

        /*
         * Prepend a leading slash if there's a file path, as required by
         * NetApp filers.
         */
        if (*p != '\0') {
		*pp_path = talloc_asprintf(ctx,
					"\\%s",
					p);
        } else {
		*pp_path = talloc_strdup(ctx, "");
	}
	if (!*pp_path) {
		return -1;
	}
	string_replace(*pp_path, '/', '\\');

 decoding:

	(void) smbc_urldecode_talloc(ctx, pp_path, *pp_path);
	(void) smbc_urldecode_talloc(ctx, pp_server, *pp_server);
	(void) smbc_urldecode_talloc(ctx, pp_share, *pp_share);
	(void) smbc_urldecode_talloc(ctx, pp_user, *pp_user);
	(void) smbc_urldecode_talloc(ctx, pp_password, *pp_password);

	return 0;
}

/*
 * Verify that the options specified in a URL are valid
 */
static int
smbc_check_options(char *server,
                   char *share,
                   char *path,
                   char *options)
{
        DEBUG(4, ("smbc_check_options(): server='%s' share='%s' "
                  "path='%s' options='%s'\n",
                  server, share, path, options));

        /* No options at all is always ok */
        if (! *options) return 0;

        /* Currently, we don't support any options. */
        return -1;
}

/*
 * Convert an SMB error into a UNIX error ...
 */
static int
smbc_errno(SMBCCTX *context,
           struct cli_state *c)
{
	int ret = cli_errno(c);
	
        if (cli_is_dos_error(c)) {
                uint8 eclass;
                uint32 ecode;

                cli_dos_error(c, &eclass, &ecode);
                
                DEBUG(3,("smbc_error %d %d (0x%x) -> %d\n", 
                         (int)eclass, (int)ecode, (int)ecode, ret));
        } else {
                NTSTATUS status;

                status = cli_nt_error(c);

                DEBUG(3,("smbc errno %s -> %d\n",
                         nt_errstr(status), ret));
        }

	return ret;
}

/* 
 * Check a server for being alive and well.
 * returns 0 if the server is in shape. Returns 1 on error 
 * 
 * Also useable outside libsmbclient to enable external cache
 * to do some checks too.
 */
static int
smbc_check_server(SMBCCTX * context,
                  SMBCSRV * server) 
{
        socklen_t size;
        struct sockaddr addr;

        size = sizeof(addr);
        return (getpeername(server->cli->fd, &addr, &size) == -1);
}

/* 
 * Remove a server from the cached server list it's unused.
 * On success, 0 is returned. 1 is returned if the server could not be removed.
 * 
 * Also useable outside libsmbclient
 */
int
smbc_remove_unused_server(SMBCCTX * context,
                          SMBCSRV * srv)
{
	SMBCFILE * file;

	/* are we being fooled ? */
	if (!context || !context->internal ||
	    !context->internal->_initialized || !srv) return 1;

	
	/* Check all open files/directories for a relation with this server */
	for (file = context->internal->_files; file; file=file->next) {
		if (file->srv == srv) {
			/* Still used */
			DEBUG(3, ("smbc_remove_usused_server: "
                                  "%p still used by %p.\n",
				  srv, file));
			return 1;
		}
	}

	DLIST_REMOVE(context->internal->_servers, srv);

	cli_shutdown(srv->cli);
	srv->cli = NULL;

	DEBUG(3, ("smbc_remove_usused_server: %p removed.\n", srv));

	(context->callbacks.remove_cached_srv_fn)(context, srv);

        SAFE_FREE(srv);
	return 0;
}

/****************************************************************
 * Call the auth_fn with fixed size (fstring) buffers.
 ***************************************************************/

static void call_auth_fn(TALLOC_CTX *ctx,
			 SMBCCTX *context,
			 const char *server,
			 const char *share,
			 char **pp_workgroup,
			 char **pp_username,
			 char **pp_password,
			 char **pp_ccname)
{
	fstring workgroup;
	fstring username;
	fstring password;
	fstring ccname;

	strlcpy(workgroup, *pp_workgroup, sizeof(workgroup));
	strlcpy(username, *pp_username, sizeof(username));
	strlcpy(password, *pp_password, sizeof(password));
	strlcpy(ccname, *pp_ccname, sizeof(ccname));

	if (context->internal->_auth_fn_with_ccname != NULL) {
		context->internal->_auth_fn_with_ccname(context, server, share,
							workgroup,
							sizeof(fstring),
							username,
							sizeof(fstring),
							password,
							sizeof(fstring),
							ccname,
							sizeof(fstring));

	} else if (context->internal->_auth_fn_with_context != NULL) {
			(context->internal->_auth_fn_with_context)(
				context,
				server, share,
				workgroup, sizeof(workgroup),
				username, sizeof(username),
				password, sizeof(password));
	} else {
		(context->callbacks.auth_fn)(
			server, share,
			workgroup, sizeof(workgroup),
			username, sizeof(username),
			password, sizeof(password));
	}

	TALLOC_FREE(*pp_workgroup);
	TALLOC_FREE(*pp_username);
	TALLOC_FREE(*pp_password);
	/* ccname is not dynamically allocated yet, so no it's not freed */

	*pp_workgroup = talloc_strdup(ctx, workgroup);
	*pp_username = talloc_strdup(ctx, username);
	*pp_password = talloc_strdup(ctx, password);
	*pp_ccname = talloc_strdup(ctx, ccname);
}

static SMBCSRV *
find_server(TALLOC_CTX *ctx,
	    SMBCCTX *context,
	    const char *server,
	    const char *share,
	    char **pp_workgroup,
	    char **pp_username,
	    char **pp_password,
	    char **pp_ccname)
{
        SMBCSRV *srv;
        int auth_called = 0;

 check_server_cache:

	srv = (context->callbacks.get_cached_srv_fn)(context, server, share,
						*pp_workgroup, *pp_username);

	if (!auth_called && !srv && (!*pp_username || !(*pp_username)[0] ||
				!*pp_password || !(*pp_password)[0])) {
		call_auth_fn(ctx, context, server, share,
			     pp_workgroup, pp_username, pp_password, pp_ccname);

		if (!pp_workgroup || !pp_username || !pp_password) {
			return NULL;
		}

		/*
                 * However, smbc_auth_fn may have picked up info relating to
                 * an existing connection, so try for an existing connection
                 * again ...
                 */
		auth_called = 1;
		goto check_server_cache;

	}

	if (srv) {
		if ((context->callbacks.check_server_fn)(context, srv)) {
			/*
                         * This server is no good anymore
                         * Try to remove it and check for more possible
                         * servers in the cache
                         */
			if ((context->callbacks.remove_unused_server_fn)(context,
                                                                         srv)) { 
                                /*
                                 * We could not remove the server completely,
                                 * remove it from the cache so we will not get
                                 * it again. It will be removed when the last
                                 * file/dir is closed.
                                 */
				(context->callbacks.remove_cached_srv_fn)(context,
                                                                          srv);
			}

			/*
                         * Maybe there are more cached connections to this
                         * server
                         */
			goto check_server_cache;
		}

		return srv;
 	}

        return NULL;
}

/*
 * Connect to a server, possibly on an existing connection
 *
 * Here, what we want to do is: If the server and username
 * match an existing connection, reuse that, otherwise, establish a
 * new connection.
 *
 * If we have to create a new connection, call the auth_fn to get the
 * info we need, unless the username and password were passed in.
 */

static SMBCSRV *
smbc_server(TALLOC_CTX *ctx,
		SMBCCTX *context,
		bool connect_if_not_found,
		const char *server,
		const char *share,
		char **pp_workgroup,
		char **pp_username,
		char **pp_password)
{
	SMBCSRV *srv=NULL;
	struct cli_state *c;
	struct nmb_name called, calling;
	const char *server_n = server;
	struct sockaddr_storage ss;
	int tried_reverse = 0;
        int port_try_first;
        int port_try_next;
        const char *username_used;
 	NTSTATUS status;
	fstring ccname_buf;
	char *ccname;

	zero_addr(&ss);
	ZERO_STRUCT(c);
	ccname_buf[0] = 0;
	ccname = ccname_buf;

	if (server[0] == 0) {
		errno = EPERM;
		return NULL;
	}

        /* Look for a cached connection */
        srv = find_server(ctx, context, server, share,
                          pp_workgroup, pp_username, pp_password,
			  &ccname);

        /*
         * If we found a connection and we're only allowed one share per
         * server...
         */
        if (srv && *share != '\0' && context->options.one_share_per_server) {

                /*
                 * ... then if there's no current connection to the share,
                 * connect to it.  find_server(), or rather the function
                 * pointed to by context->callbacks.get_cached_srv_fn which
                 * was called by find_server(), will have issued a tree
                 * disconnect if the requested share is not the same as the
                 * one that was already connected.
                 */
                if (srv->cli->cnum == (uint16) -1) {
                        /* Ensure we have accurate auth info */
			call_auth_fn(ctx, context, server, share,
				     pp_workgroup, pp_username, pp_password,
				     &ccname);

			if (!*pp_workgroup || !*pp_username || !*pp_password) {
				errno = ENOMEM;
				cli_shutdown(srv->cli);
				srv->cli = NULL;
				(context->callbacks.remove_cached_srv_fn)(context,
									srv);
				return NULL;
			}

			/*
			 * We don't need to renegotiate encryption
			 * here as the encryption context is not per
			 * tid.
			 */

			if (!cli_send_tconX(srv->cli, share, "?????",
						*pp_password,
						strlen(*pp_password)+1)) {

                                errno = smbc_errno(context, srv->cli);
                                cli_shutdown(srv->cli);
				srv->cli = NULL;
                                (context->callbacks.remove_cached_srv_fn)(context,
                                                                          srv);
                                srv = NULL;
                        }

                        /*
                         * Regenerate the dev value since it's based on both
                         * server and share
                         */
                        if (srv) {
                                srv->dev = (dev_t)(str_checksum(server) ^
                                                   str_checksum(share));
                        }
                }
        }

        /* If we have a connection... */
        if (srv) {

                /* ... then we're done here.  Give 'em what they came for. */
                return srv;
        }

        /* If we're not asked to connect when a connection doesn't exist... */
        if (! connect_if_not_found) {
                /* ... then we're done here. */
                return NULL;
        }

	if (!*pp_workgroup || !*pp_username || !*pp_password) {
		errno = ENOMEM;
		return NULL;
	}

	make_nmb_name(&calling, context->netbios_name, 0x0);
	make_nmb_name(&called , server, 0x20);

	DEBUG(4,("smbc_server: server_n=[%s] server=[%s]\n", server_n, server));

	DEBUG(4,(" -> server_n=[%s] server=[%s]\n", server_n, server));

 again:

	zero_addr(&ss);

	/* have to open a new connection */
	if ((c = cli_initialise()) == NULL) {
		errno = ENOMEM;
		return NULL;
	}

	if (context->flags & SMB_CTX_FLAG_USE_KERBEROS) {
		c->use_kerberos = True;
		cli_set_ccname(c, ccname);
	}
	if (context->flags & SMB_CTX_FLAG_FALLBACK_AFTER_KERBEROS) {
		c->fallback_after_kerberos = True;
	}

	c->timeout = context->timeout;

        /*
         * Force use of port 139 for first try if share is $IPC, empty, or
         * null, so browse lists can work
         */
        if (share == NULL || *share == '\0' || strcmp(share, "IPC$") == 0) {
                port_try_first = 139;
                port_try_next = 445;
        } else {
                port_try_first = 445;
                port_try_next = 139;
        }

        c->port = port_try_first;

	status = cli_connect(c, server_n, &ss);
	if (!NT_STATUS_IS_OK(status)) {

                /* First connection attempt failed.  Try alternate port. */
                c->port = port_try_next;

                status = cli_connect(c, server_n, &ss);
		if (!NT_STATUS_IS_OK(status)) {
			cli_shutdown(c);
			errno = ETIMEDOUT;
			return NULL;
		}
	}

	if (!cli_session_request(c, &calling, &called)) {
		cli_shutdown(c);
		if (strcmp(called.name, "*SMBSERVER")) {
			make_nmb_name(&called , "*SMBSERVER", 0x20);
			goto again;
		} else {  /* Try one more time, but ensure we don't loop */

			/* Only try this if server is an IP address ... */

			if (is_ipaddress(server) && !tried_reverse) {
				fstring remote_name;
				struct sockaddr_storage rem_ss;

				if (!interpret_string_addr(&rem_ss, server,
							NI_NUMERICHOST)) {
					DEBUG(4, ("Could not convert IP address "
						"%s to struct sockaddr_storage\n",
						server));
					errno = ETIMEDOUT;
					return NULL;
				}

				tried_reverse++; /* Yuck */

				if (name_status_find("*", 0, 0, &rem_ss, remote_name)) {
					make_nmb_name(&called, remote_name, 0x20);
					goto again;
				}
			}
		}
		errno = ETIMEDOUT;
		return NULL;
	}

	DEBUG(4,(" session request ok\n"));

	if (!cli_negprot(c)) {
		cli_shutdown(c);
		errno = ETIMEDOUT;
		return NULL;
	}

        username_used = *pp_username;

	if (!NT_STATUS_IS_OK(cli_session_setup(c, username_used,
					       *pp_password, strlen(*pp_password),
					       *pp_password, strlen(*pp_password),
					       *pp_workgroup))) {

                /* Failed.  Try an anonymous login, if allowed by flags. */
                username_used = "";

                if ((context->flags & SMBCCTX_FLAG_NO_AUTO_ANONYMOUS_LOGON) ||
                     !NT_STATUS_IS_OK(cli_session_setup(c, username_used,
							*pp_password, 1,
							*pp_password, 0,
							*pp_workgroup))) {

                        cli_shutdown(c);
                        errno = EPERM;
                        return NULL;
                }
	}

	DEBUG(4,(" session setup ok\n"));

	if (!cli_send_tconX(c, share, "?????",
			    *pp_password, strlen(*pp_password)+1)) {
		errno = smbc_errno(context, c);
		cli_shutdown(c);
		return NULL;
	}

	DEBUG(4,(" tconx ok\n"));

	if (context->internal->_smb_encryption_level) {
		/* Attempt UNIX smb encryption. */
		if (!NT_STATUS_IS_OK(cli_force_encryption(c,
						username_used,
						*pp_password,
						*pp_workgroup))) {

			/*
			 * context->internal->_smb_encryption_level == 1
			 * means don't fail if encryption can't be negotiated,
			 * == 2 means fail if encryption can't be negotiated.
			 */

			DEBUG(4,(" SMB encrypt failed\n"));

			if (context->internal->_smb_encryption_level == 2) {
	                        cli_shutdown(c);
				errno = EPERM;
				return NULL;
			}
		}
		DEBUG(4,(" SMB encrypt ok\n"));
	}

	/*
	 * Ok, we have got a nice connection
	 * Let's allocate a server structure.
	 */

	srv = SMB_MALLOC_P(SMBCSRV);
	if (!srv) {
		errno = ENOMEM;
		goto failed;
	}

	ZERO_STRUCTP(srv);
	srv->cli = c;
	srv->dev = (dev_t)(str_checksum(server) ^ str_checksum(share));
        srv->no_pathinfo = False;
        srv->no_pathinfo2 = False;
        srv->no_nt_session = False;

	/* now add it to the cache (internal or external)  */
	/* Let the cache function set errno if it wants to */
	errno = 0;
	if ((context->callbacks.add_cached_srv_fn)(context, srv,
						server, share,
						*pp_workgroup,
						*pp_username)) {
		int saved_errno = errno;
		DEBUG(3, (" Failed to add server to cache\n"));
		errno = saved_errno;
		if (errno == 0) {
			errno = ENOMEM;
		}
		goto failed;
	}

	DEBUG(2, ("Server connect ok: //%s/%s: %p\n",
		  server, share, srv));

	DLIST_ADD(context->internal->_servers, srv);
	return srv;

 failed:
	cli_shutdown(c);
	if (!srv) {
		return NULL;
	}

	SAFE_FREE(srv);
	return NULL;
}

/*
 * Connect to a server for getting/setting attributes, possibly on an existing
 * connection.  This works similarly to smbc_server().
 */
static SMBCSRV *
smbc_attr_server(TALLOC_CTX *ctx,
		SMBCCTX *context,
		const char *server,
		const char *share,
		char **pp_workgroup,
		char **pp_username,
		char **pp_password)
{
        int flags;
        struct sockaddr_storage ss;
	struct cli_state *ipc_cli;
	struct rpc_pipe_client *pipe_hnd;
        NTSTATUS nt_status;
	SMBCSRV *ipc_srv=NULL;
	const char *use_ccname = NULL;
	fstring ccname_buf;
	char *ccname;

	ccname_buf[0] = 0;
	ccname = ccname_buf;

        /*
         * See if we've already created this special connection.  Reference
         * our "special" share name '*IPC$', which is an impossible real share
         * name due to the leading asterisk.
         */
        ipc_srv = find_server(ctx, context, server, "*IPC$",
                              pp_workgroup, pp_username, pp_password,
			      &ccname);
        if (!ipc_srv) {

                /* We didn't find a cached connection.  Get the password */
		if (!*pp_password || (*pp_password)[0] == '\0') {
                        /* ... then retrieve it now. */
			call_auth_fn(ctx, context, server, share,
				     pp_workgroup, pp_username, pp_password,
				     &ccname);
			if (!*pp_workgroup || !*pp_username || !*pp_password) {
				errno = ENOMEM;
				return NULL;
			}
                }

                flags = 0;
                if (context->flags & SMB_CTX_FLAG_USE_KERBEROS) {
                        flags |= CLI_FULL_CONNECTION_USE_KERBEROS;
			use_ccname = ccname;
                }

                zero_addr(&ss);
                nt_status = cli_full_connection_ccname(&ipc_cli,
						       global_myname(), server,
						       &ss, 0, "IPC$", "?????",
						       *pp_username,
						       *pp_workgroup,
						       *pp_password,
						       use_ccname,
						       flags,
						       Undefined, NULL);
                if (! NT_STATUS_IS_OK(nt_status)) {
                        DEBUG(1,("cli_full_connection failed! (%s)\n",
                                 nt_errstr(nt_status)));
                        errno = ENOTSUP;
                        return NULL;
                }

		if (context->internal->_smb_encryption_level) {
			/* Attempt UNIX smb encryption. */
			if (!NT_STATUS_IS_OK(cli_force_encryption(ipc_cli,
						*pp_username,
						*pp_password,
						*pp_workgroup))) {

				/*
				 * context->internal->_smb_encryption_level == 1
				 * means don't fail if encryption can't be negotiated,
				 * == 2 means fail if encryption can't be negotiated.
				 */

				DEBUG(4,(" SMB encrypt failed on IPC$\n"));

				if (context->internal->_smb_encryption_level == 2) {
		                        cli_shutdown(ipc_cli);
					errno = EPERM;
					return NULL;
				}
			}
			DEBUG(4,(" SMB encrypt ok on IPC$\n"));
		}

                ipc_srv = SMB_MALLOC_P(SMBCSRV);
                if (!ipc_srv) {
                        errno = ENOMEM;
                        cli_shutdown(ipc_cli);
                        return NULL;
                }

                ZERO_STRUCTP(ipc_srv);
                ipc_srv->cli = ipc_cli;

                pipe_hnd = cli_rpc_pipe_open_noauth(ipc_srv->cli,
                                                    PI_LSARPC,
                                                    &nt_status);
                if (!pipe_hnd) {
                    DEBUG(1, ("cli_nt_session_open fail!\n"));
                    errno = ENOTSUP;
                    cli_shutdown(ipc_srv->cli);
                    free(ipc_srv);
                    return NULL;
                }

                /*
                 * Some systems don't support
                 * SEC_RIGHTS_MAXIMUM_ALLOWED, but NT sends 0x2000000
                 * so we might as well do it too.
                 */

                nt_status = rpccli_lsa_open_policy(
                    pipe_hnd,
                    talloc_tos(),
                    True,
                    GENERIC_EXECUTE_ACCESS,
                    &ipc_srv->pol);

                if (!NT_STATUS_IS_OK(nt_status)) {
                    errno = smbc_errno(context, ipc_srv->cli);
                    cli_shutdown(ipc_srv->cli);
                    return NULL;
                }

                /* now add it to the cache (internal or external) */

                errno = 0;      /* let cache function set errno if it likes */
                if ((context->callbacks.add_cached_srv_fn)(context, ipc_srv,
							server,
							"*IPC$",
							*pp_workgroup,
							*pp_username)) {
                        DEBUG(3, (" Failed to add server to cache\n"));
                        if (errno == 0) {
                                errno = ENOMEM;
                        }
                        cli_shutdown(ipc_srv->cli);
                        free(ipc_srv);
                        return NULL;
                }

                DLIST_ADD(context->internal->_servers, ipc_srv);
        }

        return ipc_srv;
}

/*
 * Routine to open() a file ...
 */

static SMBCFILE *
smbc_open_ctx(SMBCCTX *context,
              const char *fname,
              int flags,
              mode_t mode)
{
	char *server = NULL, *share = NULL, *user = NULL, *password = NULL, *workgroup = NULL;
	char *path = NULL;
	char *targetpath = NULL;
	struct cli_state *targetcli = NULL;
	SMBCSRV *srv   = NULL;
	SMBCFILE *file = NULL;
	int fd;
	TALLOC_CTX *frame = talloc_stackframe();

	if (!context || !context->internal ||
	    !context->internal->_initialized) {

		errno = EINVAL;  /* Best I can think of ... */
		TALLOC_FREE(frame);
		return NULL;

	}

	if (!fname) {

		errno = EINVAL;
		TALLOC_FREE(frame);
		return NULL;

	}

	if (smbc_parse_path(frame,
				context,
				fname,
				&workgroup,
				&server,
				&share,
				&path,
				&user,
				&password,
				NULL)) {
		errno = EINVAL;
		TALLOC_FREE(frame);
		return NULL;
        }

	if (!user || user[0] == (char)0) {
		user = talloc_strdup(frame, context->user);
		if (!user) {
                	errno = ENOMEM;
			TALLOC_FREE(frame);
			return NULL;
		}
	}

	srv = smbc_server(frame, context, True,
                          server, share, &workgroup, &user, &password);

	if (!srv) {
		if (errno == EPERM) errno = EACCES;
		TALLOC_FREE(frame);
		return NULL;  /* smbc_server sets errno */
	}

	/* Hmmm, the test for a directory is suspect here ... FIXME */

	if (strlen(path) > 0 && path[strlen(path) - 1] == '\\') {
		fd = -1;
	} else {
		file = SMB_MALLOC_P(SMBCFILE);

		if (!file) {
			errno = ENOMEM;
			TALLOC_FREE(frame);
			return NULL;
		}

		ZERO_STRUCTP(file);

		/*d_printf(">>>open: resolving %s\n", path);*/
		if (!cli_resolve_path(frame, "", srv->cli, path, &targetcli, &targetpath)) {
			d_printf("Could not resolve %s\n", path);
			SAFE_FREE(file);
			TALLOC_FREE(frame);
			return NULL;
		}
		/*d_printf(">>>open: resolved %s as %s\n", path, targetpath);*/

		if ((fd = cli_open(targetcli, targetpath, flags,
                                   context->internal->_share_mode)) < 0) {

			/* Handle the error ... */

			SAFE_FREE(file);
			errno = smbc_errno(context, targetcli);
			TALLOC_FREE(frame);
			return NULL;

		}

		/* Fill in file struct */

		file->cli_fd  = fd;
		file->fname   = SMB_STRDUP(fname);
		file->srv     = srv;
		file->offset  = 0;
		file->file    = True;

		DLIST_ADD(context->internal->_files, file);

                /*
                 * If the file was opened in O_APPEND mode, all write
                 * operations should be appended to the file.  To do that,
                 * though, using this protocol, would require a getattrE()
                 * call for each and every write, to determine where the end
                 * of the file is. (There does not appear to be an append flag
                 * in the protocol.)  Rather than add all of that overhead of
                 * retrieving the current end-of-file offset prior to each
                 * write operation, we'll assume that most append operations
                 * will continuously write, so we'll just set the offset to
                 * the end of the file now and hope that's adequate.
                 *
                 * Note to self: If this proves inadequate, and O_APPEND
                 * should, in some cases, be forced for each write, add a
                 * field in the context options structure, for
                 * "strict_append_mode" which would select between the current
                 * behavior (if FALSE) or issuing a getattrE() prior to each
                 * write and forcing the write to the end of the file (if
                 * TRUE).  Adding that capability will likely require adding
                 * an "append" flag into the _SMBCFILE structure to track
                 * whether a file was opened in O_APPEND mode.  -- djl
                 */
                if (flags & O_APPEND) {
                        if (smbc_lseek_ctx(context, file, 0, SEEK_END) < 0) {
                                (void) smbc_close_ctx(context, file);
                                errno = ENXIO;
				TALLOC_FREE(frame);
                                return NULL;
                        }
                }

		TALLOC_FREE(frame);
		return file;

	}

	/* Check if opendir needed ... */

	if (fd == -1) {
		int eno = 0;

		eno = smbc_errno(context, srv->cli);
		file = (context->opendir)(context, fname);
		if (!file) errno = eno;
		TALLOC_FREE(frame);
		return file;

	}

	errno = EINVAL; /* FIXME, correct errno ? */
	TALLOC_FREE(frame);
	return NULL;

}

/*
 * Routine to create a file 
 */

static int creat_bits = O_WRONLY | O_CREAT | O_TRUNC; /* FIXME: Do we need this */

static SMBCFILE *
smbc_creat_ctx(SMBCCTX *context,
               const char *path,
               mode_t mode)
{

	if (!context || !context->internal ||
	    !context->internal->_initialized) {

		errno = EINVAL;
		return NULL;

	}

	return smbc_open_ctx(context, path, creat_bits, mode);
}

/*
 * Routine to read() a file ...
 */

static ssize_t
smbc_read_ctx(SMBCCTX *context,
              SMBCFILE *file,
              void *buf,
              size_t count)
{
	int ret;
	char *server = NULL, *share = NULL, *user = NULL, *password = NULL;
	char *path = NULL;
	char *targetpath = NULL;
	struct cli_state *targetcli = NULL;
	TALLOC_CTX *frame = talloc_stackframe();

        /*
         * offset:
         *
         * Compiler bug (possibly) -- gcc (GCC) 3.3.5 (Debian 1:3.3.5-2) --
         * appears to pass file->offset (which is type off_t) differently than
         * a local variable of type off_t.  Using local variable "offset" in
         * the call to cli_read() instead of file->offset fixes a problem
         * retrieving data at an offset greater than 4GB.
         */
        off_t offset;

	if (!context || !context->internal ||
	    !context->internal->_initialized) {
		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;

	}

	DEBUG(4, ("smbc_read(%p, %d)\n", file, (int)count));

	if (!file || !DLIST_CONTAINS(context->internal->_files, file)) {
		errno = EBADF;
		TALLOC_FREE(frame);
		return -1;

	}

	offset = file->offset;

	/* Check that the buffer exists ... */

	if (buf == NULL) {
		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;

	}

	/*d_printf(">>>read: parsing %s\n", file->fname);*/
	if (smbc_parse_path(frame,
				context,
				file->fname,
				NULL,
				&server,
				&share,
				&path,
				&user,
				&password,
				NULL)) {
                errno = EINVAL;
		TALLOC_FREE(frame);
                return -1;
        }

	/*d_printf(">>>read: resolving %s\n", path);*/
	if (!cli_resolve_path(frame, "", file->srv->cli, path,
                              &targetcli, &targetpath)) {
		d_printf("Could not resolve %s\n", path);
		TALLOC_FREE(frame);
		return -1;
	}
	/*d_printf(">>>fstat: resolved path as %s\n", targetpath);*/

	ret = cli_read(targetcli, file->cli_fd, (char *)buf, offset, count);

	if (ret < 0) {

		errno = smbc_errno(context, targetcli);
		TALLOC_FREE(frame);
		return -1;

	}

	file->offset += ret;

	DEBUG(4, ("  --> %d\n", ret));

	TALLOC_FREE(frame);
	return ret;  /* Success, ret bytes of data ... */

}

/*
 * Routine to write() a file ...
 */

static ssize_t
smbc_write_ctx(SMBCCTX *context,
               SMBCFILE *file,
               void *buf,
               size_t count)
{
	int ret;
        off_t offset;
	char *server = NULL, *share = NULL, *user = NULL, *password = NULL;
	char *path = NULL;
	char *targetpath = NULL;
	struct cli_state *targetcli = NULL;
	TALLOC_CTX *frame = talloc_stackframe();

	/* First check all pointers before dereferencing them */

	if (!context || !context->internal ||
	    !context->internal->_initialized) {
		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;

	}

	if (!file || !DLIST_CONTAINS(context->internal->_files, file)) {
		errno = EBADF;
		TALLOC_FREE(frame);
		return -1;
	}

	/* Check that the buffer exists ... */

	if (buf == NULL) {
		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;

	}

        offset = file->offset; /* See "offset" comment in smbc_read_ctx() */

	/*d_printf(">>>write: parsing %s\n", file->fname);*/
	if (smbc_parse_path(frame,
				context,
				file->fname,
				NULL,
				&server,
				&share,
				&path,
				&user,
				&password,
				NULL)) {
                errno = EINVAL;
		TALLOC_FREE(frame);
                return -1;
        }

	/*d_printf(">>>write: resolving %s\n", path);*/
	if (!cli_resolve_path(frame, "", file->srv->cli, path,
                              &targetcli, &targetpath)) {
		d_printf("Could not resolve %s\n", path);
		TALLOC_FREE(frame);
		return -1;
	}
	/*d_printf(">>>write: resolved path as %s\n", targetpath);*/

	ret = cli_write(targetcli, file->cli_fd, 0, (char *)buf, offset, count);

	if (ret <= 0) {
		errno = smbc_errno(context, targetcli);
		TALLOC_FREE(frame);
		return -1;

	}

	file->offset += ret;

	TALLOC_FREE(frame);
	return ret;  /* Success, 0 bytes of data ... */
}

/*
 * Routine to close() a file ...
 */

static int
smbc_close_ctx(SMBCCTX *context,
               SMBCFILE *file)
{
        SMBCSRV *srv;
	char *server = NULL, *share = NULL, *user = NULL, *password = NULL;
	char *path = NULL;
	char *targetpath = NULL;
	struct cli_state *targetcli = NULL;
	TALLOC_CTX *frame = talloc_stackframe();

	if (!context || !context->internal ||
	    !context->internal->_initialized) {

		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;
	}

	if (!file || !DLIST_CONTAINS(context->internal->_files, file)) {
		errno = EBADF;
		TALLOC_FREE(frame);
		return -1;
	}

	/* IS a dir ... */
	if (!file->file) {
		TALLOC_FREE(frame);
		return (context->closedir)(context, file);
	}

	/*d_printf(">>>close: parsing %s\n", file->fname);*/
	if (smbc_parse_path(frame,
				context,
				file->fname,
				NULL,
				&server,
				&share,
				&path,
				&user,
				&password,
				NULL)) {
                errno = EINVAL;
		TALLOC_FREE(frame);
                return -1;
        }

	/*d_printf(">>>close: resolving %s\n", path);*/
	if (!cli_resolve_path(frame, "", file->srv->cli, path,
                              &targetcli, &targetpath)) {
		d_printf("Could not resolve %s\n", path);
		TALLOC_FREE(frame);
		return -1;
	}
	/*d_printf(">>>close: resolved path as %s\n", targetpath);*/

	if (!cli_close(targetcli, file->cli_fd)) {

		DEBUG(3, ("cli_close failed on %s. purging server.\n", 
			  file->fname));
		/* Deallocate slot and remove the server 
		 * from the server cache if unused */
		errno = smbc_errno(context, targetcli);
		srv = file->srv;
		DLIST_REMOVE(context->internal->_files, file);
		SAFE_FREE(file->fname);
		SAFE_FREE(file);
		(context->callbacks.remove_unused_server_fn)(context, srv);
		TALLOC_FREE(frame);
		return -1;

	}

	DLIST_REMOVE(context->internal->_files, file);
	SAFE_FREE(file->fname);
	SAFE_FREE(file);
	TALLOC_FREE(frame);

	return 0;
}

/*
 * Get info from an SMB server on a file. Use a qpathinfo call first
 * and if that fails, use getatr, as Win95 sometimes refuses qpathinfo
 */
static bool
smbc_getatr(SMBCCTX * context,
            SMBCSRV *srv,
            char *path,
            uint16 *mode,
            SMB_OFF_T *size,
            struct timespec *create_time_ts,
            struct timespec *access_time_ts,
            struct timespec *write_time_ts,
            struct timespec *change_time_ts,
            SMB_INO_T *ino)
{
	char *fixedpath = NULL;
	char *targetpath = NULL;
	struct cli_state *targetcli = NULL;
	time_t write_time;
	TALLOC_CTX *frame = talloc_stackframe();

	if (!context || !context->internal ||
	    !context->internal->_initialized) {
		errno = EINVAL;
		TALLOC_FREE(frame);
 		return -1;
 	}

	/* path fixup for . and .. */
	if (strequal(path, ".") || strequal(path, "..")) {
		fixedpath = talloc_strdup(frame, "\\");
		if (!fixedpath) {
			errno = ENOMEM;
			TALLOC_FREE(frame);
			return -1;
		}
	} else {
		fixedpath = talloc_strdup(frame, path);
		if (!fixedpath) {
			errno = ENOMEM;
			TALLOC_FREE(frame);
			return -1;
		}
		trim_string(fixedpath, NULL, "\\..");
		trim_string(fixedpath, NULL, "\\.");
	}
	DEBUG(4,("smbc_getatr: sending qpathinfo\n"));

	if (!cli_resolve_path(frame, "", srv->cli, fixedpath,
				&targetcli, &targetpath)) {
		d_printf("Couldn't resolve %s\n", path);
		TALLOC_FREE(frame);
		return False;
	}

	if (!srv->no_pathinfo2 &&
            cli_qpathinfo2(targetcli, targetpath,
                           create_time_ts,
                           access_time_ts,
                           write_time_ts,
                           change_time_ts,
                           size, mode, ino)) {
		TALLOC_FREE(frame);
		return True;
        }

	/* if this is NT then don't bother with the getatr */
	if (targetcli->capabilities & CAP_NT_SMBS) {
                errno = EPERM;
		TALLOC_FREE(frame);
                return False;
        }

	if (cli_getatr(targetcli, targetpath, mode, size, &write_time)) {

                struct timespec w_time_ts;

                w_time_ts = convert_time_t_to_timespec(write_time);

                if (write_time_ts != NULL) {
			*write_time_ts = w_time_ts;
                }

                if (create_time_ts != NULL) {
                        *create_time_ts = w_time_ts;
                }

                if (access_time_ts != NULL) {
                        *access_time_ts = w_time_ts;
                }

                if (change_time_ts != NULL) {
                        *change_time_ts = w_time_ts;
                }

		srv->no_pathinfo2 = True;
		TALLOC_FREE(frame);
		return True;
	}

        errno = EPERM;
	TALLOC_FREE(frame);
	return False;

}

/*
 * Set file info on an SMB server.  Use setpathinfo call first.  If that
 * fails, use setattrE..
 *
 * Access and modification time parameters are always used and must be
 * provided.  Create time, if zero, will be determined from the actual create
 * time of the file.  If non-zero, the create time will be set as well.
 *
 * "mode" (attributes) parameter may be set to -1 if it is not to be set.
 */
static bool
smbc_setatr(SMBCCTX * context, SMBCSRV *srv, char *path, 
            time_t create_time,
            time_t access_time,
            time_t write_time,
            time_t change_time,
            uint16 mode)
{
        int fd;
        int ret;
	TALLOC_CTX *frame = talloc_stackframe();

        /*
         * First, try setpathinfo (if qpathinfo succeeded), for it is the
         * modern function for "new code" to be using, and it works given a
         * filename rather than requiring that the file be opened to have its
         * attributes manipulated.
         */
        if (srv->no_pathinfo ||
            ! cli_setpathinfo(srv->cli, path,
                              create_time,
                              access_time,
                              write_time,
                              change_time,
                              mode)) {

                /*
                 * setpathinfo is not supported; go to plan B. 
                 *
                 * cli_setatr() does not work on win98, and it also doesn't
                 * support setting the access time (only the modification
                 * time), so in all cases, we open the specified file and use
                 * cli_setattrE() which should work on all OS versions, and
                 * supports both times.
                 */

                /* Don't try {q,set}pathinfo() again, with this server */
                srv->no_pathinfo = True;

                /* Open the file */
                if ((fd = cli_open(srv->cli, path, O_RDWR, DENY_NONE)) < 0) {

                        errno = smbc_errno(context, srv->cli);
			TALLOC_FREE(frame);
                        return -1;
                }

                /* Set the new attributes */
                ret = cli_setattrE(srv->cli, fd,
                                   change_time,
                                   access_time,
                                   write_time);

                /* Close the file */
                cli_close(srv->cli, fd);

                /*
                 * Unfortunately, setattrE() doesn't have a provision for
                 * setting the access mode (attributes).  We'll have to try
                 * cli_setatr() for that, and with only this parameter, it
                 * seems to work on win98.
                 */
                if (ret && mode != (uint16) -1) {
                        ret = cli_setatr(srv->cli, path, mode, 0);
                }

                if (! ret) {
                        errno = smbc_errno(context, srv->cli);
			TALLOC_FREE(frame);
                        return False;
                }
        }

	TALLOC_FREE(frame);
        return True;
}

 /*
  * Routine to unlink() a file
  */

static int
smbc_unlink_ctx(SMBCCTX *context,
                const char *fname)
{
	char *server = NULL, *share = NULL, *user = NULL, *password = NULL, *workgroup = NULL;
	char *path = NULL;
	char *targetpath = NULL;
	struct cli_state *targetcli = NULL;
	SMBCSRV *srv = NULL;
	TALLOC_CTX *frame = talloc_stackframe();

	if (!context || !context->internal ||
	    !context->internal->_initialized) {
		errno = EINVAL;  /* Best I can think of ... */
		TALLOC_FREE(frame);
		return -1;

	}

	if (!fname) {
		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;

	}

	if (smbc_parse_path(frame,
				context,
				fname,
				&workgroup,
				&server,
				&share,
				&path,
				&user,
				&password,
				NULL)) {
                errno = EINVAL;
		TALLOC_FREE(frame);
                return -1;
        }

	if (!user || user[0] == (char)0) {
		user = talloc_strdup(frame, context->user);
		if (!user) {
			errno = ENOMEM;
			TALLOC_FREE(frame);
			return -1;
		}
	}

	srv = smbc_server(frame, context, True,
                          server, share, &workgroup, &user, &password);

	if (!srv) {
		TALLOC_FREE(frame);
		return -1;  /* smbc_server sets errno */

	}

	/*d_printf(">>>unlink: resolving %s\n", path);*/
	if (!cli_resolve_path(frame, "", srv->cli, path,
				&targetcli, &targetpath)) {
		d_printf("Could not resolve %s\n", path);
		TALLOC_FREE(frame);
		return -1;
	}
	/*d_printf(">>>unlink: resolved path as %s\n", targetpath);*/

	if (!cli_unlink(targetcli, targetpath)) {

		errno = smbc_errno(context, targetcli);

		if (errno == EACCES) { /* Check if the file is a directory */

			int saverr = errno;
			SMB_OFF_T size = 0;
			uint16 mode = 0;
			struct timespec write_time_ts;
                        struct timespec access_time_ts;
                        struct timespec change_time_ts;
			SMB_INO_T ino = 0;

			if (!smbc_getatr(context, srv, path, &mode, &size,
					 NULL,
                                         &access_time_ts,
                                         &write_time_ts,
                                         &change_time_ts,
                                         &ino)) {

				/* Hmmm, bad error ... What? */

				errno = smbc_errno(context, targetcli);
				TALLOC_FREE(frame);
				return -1;

			}
			else {

				if (IS_DOS_DIR(mode))
					errno = EISDIR;
				else
					errno = saverr;  /* Restore this */

			}
		}

		TALLOC_FREE(frame);
		return -1;

	}

	TALLOC_FREE(frame);
	return 0;  /* Success ... */

}

/*
 * Routine to rename() a file
 */

static int
smbc_rename_ctx(SMBCCTX *ocontext,
                const char *oname, 
                SMBCCTX *ncontext,
                const char *nname)
{
	char *server1 = NULL;
        char *share1 = NULL;
        char *server2 = NULL;
        char *share2 = NULL;
        char *user1 = NULL;
        char *user2 = NULL;
        char *password1 = NULL;
        char *password2 = NULL;
        char *workgroup = NULL;
	char *path1 = NULL;
        char *path2 = NULL;
        char *targetpath1 = NULL;
        char *targetpath2 = NULL;
	struct cli_state *targetcli1 = NULL;
        struct cli_state *targetcli2 = NULL;
	SMBCSRV *srv = NULL;
	TALLOC_CTX *frame = talloc_stackframe();

	if (!ocontext || !ncontext ||
	    !ocontext->internal || !ncontext->internal ||
	    !ocontext->internal->_initialized ||
	    !ncontext->internal->_initialized) {
		errno = EINVAL;  /* Best I can think of ... */
		TALLOC_FREE(frame);
		return -1;
	}

	if (!oname || !nname) {
		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;
	}

	DEBUG(4, ("smbc_rename(%s,%s)\n", oname, nname));

	if (smbc_parse_path(frame,
			ocontext,
			oname,
			&workgroup,
			&server1,
			&share1,
			&path1,
			&user1,
			&password1,
                        NULL)) {
                errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;
	}

	if (!user1 || user1[0] == (char)0) {
		user1 = talloc_strdup(frame, ocontext->user);
		if (!user1) {
                	errno = ENOMEM;
			TALLOC_FREE(frame);
			return -1;
		}
	}

	if (smbc_parse_path(frame,
				ncontext,
				nname,
				NULL,
				&server2,
				&share2,
				&path2,
				&user2,
				&password2,
				NULL)) {
                errno = EINVAL;
		TALLOC_FREE(frame);
                return -1;
	}

	if (!user2 || user2[0] == (char)0) {
		user2 = talloc_strdup(frame, ncontext->user);
		if (!user2) {
                	errno = ENOMEM;
			TALLOC_FREE(frame);
			return -1;
		}
	}

	if (strcmp(server1, server2) || strcmp(share1, share2) ||
	    strcmp(user1, user2)) {
		/* Can't rename across file systems, or users?? */
		errno = EXDEV;
		TALLOC_FREE(frame);
		return -1;
	}

	srv = smbc_server(frame, ocontext, True,
                          server1, share1, &workgroup, &user1, &password1);
	if (!srv) {
		TALLOC_FREE(frame);
		return -1;

	}

	/*d_printf(">>>rename: resolving %s\n", path1);*/
	if (!cli_resolve_path(frame, "", srv->cli, path1,
				&targetcli1, &targetpath1)) {
		d_printf("Could not resolve %s\n", path1);
		TALLOC_FREE(frame);
		return -1;
	}
	/*d_printf(">>>rename: resolved path as %s\n", targetpath1);*/
	/*d_printf(">>>rename: resolving %s\n", path2);*/
	if (!cli_resolve_path(frame, "", srv->cli, path2,
				&targetcli2, &targetpath2)) {
		d_printf("Could not resolve %s\n", path2);
		TALLOC_FREE(frame);
		return -1;
	}
	/*d_printf(">>>rename: resolved path as %s\n", targetpath2);*/

	if (strcmp(targetcli1->desthost, targetcli2->desthost) ||
            strcmp(targetcli1->share, targetcli2->share))
	{
		/* can't rename across file systems */
		errno = EXDEV;
		TALLOC_FREE(frame);
		return -1;
	}

	if (!cli_rename(targetcli1, targetpath1, targetpath2)) {
		int eno = smbc_errno(ocontext, targetcli1);

		if (eno != EEXIST ||
		    !cli_unlink(targetcli1, targetpath2) ||
		    !cli_rename(targetcli1, targetpath1, targetpath2)) {

			errno = eno;
			TALLOC_FREE(frame);
			return -1;

		}
	}

	TALLOC_FREE(frame);
	return 0; /* Success */
}

/*
 * A routine to lseek() a file
 */

static off_t
smbc_lseek_ctx(SMBCCTX *context,
               SMBCFILE *file,
               off_t offset,
               int whence)
{
	SMB_OFF_T size;
	char *server = NULL, *share = NULL, *user = NULL, *password = NULL;
	char *path = NULL;
	char *targetpath = NULL;
	struct cli_state *targetcli = NULL;
	TALLOC_CTX *frame = talloc_stackframe();

	if (!context || !context->internal ||
	    !context->internal->_initialized) {
		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;
	}

	if (!file || !DLIST_CONTAINS(context->internal->_files, file)) {

		errno = EBADF;
		TALLOC_FREE(frame);
		return -1;

	}

	if (!file->file) {

		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;      /* Can't lseek a dir ... */

	}

	switch (whence) {
	case SEEK_SET:
		file->offset = offset;
		break;

	case SEEK_CUR:
		file->offset += offset;
		break;

	case SEEK_END:
		/*d_printf(">>>lseek: parsing %s\n", file->fname);*/
		if (smbc_parse_path(frame,
					context,
					file->fname,
					NULL,
					&server,
					&share,
					&path,
					&user,
					&password,
					NULL)) {
			errno = EINVAL;
			TALLOC_FREE(frame);
			return -1;
		}

		/*d_printf(">>>lseek: resolving %s\n", path);*/
		if (!cli_resolve_path(frame, "", file->srv->cli, path,
                                      &targetcli, &targetpath)) {
			d_printf("Could not resolve %s\n", path);
			TALLOC_FREE(frame);
			return -1;
		}
		/*d_printf(">>>lseek: resolved path as %s\n", targetpath);*/

		if (!cli_qfileinfo(targetcli, file->cli_fd, NULL,
                                   &size, NULL, NULL, NULL, NULL, NULL))
		{
		    SMB_OFF_T b_size = size;
			if (!cli_getattrE(targetcli, file->cli_fd,
                                          NULL, &b_size, NULL, NULL, NULL))
		    {
			errno = EINVAL;
			TALLOC_FREE(frame);
			return -1;
		    } else
			size = b_size;
		}
		file->offset = size + offset;
		break;

	default:
		errno = EINVAL;
		break;

	}

	TALLOC_FREE(frame);
	return file->offset;

}

/* 
 * Generate an inode number from file name for those things that need it
 */

static ino_t
smbc_inode(SMBCCTX *context,
           const char *name)
{
	if (!context || !context->internal ||
	    !context->internal->_initialized) {

		errno = EINVAL;
		return -1;

	}

	if (!*name) return 2; /* FIXME, why 2 ??? */
	return (ino_t)str_checksum(name);

}

/*
 * Routine to put basic stat info into a stat structure ... Used by stat and
 * fstat below.
 */

static int
smbc_setup_stat(SMBCCTX *context,
                struct stat *st,
                char *fname,
                SMB_OFF_T size,
                int mode)
{
	TALLOC_CTX *frame = talloc_stackframe();
	
	st->st_mode = 0;

	if (IS_DOS_DIR(mode)) {
		st->st_mode = SMBC_DIR_MODE;
	} else {
		st->st_mode = SMBC_FILE_MODE;
	}

	if (IS_DOS_ARCHIVE(mode)) st->st_mode |= S_IXUSR;
	if (IS_DOS_SYSTEM(mode)) st->st_mode |= S_IXGRP;
	if (IS_DOS_HIDDEN(mode)) st->st_mode |= S_IXOTH;
	if (!IS_DOS_READONLY(mode)) st->st_mode |= S_IWUSR;

	st->st_size = size;
#ifdef HAVE_STAT_ST_BLKSIZE
	st->st_blksize = 512;
#endif
#ifdef HAVE_STAT_ST_BLOCKS
	st->st_blocks = (size+511)/512;
#endif
#ifdef HAVE_STRUCT_STAT_ST_RDEV
	st->st_rdev = 0;
#endif
	st->st_uid = getuid();
	st->st_gid = getgid();

	if (IS_DOS_DIR(mode)) {
		st->st_nlink = 2;
	} else {
		st->st_nlink = 1;
	}

	if (st->st_ino == 0) {
		st->st_ino = smbc_inode(context, fname);
	}
	
	TALLOC_FREE(frame);
	return True;  /* FIXME: Is this needed ? */

}

/*
 * Routine to stat a file given a name
 */

static int
smbc_stat_ctx(SMBCCTX *context,
              const char *fname,
              struct stat *st)
{
	SMBCSRV *srv = NULL;
	char *server = NULL;
	char *share = NULL;
	char *user = NULL;
	char *password = NULL;
	char *workgroup = NULL;
	char *path = NULL;
	struct timespec write_time_ts;
        struct timespec access_time_ts;
        struct timespec change_time_ts;
	SMB_OFF_T size = 0;
	uint16 mode = 0;
	SMB_INO_T ino = 0;
	TALLOC_CTX *frame = talloc_stackframe();

	if (!context || !context->internal ||
	    !context->internal->_initialized) {

		errno = EINVAL;  /* Best I can think of ... */
		TALLOC_FREE(frame);
		return -1;
	}

	if (!fname) {
		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;
	}

	DEBUG(4, ("smbc_stat(%s)\n", fname));

	if (smbc_parse_path(frame,
				context,
				fname,
				&workgroup,
				&server,
				&share,
				&path,
				&user,
				&password,
				NULL)) {
		errno = EINVAL;
		TALLOC_FREE(frame);
                return -1;
        }

	if (!user || user[0] == (char)0) {
		user = talloc_strdup(frame,context->user);
		if (!user) {
			errno = ENOMEM;
			TALLOC_FREE(frame);
			return -1;
		}
	}

	srv = smbc_server(frame, context, True,
                          server, share, &workgroup, &user, &password);

	if (!srv) {
		TALLOC_FREE(frame);
		return -1;  /* errno set by smbc_server */
	}

	if (!smbc_getatr(context, srv, path, &mode, &size,
			 NULL,
                         &access_time_ts,
                         &write_time_ts,
                         &change_time_ts,
                         &ino)) {
		errno = smbc_errno(context, srv->cli);
		TALLOC_FREE(frame);
		return -1;
	}

	st->st_ino = ino;

	smbc_setup_stat(context, st, (char *) fname, size, mode);

	set_atimespec(st, access_time_ts);
	set_ctimespec(st, change_time_ts);
	set_mtimespec(st, write_time_ts);
	st->st_dev   = srv->dev;

	TALLOC_FREE(frame);
	return 0;

}

/*
 * Routine to stat a file given an fd
 */

static int
smbc_fstat_ctx(SMBCCTX *context,
               SMBCFILE *file,
               struct stat *st)
{
	struct timespec change_time_ts;
        struct timespec access_time_ts;
        struct timespec write_time_ts;
	SMB_OFF_T size;
	uint16 mode;
	char *server = NULL;
	char *share = NULL;
	char *user = NULL;
	char *password = NULL;
	char *path = NULL;
        char *targetpath = NULL;
	struct cli_state *targetcli = NULL;
	SMB_INO_T ino = 0;
	TALLOC_CTX *frame = talloc_stackframe();

	if (!context || !context->internal ||
	    !context->internal->_initialized) {
		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;
	}

	if (!file || !DLIST_CONTAINS(context->internal->_files, file)) {
		errno = EBADF;
		TALLOC_FREE(frame);
		return -1;
	}

	if (!file->file) {
		TALLOC_FREE(frame);
		return (context->fstatdir)(context, file, st);
	}

	/*d_printf(">>>fstat: parsing %s\n", file->fname);*/
	if (smbc_parse_path(frame,
				context,
				file->fname,
				NULL,
				&server,
				&share,
				&path,
				&user,
				&password,
				NULL)) {
                errno = EINVAL;
		TALLOC_FREE(frame);
                return -1;
        }

	/*d_printf(">>>fstat: resolving %s\n", path);*/
	if (!cli_resolve_path(frame, "", file->srv->cli, path,
                              &targetcli, &targetpath)) {
		d_printf("Could not resolve %s\n", path);
		TALLOC_FREE(frame);
		return -1;
	}
	/*d_printf(">>>fstat: resolved path as %s\n", targetpath);*/

	if (!cli_qfileinfo(targetcli, file->cli_fd, &mode, &size,
                           NULL,
                           &access_time_ts,
                           &write_time_ts,
                           &change_time_ts,
                           &ino)) {

		time_t change_time, access_time, write_time;

		if (!cli_getattrE(targetcli, file->cli_fd, &mode, &size,
				&change_time, &access_time, &write_time)) {

			errno = EINVAL;
			TALLOC_FREE(frame);
			return -1;
		}

		change_time_ts = convert_time_t_to_timespec(change_time);
		access_time_ts = convert_time_t_to_timespec(access_time);
		write_time_ts = convert_time_t_to_timespec(write_time);
	}

	st->st_ino = ino;

	smbc_setup_stat(context, st, file->fname, size, mode);

	set_atimespec(st, access_time_ts);
	set_ctimespec(st, change_time_ts);
	set_mtimespec(st, write_time_ts);
	st->st_dev = file->srv->dev;

	TALLOC_FREE(frame);
	return 0;

}

/*
 * Routine to open a directory
 * We accept the URL syntax explained in smbc_parse_path(), above.
 */

static void
smbc_remove_dir(SMBCFILE *dir)
{
	struct smbc_dir_list *d,*f;

	d = dir->dir_list;
	while (d) {

		f = d; d = d->next;

		SAFE_FREE(f->dirent);
		SAFE_FREE(f);

	}

	dir->dir_list = dir->dir_end = dir->dir_next = NULL;

}

static int
add_dirent(SMBCFILE *dir,
           const char *name,
           const char *comment,
           uint32 type)
{
	struct smbc_dirent *dirent;
	int size;
        int name_length = (name == NULL ? 0 : strlen(name));
        int comment_len = (comment == NULL ? 0 : strlen(comment));

	/*
	 * Allocate space for the dirent, which must be increased by the 
	 * size of the name and the comment and 1 each for the null terminator.
	 */

	size = sizeof(struct smbc_dirent) + name_length + comment_len + 2;
    
	dirent = (struct smbc_dirent *)SMB_MALLOC(size);

	if (!dirent) {

		dir->dir_error = ENOMEM;
		return -1;

	}

	ZERO_STRUCTP(dirent);

	if (dir->dir_list == NULL) {

		dir->dir_list = SMB_MALLOC_P(struct smbc_dir_list);
		if (!dir->dir_list) {

			SAFE_FREE(dirent);
			dir->dir_error = ENOMEM;
			return -1;

		}
		ZERO_STRUCTP(dir->dir_list);

		dir->dir_end = dir->dir_next = dir->dir_list;
	}
	else {

		dir->dir_end->next = SMB_MALLOC_P(struct smbc_dir_list);
		
		if (!dir->dir_end->next) {
			
			SAFE_FREE(dirent);
			dir->dir_error = ENOMEM;
			return -1;

		}
		ZERO_STRUCTP(dir->dir_end->next);

		dir->dir_end = dir->dir_end->next;
	}

	dir->dir_end->next = NULL;
	dir->dir_end->dirent = dirent;
	
	dirent->smbc_type = type;
	dirent->namelen = name_length;
	dirent->commentlen = comment_len;
	dirent->dirlen = size;
  
        /*
         * dirent->namelen + 1 includes the null (no null termination needed)
         * Ditto for dirent->commentlen.
         * The space for the two null bytes was allocated.
         */
	strncpy(dirent->name, (name?name:""), dirent->namelen + 1);
	dirent->comment = (char *)(&dirent->name + dirent->namelen + 1);
	strncpy(dirent->comment, (comment?comment:""), dirent->commentlen + 1);
	
	return 0;

}

static void
list_unique_wg_fn(const char *name,
                  uint32 type,
                  const char *comment,
                  void *state)
{
	SMBCFILE *dir = (SMBCFILE *)state;
        struct smbc_dir_list *dir_list;
        struct smbc_dirent *dirent;
	int dirent_type;
        int do_remove = 0;

	dirent_type = dir->dir_type;

	if (add_dirent(dir, name, comment, dirent_type) < 0) {

		/* An error occurred, what do we do? */
		/* FIXME: Add some code here */
	}

        /* Point to the one just added */
        dirent = dir->dir_end->dirent;

        /* See if this was a duplicate */
        for (dir_list = dir->dir_list;
             dir_list != dir->dir_end;
             dir_list = dir_list->next) {
                if (! do_remove &&
                    strcmp(dir_list->dirent->name, dirent->name) == 0) {
                        /* Duplicate.  End end of list need to be removed. */
                        do_remove = 1;
                }

                if (do_remove && dir_list->next == dir->dir_end) {
                        /* Found the end of the list.  Remove it. */
                        dir->dir_end = dir_list;
                        free(dir_list->next);
                        free(dirent);
                        dir_list->next = NULL;
                        break;
                }
        }
}

static void
list_fn(const char *name,
        uint32 type,
        const char *comment,
        void *state)
{
	SMBCFILE *dir = (SMBCFILE *)state;
	int dirent_type;

	/*
         * We need to process the type a little ...
         *
         * Disk share     = 0x00000000
         * Print share    = 0x00000001
         * Comms share    = 0x00000002 (obsolete?)
         * IPC$ share     = 0x00000003
         *
         * administrative shares:
         * ADMIN$, IPC$, C$, D$, E$ ...  are type |= 0x80000000
         */

	if (dir->dir_type == SMBC_FILE_SHARE) {
		switch (type) {
                case 0 | 0x80000000:
		case 0:
			dirent_type = SMBC_FILE_SHARE;
			break;

		case 1:
			dirent_type = SMBC_PRINTER_SHARE;
			break;

		case 2:
			dirent_type = SMBC_COMMS_SHARE;
			break;

                case 3 | 0x80000000:
		case 3:
			dirent_type = SMBC_IPC_SHARE;
			break;

		default:
			dirent_type = SMBC_FILE_SHARE; /* FIXME, error? */
			break;
		}
	}
	else {
                dirent_type = dir->dir_type;
        }

	if (add_dirent(dir, name, comment, dirent_type) < 0) {

		/* An error occurred, what do we do? */
		/* FIXME: Add some code here */

	}
}

static void
dir_list_fn(const char *mnt,
            file_info *finfo,
            const char *mask,
            void *state)
{

	if (add_dirent((SMBCFILE *)state, finfo->name, "", 
		       (finfo->mode&aDIR?SMBC_DIR:SMBC_FILE)) < 0) {

		/* Handle an error ... */

		/* FIXME: Add some code ... */

	} 

}

static int
net_share_enum_rpc(struct cli_state *cli,
                   void (*fn)(const char *name,
                              uint32 type,
                              const char *comment,
                              void *state),
                   void *state)
{
        int i;
	WERROR result;
	ENUM_HND enum_hnd;
        uint32 info_level = 1;
	uint32 preferred_len = 0xffffffff;
        uint32 type;
	SRV_SHARE_INFO_CTR ctr;
	fstring name = "";
        fstring comment = "";
	struct rpc_pipe_client *pipe_hnd;
        NTSTATUS nt_status;

        /* Open the server service pipe */
        pipe_hnd = cli_rpc_pipe_open_noauth(cli, PI_SRVSVC, &nt_status);
        if (!pipe_hnd) {
                DEBUG(1, ("net_share_enum_rpc pipe open fail!\n"));
                return -1;
        }

        /* Issue the NetShareEnum RPC call and retrieve the response */
	init_enum_hnd(&enum_hnd, 0);
	result = rpccli_srvsvc_net_share_enum(pipe_hnd,
                                              talloc_tos(),
                                              info_level,
                                              &ctr,
                                              preferred_len,
                                              &enum_hnd);

        /* Was it successful? */
	if (!W_ERROR_IS_OK(result) || ctr.num_entries == 0) {
                /*  Nope.  Go clean up. */
		goto done;
        }

        /* For each returned entry... */
        for (i = 0; i < ctr.num_entries; i++) {

                /* pull out the share name */
                rpcstr_pull_unistr2_fstring(
                        name, &ctr.share.info1[i].info_1_str.uni_netname);

                /* pull out the share's comment */
                rpcstr_pull_unistr2_fstring(
                        comment, &ctr.share.info1[i].info_1_str.uni_remark);

                /* Get the type value */
                type = ctr.share.info1[i].info_1.type;

                /* Add this share to the list */
                (*fn)(name, type, comment, state);
        }

done:
        /* Close the server service pipe */
        cli_rpc_pipe_close(pipe_hnd);

        /* Tell 'em if it worked */
        return W_ERROR_IS_OK(result) ? 0 : -1;
}



static SMBCFILE *
smbc_opendir_ctx(SMBCCTX *context,
                 const char *fname)
{
        int saved_errno;
	char *server = NULL, *share = NULL, *user = NULL, *password = NULL, *options = NULL;
	char *workgroup = NULL;
	char *path = NULL;
        uint16 mode;
        char *p = NULL;
	SMBCSRV *srv  = NULL;
	SMBCFILE *dir = NULL;
        struct _smbc_callbacks *cb = NULL;
	struct sockaddr_storage rem_ss;
	TALLOC_CTX *frame = talloc_stackframe();

	if (!context || !context->internal ||
	    !context->internal->_initialized) {
	        DEBUG(4, ("no valid context\n"));
		errno = EINVAL + 8192;
		TALLOC_FREE(frame);
		return NULL;

	}

	if (!fname) {
		DEBUG(4, ("no valid fname\n"));
		errno = EINVAL + 8193;
		TALLOC_FREE(frame);
		return NULL;
	}

	if (smbc_parse_path(frame,
				context,
				fname,
				&workgroup,
				&server,
				&share,
				&path,
				&user,
				&password,
				&options)) {
	        DEBUG(4, ("no valid path\n"));
		errno = EINVAL + 8194;
		TALLOC_FREE(frame);
		return NULL;
	}

	DEBUG(4, ("parsed path: fname='%s' server='%s' share='%s' "
                  "path='%s' options='%s'\n",
                  fname, server, share, path, options));

        /* Ensure the options are valid */
        if (smbc_check_options(server, share, path, options)) {
                DEBUG(4, ("unacceptable options (%s)\n", options));
                errno = EINVAL + 8195;
		TALLOC_FREE(frame);
                return NULL;
        }

	if (!user || user[0] == (char)0) {
		user = talloc_strdup(frame, context->user);
		if (!user) {
			errno = ENOMEM;
			TALLOC_FREE(frame);
			return NULL;
		}
	}

	dir = SMB_MALLOC_P(SMBCFILE);

	if (!dir) {
		errno = ENOMEM;
		TALLOC_FREE(frame);
		return NULL;
	}

	ZERO_STRUCTP(dir);

	dir->cli_fd   = 0;
	dir->fname    = SMB_STRDUP(fname);
	dir->srv      = NULL;
	dir->offset   = 0;
	dir->file     = False;
	dir->dir_list = dir->dir_next = dir->dir_end = NULL;

	if (server[0] == (char)0) {

                int i;
                int count;
                int max_lmb_count;
                struct ip_service *ip_list;
                struct ip_service server_addr;
                struct user_auth_info u_info;

		if (share[0] != (char)0 || path[0] != (char)0) {

			errno = EINVAL + 8196;
			if (dir) {
				SAFE_FREE(dir->fname);
				SAFE_FREE(dir);
			}
			TALLOC_FREE(frame);
			return NULL;
		}

                /* Determine how many local master browsers to query */
                max_lmb_count = (context->options.browse_max_lmb_count == 0
                                 ? INT_MAX
                                 : context->options.browse_max_lmb_count);

		memset(&u_info, '\0', sizeof(u_info));
		u_info.username = talloc_strdup(frame,user);
		u_info.password = talloc_strdup(frame,password);
		if (!u_info.username || !u_info.password) {
			if (dir) {
				SAFE_FREE(dir->fname);
				SAFE_FREE(dir);
			}
			TALLOC_FREE(frame);
			return NULL;
		}

		/*
                 * We have server and share and path empty but options
                 * requesting that we scan all master browsers for their list
                 * of workgroups/domains.  This implies that we must first try
                 * broadcast queries to find all master browsers, and if that
                 * doesn't work, then try our other methods which return only
                 * a single master browser.
                 */

                ip_list = NULL;
                if (!NT_STATUS_IS_OK(name_resolve_bcast(MSBROWSE, 1, &ip_list,
				     &count)))
		{

                        SAFE_FREE(ip_list);

                        if (!find_master_ip(workgroup, &server_addr.ss)) {

				if (dir) {
					SAFE_FREE(dir->fname);
					SAFE_FREE(dir);
				}
                                errno = ENOENT;
				TALLOC_FREE(frame);
                                return NULL;
                        }

			ip_list = (struct ip_service *)memdup(
				&server_addr, sizeof(server_addr));
			if (ip_list == NULL) {
				errno = ENOMEM;
				TALLOC_FREE(frame);
				return NULL;
			}
                        count = 1;
                }

                for (i = 0; i < count && i < max_lmb_count; i++) {
			char addr[INET6_ADDRSTRLEN];
			char *wg_ptr = NULL;
                	struct cli_state *cli = NULL;

			print_sockaddr(addr, sizeof(addr), &ip_list[i].ss);
                        DEBUG(99, ("Found master browser %d of %d: %s\n",
                                   i+1, MAX(count, max_lmb_count),
                                   addr));

                        cli = get_ipc_connect_master_ip(talloc_tos(),
							&ip_list[i],
                                                        &u_info,
							&wg_ptr);
			/* cli == NULL is the master browser refused to talk or
			   could not be found */
			if (!cli) {
				continue;
			}

			workgroup = talloc_strdup(frame, wg_ptr);
			server = talloc_strdup(frame, cli->desthost);

                        cli_shutdown(cli);

			if (!workgroup || !server) {
				errno = ENOMEM;
				TALLOC_FREE(frame);
				return NULL;
			}

                        DEBUG(4, ("using workgroup %s %s\n",
                                  workgroup, server));

                        /*
                         * For each returned master browser IP address, get a
                         * connection to IPC$ on the server if we do not
                         * already have one, and determine the
                         * workgroups/domains that it knows about.
                         */

                        srv = smbc_server(frame, context, True, server, "IPC$",
                                          &workgroup, &user, &password);
                        if (!srv) {
                                continue;
                        }

                        dir->srv = srv;
                        dir->dir_type = SMBC_WORKGROUP;

                        /* Now, list the stuff ... */

                        if (!cli_NetServerEnum(srv->cli,
                                               workgroup,
                                               SV_TYPE_DOMAIN_ENUM,
                                               list_unique_wg_fn,
                                               (void *)dir)) {
                                continue;
                        }
                }

                SAFE_FREE(ip_list);
        } else {
                /*
                 * Server not an empty string ... Check the rest and see what
                 * gives
                 */
		if (*share == '\0') {
			if (*path != '\0') {

                                /* Should not have empty share with path */
				errno = EINVAL + 8197;
				if (dir) {
					SAFE_FREE(dir->fname);
					SAFE_FREE(dir);
				}
				TALLOC_FREE(frame);
				return NULL;

			}

			/*
                         * We don't know if <server> is really a server name
                         * or is a workgroup/domain name.  If we already have
                         * a server structure for it, we'll use it.
                         * Otherwise, check to see if <server><1D>,
                         * <server><1B>, or <server><20> translates.  We check
                         * to see if <server> is an IP address first.
                         */

                        /*
                         * See if we have an existing server.  Do not
                         * establish a connection if one does not already
                         * exist.
                         */
                        srv = smbc_server(frame, context, False, server, "IPC$",
                                          &workgroup, &user, &password);

                        /*
                         * If no existing server and not an IP addr, look for
                         * LMB or DMB
                         */
			if (!srv &&
                            !is_ipaddress(server) &&
			    (resolve_name(server, &rem_ss, 0x1d) ||   /* LMB */
                             resolve_name(server, &rem_ss, 0x1b) )) { /* DMB */

				fstring buserver;

				dir->dir_type = SMBC_SERVER;

				/*
				 * Get the backup list ...
				 */
				if (!name_status_find(server, 0, 0,
                                                      &rem_ss, buserver)) {

                                        DEBUG(0, ("Could not get name of "
                                                  "local/domain master browser "
                                                  "for server %s\n", server));
					if (dir) {
						SAFE_FREE(dir->fname);
						SAFE_FREE(dir);
					}
					errno = EPERM;
					TALLOC_FREE(frame);
					return NULL;

				}

				/*
                                 * Get a connection to IPC$ on the server if
                                 * we do not already have one
                                 */
				srv = smbc_server(frame, context, True,
                                                  buserver, "IPC$",
                                                  &workgroup, &user, &password);
				if (!srv) {
				        DEBUG(0, ("got no contact to IPC$\n"));
					if (dir) {
						SAFE_FREE(dir->fname);
						SAFE_FREE(dir);
					}
					TALLOC_FREE(frame);
					return NULL;

				}

				dir->srv = srv;

				/* Now, list the servers ... */
				if (!cli_NetServerEnum(srv->cli, server,
                                                       0x0000FFFE, list_fn,
						       (void *)dir)) {

					if (dir) {
						SAFE_FREE(dir->fname);
						SAFE_FREE(dir);
					}
					TALLOC_FREE(frame);
					return NULL;
				}
			} else if (srv ||
                                   (resolve_name(server, &rem_ss, 0x20))) {

                                /* If we hadn't found the server, get one now */
                                if (!srv) {
                                        srv = smbc_server(frame, context, True,
                                                          server, "IPC$",
                                                          &workgroup,
                                                          &user, &password);
                                }

                                if (!srv) {
                                        if (dir) {
                                                SAFE_FREE(dir->fname);
                                                SAFE_FREE(dir);
                                        }
					TALLOC_FREE(frame);
                                        return NULL;

                                }

                                dir->dir_type = SMBC_FILE_SHARE;
                                dir->srv = srv;

                                /* List the shares ... */

                                if (net_share_enum_rpc(
                                            srv->cli,
                                            list_fn,
                                            (void *) dir) < 0 &&
                                    cli_RNetShareEnum(
                                            srv->cli,
                                            list_fn,
                                            (void *)dir) < 0) {

                                        errno = cli_errno(srv->cli);
                                        if (dir) {
                                                SAFE_FREE(dir->fname);
                                                SAFE_FREE(dir);
                                        }
					TALLOC_FREE(frame);
                                        return NULL;

                                }
                        } else {
                                /* Neither the workgroup nor server exists */
                                errno = ECONNREFUSED;
                                if (dir) {
                                        SAFE_FREE(dir->fname);
                                        SAFE_FREE(dir);
                                }
				TALLOC_FREE(frame);
                                return NULL;
			}

		}
		else {
                        /*
                         * The server and share are specified ... work from
                         * there ...
                         */
			char *targetpath;
			struct cli_state *targetcli;

			/* We connect to the server and list the directory */
			dir->dir_type = SMBC_FILE_SHARE;

			srv = smbc_server(frame, context, True, server, share,
                                          &workgroup, &user, &password);

			if (!srv) {
				if (dir) {
					SAFE_FREE(dir->fname);
					SAFE_FREE(dir);
				}
				TALLOC_FREE(frame);
				return NULL;
			}

			dir->srv = srv;

			/* Now, list the files ... */

                        p = path + strlen(path);
			path = talloc_asprintf_append(path, "\\*");
			if (!path) {
				if (dir) {
					SAFE_FREE(dir->fname);
					SAFE_FREE(dir);
				}
				TALLOC_FREE(frame);
				return NULL;
			}

			if (!cli_resolve_path(frame, "", srv->cli, path,
                                              &targetcli, &targetpath)) {
				d_printf("Could not resolve %s\n", path);
				if (dir) {
					SAFE_FREE(dir->fname);
					SAFE_FREE(dir);
				}
				TALLOC_FREE(frame);
				return NULL;
			}

			if (cli_list(targetcli, targetpath,
                                     aDIR | aSYSTEM | aHIDDEN,
                                     dir_list_fn, (void *)dir) < 0) {

				if (dir) {
					SAFE_FREE(dir->fname);
					SAFE_FREE(dir);
				}
				saved_errno = smbc_errno(context, targetcli);

                                if (saved_errno == EINVAL) {
                                    /*
                                     * See if they asked to opendir something
                                     * other than a directory.  If so, the
                                     * converted error value we got would have
                                     * been EINVAL rather than ENOTDIR.
                                     */
                                    *p = '\0'; /* restore original path */

                                    if (smbc_getatr(context, srv, path,
                                                    &mode, NULL,
                                                    NULL, NULL, NULL, NULL,
                                                    NULL) &&
                                        ! IS_DOS_DIR(mode)) {

                                        /* It is.  Correct the error value */
                                        saved_errno = ENOTDIR;
                                    }
                                }

                                /*
                                 * If there was an error and the server is no
                                 * good any more...
                                 */
                                cb = &context->callbacks;
                                if (cli_is_error(targetcli) &&
                                    (cb->check_server_fn)(context, srv)) {

                                        /* ... then remove it. */
                                        if ((cb->remove_unused_server_fn)(context,
                                                                          srv)) { 
                                                /*
                                                 * We could not remove the
                                                 * server completely, remove
                                                 * it from the cache so we
                                                 * will not get it again. It
                                                 * will be removed when the
                                                 * last file/dir is closed.
                                                 */
                                                (cb->remove_cached_srv_fn)(context,
                                                                           srv);
                                        }
                                }

                                errno = saved_errno;
				TALLOC_FREE(frame);
				return NULL;
			}
		}

	}

	DLIST_ADD(context->internal->_files, dir);
	TALLOC_FREE(frame);
	return dir;

}

/*
 * Routine to close a directory
 */

static int
smbc_closedir_ctx(SMBCCTX *context,
                  SMBCFILE *dir)
{
	TALLOC_CTX *frame = talloc_stackframe();

        if (!context || !context->internal ||
	    !context->internal->_initialized) {
		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;
	}

	if (!dir || !DLIST_CONTAINS(context->internal->_files, dir)) {
		errno = EBADF;
		TALLOC_FREE(frame);
		return -1;
	}

	smbc_remove_dir(dir); /* Clean it up */

	DLIST_REMOVE(context->internal->_files, dir);

	if (dir) {

		SAFE_FREE(dir->fname);
		SAFE_FREE(dir);    /* Free the space too */
	}

	TALLOC_FREE(frame);
	return 0;

}

static void
smbc_readdir_internal(SMBCCTX * context,
                      struct smbc_dirent *dest,
                      struct smbc_dirent *src,
                      int max_namebuf_len)
{
        if (context->options.urlencode_readdir_entries) {

                /* url-encode the name.  get back remaining buffer space */
                max_namebuf_len =
                        smbc_urlencode(dest->name, src->name, max_namebuf_len);

                /* We now know the name length */
                dest->namelen = strlen(dest->name);

                /* Save the pointer to the beginning of the comment */
                dest->comment = dest->name + dest->namelen + 1;

                /* Copy the comment */
                strncpy(dest->comment, src->comment, max_namebuf_len - 1);
                dest->comment[max_namebuf_len - 1] = '\0';

                /* Save other fields */
                dest->smbc_type = src->smbc_type;
                dest->commentlen = strlen(dest->comment);
                dest->dirlen = ((dest->comment + dest->commentlen + 1) -
                                (char *) dest);
        } else {

                /* No encoding.  Just copy the entry as is. */
                memcpy(dest, src, src->dirlen);
                dest->comment = (char *)(&dest->name + src->namelen + 1);
        }
        
}

/*
 * Routine to get a directory entry
 */

struct smbc_dirent *
smbc_readdir_ctx(SMBCCTX *context,
                 SMBCFILE *dir)
{
        int maxlen;
	struct smbc_dirent *dirp, *dirent;
	TALLOC_CTX *frame = talloc_stackframe();

	/* Check that all is ok first ... */

	if (!context || !context->internal ||
	    !context->internal->_initialized) {

		errno = EINVAL;
                DEBUG(0, ("Invalid context in smbc_readdir_ctx()\n"));
		TALLOC_FREE(frame);
		return NULL;

	}

	if (!dir || !DLIST_CONTAINS(context->internal->_files, dir)) {

		errno = EBADF;
                DEBUG(0, ("Invalid dir in smbc_readdir_ctx()\n"));
		TALLOC_FREE(frame);
		return NULL;

	}

	if (dir->file != False) { /* FIXME, should be dir, perhaps */

		errno = ENOTDIR;
                DEBUG(0, ("Found file vs directory in smbc_readdir_ctx()\n"));
		TALLOC_FREE(frame);
		return NULL;

	}

	if (!dir->dir_next) {
		TALLOC_FREE(frame);
		return NULL;
        }

        dirent = dir->dir_next->dirent;
        if (!dirent) {

                errno = ENOENT;
		TALLOC_FREE(frame);
                return NULL;

        }

        dirp = (struct smbc_dirent *)context->internal->_dirent;
        maxlen = (sizeof(context->internal->_dirent) -
                  sizeof(struct smbc_dirent));

        smbc_readdir_internal(context, dirp, dirent, maxlen);

        dir->dir_next = dir->dir_next->next;

	TALLOC_FREE(frame);
        return dirp;
}

/*
 * Routine to get directory entries
 */

static int
smbc_getdents_ctx(SMBCCTX *context,
                  SMBCFILE *dir,
                  struct smbc_dirent *dirp,
                  int count)
{
	int rem = count;
        int reqd;
        int maxlen;
	char *ndir = (char *)dirp;
	struct smbc_dir_list *dirlist;
	TALLOC_CTX *frame = talloc_stackframe();

	/* Check that all is ok first ... */

	if (!context || !context->internal ||
	    !context->internal->_initialized) {

		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;

	}

	if (!dir || !DLIST_CONTAINS(context->internal->_files, dir)) {

		errno = EBADF;
		TALLOC_FREE(frame);
		return -1;
    
	}

	if (dir->file != False) { /* FIXME, should be dir, perhaps */

		errno = ENOTDIR;
		TALLOC_FREE(frame);
		return -1;

	}

	/* 
	 * Now, retrieve the number of entries that will fit in what was passed
	 * We have to figure out if the info is in the list, or we need to 
	 * send a request to the server to get the info.
	 */

	while ((dirlist = dir->dir_next)) {
		struct smbc_dirent *dirent;

		if (!dirlist->dirent) {

			errno = ENOENT;  /* Bad error */
			TALLOC_FREE(frame);
			return -1;

		}

                /* Do urlencoding of next entry, if so selected */
                dirent = (struct smbc_dirent *)context->internal->_dirent;
                maxlen = (sizeof(context->internal->_dirent) -
                          sizeof(struct smbc_dirent));
                smbc_readdir_internal(context, dirent, dirlist->dirent, maxlen);

                reqd = dirent->dirlen;

		if (rem < reqd) {

			if (rem < count) { /* We managed to copy something */

				errno = 0;
				TALLOC_FREE(frame);
				return count - rem;

			}
			else { /* Nothing copied ... */

				errno = EINVAL;  /* Not enough space ... */
				TALLOC_FREE(frame);
				return -1;

			}

		}

		memcpy(ndir, dirent, reqd); /* Copy the data in ... */
    
		((struct smbc_dirent *)ndir)->comment = 
			(char *)(&((struct smbc_dirent *)ndir)->name +
                                 dirent->namelen +
                                 1);

		ndir += reqd;

		rem -= reqd;

		dir->dir_next = dirlist = dirlist -> next;
	}

	TALLOC_FREE(frame);

	if (rem == count)
		return 0;
	else
		return count - rem;

}

/*
 * Routine to create a directory ...
 */

static int
smbc_mkdir_ctx(SMBCCTX *context,
               const char *fname,
               mode_t mode)
{
	SMBCSRV *srv = NULL;
	char *server = NULL;
        char *share = NULL;
        char *user = NULL;
        char *password = NULL;
        char *workgroup = NULL;
	char *path = NULL;
	char *targetpath = NULL;
	struct cli_state *targetcli = NULL;
	TALLOC_CTX *frame = talloc_stackframe();

	if (!context || !context->internal ||
	    !context->internal->_initialized) {
		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;
	}

	if (!fname) {
		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;
	}

	DEBUG(4, ("smbc_mkdir(%s)\n", fname));

	if (smbc_parse_path(frame,
				context,
				fname,
				&workgroup,
				&server,
				&share,
				&path,
				&user,
				&password,
				NULL)) {
                errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;
        }

	if (!user || user[0] == (char)0) {
		user = talloc_strdup(frame, context->user);
		if (!user) {
                	errno = ENOMEM;
			TALLOC_FREE(frame);
			return -1;
		}
	}

	srv = smbc_server(frame, context, True,
                          server, share, &workgroup, &user, &password);

	if (!srv) {

		TALLOC_FREE(frame);
		return -1;  /* errno set by smbc_server */

	}

	/*d_printf(">>>mkdir: resolving %s\n", path);*/
	if (!cli_resolve_path(frame, "", srv->cli, path,
				&targetcli, &targetpath)) {
		d_printf("Could not resolve %s\n", path);
		TALLOC_FREE(frame);
		return -1;
	}
	/*d_printf(">>>mkdir: resolved path as %s\n", targetpath);*/

	if (!cli_mkdir(targetcli, targetpath)) {

		errno = smbc_errno(context, targetcli);
		TALLOC_FREE(frame);
		return -1;

	} 

	TALLOC_FREE(frame);
	return 0;

}

/*
 * Our list function simply checks to see if a directory is not empty
 */

static int smbc_rmdir_dirempty = True;

static void
rmdir_list_fn(const char *mnt,
              file_info *finfo,
              const char *mask,
              void *state)
{
	if (strncmp(finfo->name, ".", 1) != 0 &&
            strncmp(finfo->name, "..", 2) != 0) {
		smbc_rmdir_dirempty = False;
        }
}

/*
 * Routine to remove a directory
 */

static int
smbc_rmdir_ctx(SMBCCTX *context,
               const char *fname)
{
	SMBCSRV *srv = NULL;
	char *server = NULL;
        char *share = NULL;
        char *user = NULL;
        char *password = NULL;
        char *workgroup = NULL;
	char *path = NULL;
        char *targetpath = NULL;
	struct cli_state *targetcli = NULL;
	TALLOC_CTX *frame = talloc_stackframe();

	if (!context || !context->internal ||
	    !context->internal->_initialized) {
		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;
	}

	if (!fname) {
		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;
	}

	DEBUG(4, ("smbc_rmdir(%s)\n", fname));

	if (smbc_parse_path(frame,
				context,
				fname,
				&workgroup,
				&server,
				&share,
				&path,
				&user,
				&password,
				NULL)) {
                errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;
        }

	if (!user || user[0] == (char)0) {
		user = talloc_strdup(frame, context->user);
		if (!user) {
                	errno = ENOMEM;
			TALLOC_FREE(frame);
			return -1;
		}
	}

	srv = smbc_server(frame, context, True,
                          server, share, &workgroup, &user, &password);

	if (!srv) {

		TALLOC_FREE(frame);
		return -1;  /* errno set by smbc_server */

	}

	/*d_printf(">>>rmdir: resolving %s\n", path);*/
	if (!cli_resolve_path(frame, "", srv->cli, path,
				&targetcli, &targetpath)) {
		d_printf("Could not resolve %s\n", path);
		TALLOC_FREE(frame);
		return -1;
	}
	/*d_printf(">>>rmdir: resolved path as %s\n", targetpath);*/


	if (!cli_rmdir(targetcli, targetpath)) {

		errno = smbc_errno(context, targetcli);

		if (errno == EACCES) {  /* Check if the dir empty or not */

                        /* Local storage to avoid buffer overflows */
			char *lpath;

			smbc_rmdir_dirempty = True;  /* Make this so ... */

			lpath = talloc_asprintf(frame, "%s\\*",
						targetpath);
			if (!lpath) {
				errno = ENOMEM;
				TALLOC_FREE(frame);
				return -1;
			}

			if (cli_list(targetcli, lpath,
                                     aDIR | aSYSTEM | aHIDDEN,
                                     rmdir_list_fn, NULL) < 0) {

				/* Fix errno to ignore latest error ... */
				DEBUG(5, ("smbc_rmdir: "
                                          "cli_list returned an error: %d\n",
					  smbc_errno(context, targetcli)));
				errno = EACCES;

			}

			if (smbc_rmdir_dirempty)
				errno = EACCES;
			else
				errno = ENOTEMPTY;

		}

		TALLOC_FREE(frame);
		return -1;

	} 

	TALLOC_FREE(frame);
	return 0;

}

/*
 * Routine to return the current directory position
 */

static off_t
smbc_telldir_ctx(SMBCCTX *context,
                 SMBCFILE *dir)
{
	TALLOC_CTX *frame = talloc_stackframe();

	if (!context || !context->internal ||
	    !context->internal->_initialized) {

		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;

	}

	if (!dir || !DLIST_CONTAINS(context->internal->_files, dir)) {

		errno = EBADF;
		TALLOC_FREE(frame);
		return -1;

	}

	if (dir->file != False) { /* FIXME, should be dir, perhaps */

		errno = ENOTDIR;
		TALLOC_FREE(frame);
		return -1;

	}

        /* See if we're already at the end. */
        if (dir->dir_next == NULL) {
                /* We are. */
		TALLOC_FREE(frame);
                return -1;
        }

	/*
	 * We return the pointer here as the offset
	 */
	TALLOC_FREE(frame);
        return (off_t)(long)dir->dir_next->dirent;
}

/*
 * A routine to run down the list and see if the entry is OK
 */

struct smbc_dir_list *
smbc_check_dir_ent(struct smbc_dir_list *list, 
                   struct smbc_dirent *dirent)
{

	/* Run down the list looking for what we want */

	if (dirent) {

		struct smbc_dir_list *tmp = list;

		while (tmp) {

			if (tmp->dirent == dirent)
				return tmp;

			tmp = tmp->next;

		}

	}

	return NULL;  /* Not found, or an error */

}


/*
 * Routine to seek on a directory
 */

static int
smbc_lseekdir_ctx(SMBCCTX *context,
                  SMBCFILE *dir,
                  off_t offset)
{
	long int l_offset = offset;  /* Handle problems of size */
	struct smbc_dirent *dirent = (struct smbc_dirent *)l_offset;
	struct smbc_dir_list *list_ent = (struct smbc_dir_list *)NULL;
	TALLOC_CTX *frame = talloc_stackframe();

	if (!context || !context->internal ||
	    !context->internal->_initialized) {

		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;

	}

	if (dir->file != False) { /* FIXME, should be dir, perhaps */

		errno = ENOTDIR;
		TALLOC_FREE(frame);
		return -1;

	}

	/* Now, check what we were passed and see if it is OK ... */

	if (dirent == NULL) {  /* Seek to the begining of the list */

		dir->dir_next = dir->dir_list;
		TALLOC_FREE(frame);
		return 0;

	}

        if (offset == -1) {     /* Seek to the end of the list */
                dir->dir_next = NULL;
		TALLOC_FREE(frame);
                return 0;
        }

	/* Now, run down the list and make sure that the entry is OK       */
	/* This may need to be changed if we change the format of the list */

	if ((list_ent = smbc_check_dir_ent(dir->dir_list, dirent)) == NULL) {
		errno = EINVAL;   /* Bad entry */
		TALLOC_FREE(frame);
		return -1;
	}

	dir->dir_next = list_ent;

	TALLOC_FREE(frame);
	return 0;
}

/*
 * Routine to fstat a dir
 */

static int
smbc_fstatdir_ctx(SMBCCTX *context,
                  SMBCFILE *dir,
                  struct stat *st)
{

	if (!context || !context->internal ||
	    !context->internal->_initialized) {
		errno = EINVAL;
		return -1;
	}

	/* No code yet ... */
	return 0;
}

static int
smbc_chmod_ctx(SMBCCTX *context,
               const char *fname,
               mode_t newmode)
{
        SMBCSRV *srv = NULL;
	char *server = NULL;
        char *share = NULL;
        char *user = NULL;
        char *password = NULL;
        char *workgroup = NULL;
	char *path = NULL;
	uint16 mode;
	TALLOC_CTX *frame = talloc_stackframe();

	if (!context || !context->internal ||
	    !context->internal->_initialized) {
		errno = EINVAL;  /* Best I can think of ... */
		TALLOC_FREE(frame);
		return -1;
	}

	if (!fname) {
		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;
	}

	DEBUG(4, ("smbc_chmod(%s, 0%3o)\n", fname, newmode));

	if (smbc_parse_path(frame,
				context,
				fname,
				&workgroup,
				&server,
				&share,
				&path,
				&user,
				&password,
				NULL)) {
                errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;
        }

	if (!user || user[0] == (char)0) {
		user = talloc_strdup(frame, context->user);
		if (!user) {
                	errno = ENOMEM;
			TALLOC_FREE(frame);
			return -1;
		}
	}

	srv = smbc_server(frame, context, True,
                          server, share, &workgroup, &user, &password);

	if (!srv) {
		TALLOC_FREE(frame);
		return -1;  /* errno set by smbc_server */
	}

	mode = 0;

	if (!(newmode & (S_IWUSR | S_IWGRP | S_IWOTH))) mode |= aRONLY;
	if ((newmode & S_IXUSR) && lp_map_archive(-1)) mode |= aARCH;
	if ((newmode & S_IXGRP) && lp_map_system(-1)) mode |= aSYSTEM;
	if ((newmode & S_IXOTH) && lp_map_hidden(-1)) mode |= aHIDDEN;

	if (!cli_setatr(srv->cli, path, mode, 0)) {
		errno = smbc_errno(context, srv->cli);
		TALLOC_FREE(frame);
		return -1;
	}

	TALLOC_FREE(frame);
        return 0;
}

static int
smbc_utimes_ctx(SMBCCTX *context,
                const char *fname,
                struct timeval *tbuf)
{
        SMBCSRV *srv = NULL;
	char *server = NULL;
	char *share = NULL;
	char *user = NULL;
	char *password = NULL;
	char *workgroup = NULL;
	char *path = NULL;
        time_t access_time;
        time_t write_time;
	TALLOC_CTX *frame = talloc_stackframe();

	if (!context || !context->internal ||
	    !context->internal->_initialized) {
		errno = EINVAL;  /* Best I can think of ... */
		TALLOC_FREE(frame);
		return -1;
	}

	if (!fname) {
		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;
	}

        if (tbuf == NULL) {
                access_time = write_time = time(NULL);
        } else {
                access_time = tbuf[0].tv_sec;
                write_time = tbuf[1].tv_sec;
        }

        if (DEBUGLVL(4)) {
                char *p;
                char atimebuf[32];
                char mtimebuf[32];

                strncpy(atimebuf, ctime(&access_time), sizeof(atimebuf) - 1);
                atimebuf[sizeof(atimebuf) - 1] = '\0';
                if ((p = strchr(atimebuf, '\n')) != NULL) {
                        *p = '\0';
                }

                strncpy(mtimebuf, ctime(&write_time), sizeof(mtimebuf) - 1);
                mtimebuf[sizeof(mtimebuf) - 1] = '\0';
                if ((p = strchr(mtimebuf, '\n')) != NULL) {
                        *p = '\0';
                }

                dbgtext("smbc_utimes(%s, atime = %s mtime = %s)\n",
                        fname, atimebuf, mtimebuf);
        }

	if (smbc_parse_path(frame,
				context,
				fname,
				&workgroup,
				&server,
				&share,
				&path,
				&user,
				&password,
				NULL)) {
		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;
        }

	if (!user || user[0] == (char)0) {
		user = talloc_strdup(frame, context->user);
		if (!user) {
			errno = ENOMEM;
			TALLOC_FREE(frame);
			return -1;
		}
	}

	srv = smbc_server(frame, context, True,
                          server, share, &workgroup, &user, &password);

	if (!srv) {
		TALLOC_FREE(frame);
		return -1;      /* errno set by smbc_server */
	}

        if (!smbc_setatr(context, srv, path,
                         0, access_time, write_time, 0, 0)) {
		TALLOC_FREE(frame);
                return -1;      /* errno set by smbc_setatr */
        }

	TALLOC_FREE(frame);
        return 0;
}


/*
 * Sort ACEs according to the documentation at
 * http://support.microsoft.com/kb/269175, at least as far as it defines the
 * order.
 */

static int
ace_compare(SEC_ACE *ace1,
            SEC_ACE *ace2)
{
        bool b1;
        bool b2;

        /* If the ACEs are equal, we have nothing more to do. */
        if (sec_ace_equal(ace1, ace2)) {
		return 0;
        }

        /* Inherited follow non-inherited */
        b1 = ((ace1->flags & SEC_ACE_FLAG_INHERITED_ACE) != 0);
        b2 = ((ace2->flags & SEC_ACE_FLAG_INHERITED_ACE) != 0);
        if (b1 != b2) {
                return (b1 ? 1 : -1);
        }

        /*
         * What shall we do with AUDITs and ALARMs?  It's undefined.  We'll
         * sort them after DENY and ALLOW.
         */
        b1 = (ace1->type != SEC_ACE_TYPE_ACCESS_ALLOWED &&
              ace1->type != SEC_ACE_TYPE_ACCESS_ALLOWED_OBJECT &&
              ace1->type != SEC_ACE_TYPE_ACCESS_DENIED &&
              ace1->type != SEC_ACE_TYPE_ACCESS_DENIED_OBJECT);
        b2 = (ace2->type != SEC_ACE_TYPE_ACCESS_ALLOWED &&
              ace2->type != SEC_ACE_TYPE_ACCESS_ALLOWED_OBJECT &&
              ace2->type != SEC_ACE_TYPE_ACCESS_DENIED &&
              ace2->type != SEC_ACE_TYPE_ACCESS_DENIED_OBJECT);
        if (b1 != b2) {
                return (b1 ? 1 : -1);
        }

        /* Allowed ACEs follow denied ACEs */
        b1 = (ace1->type == SEC_ACE_TYPE_ACCESS_ALLOWED ||
              ace1->type == SEC_ACE_TYPE_ACCESS_ALLOWED_OBJECT);
        b2 = (ace2->type == SEC_ACE_TYPE_ACCESS_ALLOWED ||
              ace2->type == SEC_ACE_TYPE_ACCESS_ALLOWED_OBJECT);
        if (b1 != b2) {
                return (b1 ? 1 : -1);
        }

        /*
         * ACEs applying to an entity's object follow those applying to the
         * entity itself
         */
        b1 = (ace1->type == SEC_ACE_TYPE_ACCESS_ALLOWED_OBJECT ||
              ace1->type == SEC_ACE_TYPE_ACCESS_DENIED_OBJECT);
        b2 = (ace2->type == SEC_ACE_TYPE_ACCESS_ALLOWED_OBJECT ||
              ace2->type == SEC_ACE_TYPE_ACCESS_DENIED_OBJECT);
        if (b1 != b2) {
                return (b1 ? 1 : -1);
        }

        /*
         * If we get this far, the ACEs are similar as far as the
         * characteristics we typically care about (those defined by the
         * referenced MS document).  We'll now sort by characteristics that
         * just seems reasonable.
         */

	if (ace1->type != ace2->type) {
		return ace2->type - ace1->type;
        }

	if (sid_compare(&ace1->trustee, &ace2->trustee)) {
		return sid_compare(&ace1->trustee, &ace2->trustee);
        }

	if (ace1->flags != ace2->flags) {
		return ace1->flags - ace2->flags;
        }

	if (ace1->access_mask != ace2->access_mask) {
		return ace1->access_mask - ace2->access_mask;
        }

	if (ace1->size != ace2->size) {
		return ace1->size - ace2->size;
        }

	return memcmp(ace1, ace2, sizeof(SEC_ACE));
}


static void
sort_acl(SEC_ACL *the_acl)
{
	uint32 i;
	if (!the_acl) return;

	qsort(the_acl->aces, the_acl->num_aces, sizeof(the_acl->aces[0]),
              QSORT_CAST ace_compare);

	for (i=1;i<the_acl->num_aces;) {
		if (sec_ace_equal(&the_acl->aces[i-1], &the_acl->aces[i])) {
			int j;
			for (j=i; j<the_acl->num_aces-1; j++) {
				the_acl->aces[j] = the_acl->aces[j+1];
			}
			the_acl->num_aces--;
		} else {
			i++;
		}
	}
}

/* convert a SID to a string, either numeric or username/group */
static void
convert_sid_to_string(struct cli_state *ipc_cli,
                      POLICY_HND *pol,
                      fstring str,
                      bool numeric,
                      DOM_SID *sid)
{
	char **domains = NULL;
	char **names = NULL;
	enum lsa_SidType *types = NULL;
	struct rpc_pipe_client *pipe_hnd = find_lsa_pipe_hnd(ipc_cli);
	TALLOC_CTX *ctx;

	sid_to_fstring(str, sid);

	if (numeric) {
		return;     /* no lookup desired */
	}

	if (!pipe_hnd) {
		return;
	}

	/* Ask LSA to convert the sid to a name */

	ctx = talloc_stackframe();

	if (!NT_STATUS_IS_OK(rpccli_lsa_lookup_sids(pipe_hnd, ctx,
						 pol, 1, sid, &domains,
						 &names, &types)) ||
	    !domains || !domains[0] || !names || !names[0]) {
		TALLOC_FREE(ctx);
		return;
	}

	TALLOC_FREE(ctx);
	/* Converted OK */

	slprintf(str, sizeof(fstring) - 1, "%s%s%s",
		 domains[0], lp_winbind_separator(),
		 names[0]);
}

/* convert a string to a SID, either numeric or username/group */
static bool
convert_string_to_sid(struct cli_state *ipc_cli,
                      POLICY_HND *pol,
                      bool numeric,
                      DOM_SID *sid,
                      const char *str)
{
	enum lsa_SidType *types = NULL;
	DOM_SID *sids = NULL;
	bool result = True;
	TALLOC_CTX *ctx = NULL;
	struct rpc_pipe_client *pipe_hnd = find_lsa_pipe_hnd(ipc_cli);

	if (!pipe_hnd) {
		return False;
	}

        if (numeric) {
                if (strncmp(str, "S-", 2) == 0) {
                        return string_to_sid(sid, str);
                }

                result = False;
                goto done;
        }

	ctx = talloc_stackframe();
	if (!NT_STATUS_IS_OK(rpccli_lsa_lookup_names(pipe_hnd, ctx,
					  pol, 1, &str, NULL, 1, &sids,
					  &types))) {
		result = False;
		goto done;
	}

	sid_copy(sid, &sids[0]);
 done:

	TALLOC_FREE(ctx);
	return result;
}


/* parse an ACE in the same format as print_ace() */
static bool
parse_ace(struct cli_state *ipc_cli,
          POLICY_HND *pol,
          SEC_ACE *ace,
          bool numeric,
          char *str)
{
	char *p;
	const char *cp;
	char *tok;
	unsigned int atype;
        unsigned int aflags;
        unsigned int amask;
	DOM_SID sid;
	SEC_ACCESS mask;
	const struct perm_value *v;
        struct perm_value {
                const char *perm;
                uint32 mask;
        };
	TALLOC_CTX *frame = talloc_stackframe();

        /* These values discovered by inspection */
        static const struct perm_value special_values[] = {
                { "R", 0x00120089 },
                { "W", 0x00120116 },
                { "X", 0x001200a0 },
                { "D", 0x00010000 },
                { "P", 0x00040000 },
                { "O", 0x00080000 },
                { NULL, 0 },
        };

        static const struct perm_value standard_values[] = {
                { "READ",   0x001200a9 },
                { "CHANGE", 0x001301bf },
                { "FULL",   0x001f01ff },
                { NULL, 0 },
        };


	ZERO_STRUCTP(ace);
	p = strchr_m(str,':');
	if (!p) {
		TALLOC_FREE(frame);
		return False;
	}
	*p = '\0';
	p++;
	/* Try to parse numeric form */

	if (sscanf(p, "%i/%i/%i", &atype, &aflags, &amask) == 3 &&
	    convert_string_to_sid(ipc_cli, pol, numeric, &sid, str)) {
		goto done;
	}

	/* Try to parse text form */

	if (!convert_string_to_sid(ipc_cli, pol, numeric, &sid, str)) {
		TALLOC_FREE(frame);
		return false;
	}

	cp = p;
	if (!next_token_talloc(frame, &cp, &tok, "/")) {
		TALLOC_FREE(frame);
		return false;
	}

	if (StrnCaseCmp(tok, "ALLOWED", strlen("ALLOWED")) == 0) {
		atype = SEC_ACE_TYPE_ACCESS_ALLOWED;
	} else if (StrnCaseCmp(tok, "DENIED", strlen("DENIED")) == 0) {
		atype = SEC_ACE_TYPE_ACCESS_DENIED;
	} else {
		TALLOC_FREE(frame);
		return false;
	}

	/* Only numeric form accepted for flags at present */

	if (!(next_token_talloc(frame, &cp, &tok, "/") &&
	      sscanf(tok, "%i", &aflags))) {
		TALLOC_FREE(frame);
		return false;
	}

	if (!next_token_talloc(frame, &cp, &tok, "/")) {
		TALLOC_FREE(frame);
		return false;
	}

	if (strncmp(tok, "0x", 2) == 0) {
		if (sscanf(tok, "%i", &amask) != 1) {
			TALLOC_FREE(frame);
			return false;
		}
		goto done;
	}

	for (v = standard_values; v->perm; v++) {
		if (strcmp(tok, v->perm) == 0) {
			amask = v->mask;
			goto done;
		}
	}

	p = tok;

	while(*p) {
		bool found = False;

		for (v = special_values; v->perm; v++) {
			if (v->perm[0] == *p) {
				amask |= v->mask;
				found = True;
			}
		}

		if (!found) {
			TALLOC_FREE(frame);
		 	return false;
		}
		p++;
	}

	if (*p) {
		TALLOC_FREE(frame);
		return false;
	}

 done:
	mask = amask;
	init_sec_ace(ace, &sid, atype, mask, aflags);
	TALLOC_FREE(frame);
	return true;
}

/* add an ACE to a list of ACEs in a SEC_ACL */
static bool
add_ace(SEC_ACL **the_acl,
        SEC_ACE *ace,
        TALLOC_CTX *ctx)
{
	SEC_ACL *newacl;
	SEC_ACE *aces;

	if (! *the_acl) {
		(*the_acl) = make_sec_acl(ctx, 3, 1, ace);
		return True;
	}

	if ((aces = SMB_CALLOC_ARRAY(SEC_ACE, 1+(*the_acl)->num_aces)) == NULL) {
		return False;
	}
	memcpy(aces, (*the_acl)->aces, (*the_acl)->num_aces * sizeof(SEC_ACE));
	memcpy(aces+(*the_acl)->num_aces, ace, sizeof(SEC_ACE));
	newacl = make_sec_acl(ctx, (*the_acl)->revision,
                              1+(*the_acl)->num_aces, aces);
	SAFE_FREE(aces);
	(*the_acl) = newacl;
	return True;
}


/* parse a ascii version of a security descriptor */
static SEC_DESC *
sec_desc_parse(TALLOC_CTX *ctx,
               struct cli_state *ipc_cli,
               POLICY_HND *pol,
               bool numeric,
               char *str)
{
	const char *p = str;
	char *tok;
	SEC_DESC *ret = NULL;
	size_t sd_size;
	DOM_SID *group_sid=NULL;
        DOM_SID *owner_sid=NULL;
	SEC_ACL *dacl=NULL;
	int revision=1;

	while (next_token_talloc(ctx, &p, &tok, "\t,\r\n")) {

		if (StrnCaseCmp(tok,"REVISION:", 9) == 0) {
			revision = strtol(tok+9, NULL, 16);
			continue;
		}

		if (StrnCaseCmp(tok,"OWNER:", 6) == 0) {
			if (owner_sid) {
				DEBUG(5, ("OWNER specified more than once!\n"));
				goto done;
			}
			owner_sid = SMB_CALLOC_ARRAY(DOM_SID, 1);
			if (!owner_sid ||
			    !convert_string_to_sid(ipc_cli, pol,
                                                   numeric,
                                                   owner_sid, tok+6)) {
				DEBUG(5, ("Failed to parse owner sid\n"));
				goto done;
			}
			continue;
		}

		if (StrnCaseCmp(tok,"OWNER+:", 7) == 0) {
			if (owner_sid) {
				DEBUG(5, ("OWNER specified more than once!\n"));
				goto done;
			}
			owner_sid = SMB_CALLOC_ARRAY(DOM_SID, 1);
			if (!owner_sid ||
			    !convert_string_to_sid(ipc_cli, pol,
                                                   False,
                                                   owner_sid, tok+7)) {
				DEBUG(5, ("Failed to parse owner sid\n"));
				goto done;
			}
			continue;
		}

		if (StrnCaseCmp(tok,"GROUP:", 6) == 0) {
			if (group_sid) {
				DEBUG(5, ("GROUP specified more than once!\n"));
				goto done;
			}
			group_sid = SMB_CALLOC_ARRAY(DOM_SID, 1);
			if (!group_sid ||
			    !convert_string_to_sid(ipc_cli, pol,
                                                   numeric,
                                                   group_sid, tok+6)) {
				DEBUG(5, ("Failed to parse group sid\n"));
				goto done;
			}
			continue;
		}

		if (StrnCaseCmp(tok,"GROUP+:", 7) == 0) {
			if (group_sid) {
				DEBUG(5, ("GROUP specified more than once!\n"));
				goto done;
			}
			group_sid = SMB_CALLOC_ARRAY(DOM_SID, 1);
			if (!group_sid ||
			    !convert_string_to_sid(ipc_cli, pol,
                                                   False,
                                                   group_sid, tok+6)) {
				DEBUG(5, ("Failed to parse group sid\n"));
				goto done;
			}
			continue;
		}

		if (StrnCaseCmp(tok,"ACL:", 4) == 0) {
			SEC_ACE ace;
			if (!parse_ace(ipc_cli, pol, &ace, numeric, tok+4)) {
				DEBUG(5, ("Failed to parse ACL %s\n", tok));
				goto done;
			}
			if(!add_ace(&dacl, &ace, ctx)) {
				DEBUG(5, ("Failed to add ACL %s\n", tok));
				goto done;
			}
			continue;
		}

		if (StrnCaseCmp(tok,"ACL+:", 5) == 0) {
			SEC_ACE ace;
			if (!parse_ace(ipc_cli, pol, &ace, False, tok+5)) {
				DEBUG(5, ("Failed to parse ACL %s\n", tok));
				goto done;
			}
			if(!add_ace(&dacl, &ace, ctx)) {
				DEBUG(5, ("Failed to add ACL %s\n", tok));
				goto done;
			}
			continue;
		}

		DEBUG(5, ("Failed to parse security descriptor\n"));
		goto done;
	}

	ret = make_sec_desc(ctx, revision, SEC_DESC_SELF_RELATIVE, 
			    owner_sid, group_sid, NULL, dacl, &sd_size);

  done:
	SAFE_FREE(group_sid);
	SAFE_FREE(owner_sid);

	return ret;
}


/* Obtain the current dos attributes */
static DOS_ATTR_DESC *
dos_attr_query(SMBCCTX *context,
               TALLOC_CTX *ctx,
               const char *filename,
               SMBCSRV *srv)
{
        struct timespec create_time_ts;
        struct timespec write_time_ts;
        struct timespec access_time_ts;
        struct timespec change_time_ts;
        SMB_OFF_T size = 0;
        uint16 mode = 0;
	SMB_INO_T inode = 0;
        DOS_ATTR_DESC *ret;

        ret = TALLOC_P(ctx, DOS_ATTR_DESC);
        if (!ret) {
                errno = ENOMEM;
                return NULL;
        }

        /* Obtain the DOS attributes */
        if (!smbc_getatr(context, srv, CONST_DISCARD(char *, filename),
                         &mode, &size,
                         &create_time_ts,
                         &access_time_ts,
                         &write_time_ts,
                         &change_time_ts,
                         &inode)) {
                errno = smbc_errno(context, srv->cli);
                DEBUG(5, ("dos_attr_query Failed to query old attributes\n"));
                return NULL;
        }

        ret->mode = mode;
        ret->size = size;
        ret->create_time = convert_timespec_to_time_t(create_time_ts);
        ret->access_time = convert_timespec_to_time_t(access_time_ts);
        ret->write_time = convert_timespec_to_time_t(write_time_ts);
        ret->change_time = convert_timespec_to_time_t(change_time_ts);
        ret->inode = inode;

        return ret;
}


/* parse a ascii version of a security descriptor */
static void
dos_attr_parse(SMBCCTX *context,
               DOS_ATTR_DESC *dad,
               SMBCSRV *srv,
               char *str)
{
        int n;
        const char *p = str;
	char *tok = NULL;
	TALLOC_CTX *frame = NULL;
        struct {
                const char * create_time_attr;
                const char * access_time_attr;
                const char * write_time_attr;
                const char * change_time_attr;
        } attr_strings;

        /* Determine whether to use old-style or new-style attribute names */
        if (context->internal->_full_time_names) {
                /* new-style names */
                attr_strings.create_time_attr = "CREATE_TIME";
                attr_strings.access_time_attr = "ACCESS_TIME";
                attr_strings.write_time_attr = "WRITE_TIME";
                attr_strings.change_time_attr = "CHANGE_TIME";
        } else {
                /* old-style names */
                attr_strings.create_time_attr = NULL;
                attr_strings.access_time_attr = "A_TIME";
                attr_strings.write_time_attr = "M_TIME";
                attr_strings.change_time_attr = "C_TIME";
        }

        /* if this is to set the entire ACL... */
        if (*str == '*') {
                /* ... then increment past the first colon if there is one */
                if ((p = strchr(str, ':')) != NULL) {
                        ++p;
                } else {
                        p = str;
                }
        }

	frame = talloc_stackframe();
	while (next_token_talloc(frame, &p, &tok, "\t,\r\n")) {
		if (StrnCaseCmp(tok, "MODE:", 5) == 0) {
                        long request = strtol(tok+5, NULL, 16);
                        if (request == 0) {
                                dad->mode = (request |
                                             (IS_DOS_DIR(dad->mode)
                                              ? FILE_ATTRIBUTE_DIRECTORY
                                              : FILE_ATTRIBUTE_NORMAL));
                        } else {
                                dad->mode = request;
                        }
			continue;
		}

		if (StrnCaseCmp(tok, "SIZE:", 5) == 0) {
                        dad->size = (SMB_OFF_T)atof(tok+5);
			continue;
		}

                n = strlen(attr_strings.access_time_attr);
                if (StrnCaseCmp(tok, attr_strings.access_time_attr, n) == 0) {
                        dad->access_time = (time_t)strtol(tok+n+1, NULL, 10);
			continue;
		}

                n = strlen(attr_strings.change_time_attr);
                if (StrnCaseCmp(tok, attr_strings.change_time_attr, n) == 0) {
                        dad->change_time = (time_t)strtol(tok+n+1, NULL, 10);
			continue;
		}

                n = strlen(attr_strings.write_time_attr);
                if (StrnCaseCmp(tok, attr_strings.write_time_attr, n) == 0) {
                        dad->write_time = (time_t)strtol(tok+n+1, NULL, 10);
			continue;
		}

		if (attr_strings.create_time_attr != NULL) {
			n = strlen(attr_strings.create_time_attr);
			if (StrnCaseCmp(tok, attr_strings.create_time_attr,
					n) == 0) {
				dad->create_time = (time_t)strtol(tok+n+1,
								  NULL, 10);
				continue;
			}
		}

		if (StrnCaseCmp(tok, "INODE:", 6) == 0) {
                        dad->inode = (SMB_INO_T)atof(tok+6);
			continue;
		}
	}
	TALLOC_FREE(frame);
}

/*****************************************************
 Retrieve the acls for a file.
*******************************************************/

static int
cacl_get(SMBCCTX *context,
         TALLOC_CTX *ctx,
         SMBCSRV *srv,
         struct cli_state *ipc_cli,
         POLICY_HND *pol,
         char *filename,
         char *attr_name,
         char *buf,
         int bufsize)
{
	uint32 i;
        int n = 0;
        int n_used;
        bool all;
        bool all_nt;
        bool all_nt_acls;
        bool all_dos;
        bool some_nt;
        bool some_dos;
        bool exclude_nt_revision = False;
        bool exclude_nt_owner = False;
        bool exclude_nt_group = False;
        bool exclude_nt_acl = False;
        bool exclude_dos_mode = False;
        bool exclude_dos_size = False;
        bool exclude_dos_create_time = False;
        bool exclude_dos_access_time = False;
        bool exclude_dos_write_time = False;
        bool exclude_dos_change_time = False;
        bool exclude_dos_inode = False;
        bool numeric = True;
        bool determine_size = (bufsize == 0);
	int fnum = -1;
	SEC_DESC *sd;
	fstring sidstr;
        fstring name_sandbox;
        char *name;
        char *pExclude;
        char *p;
        struct timespec create_time_ts;
	struct timespec write_time_ts;
        struct timespec access_time_ts;
        struct timespec change_time_ts;
	time_t create_time = (time_t)0;
	time_t write_time = (time_t)0;
        time_t access_time = (time_t)0;
        time_t change_time = (time_t)0;
	SMB_OFF_T size = 0;
	uint16 mode = 0;
	SMB_INO_T ino = 0;
        struct cli_state *cli = srv->cli;
        struct {
                const char * create_time_attr;
                const char * access_time_attr;
                const char * write_time_attr;
                const char * change_time_attr;
        } attr_strings;
        struct {
                const char * create_time_attr;
                const char * access_time_attr;
                const char * write_time_attr;
                const char * change_time_attr;
        } excl_attr_strings;

        /* Determine whether to use old-style or new-style attribute names */
        if (context->internal->_full_time_names) {
                /* new-style names */
                attr_strings.create_time_attr = "CREATE_TIME";
                attr_strings.access_time_attr = "ACCESS_TIME";
                attr_strings.write_time_attr = "WRITE_TIME";
                attr_strings.change_time_attr = "CHANGE_TIME";

                excl_attr_strings.create_time_attr = "CREATE_TIME";
                excl_attr_strings.access_time_attr = "ACCESS_TIME";
                excl_attr_strings.write_time_attr = "WRITE_TIME";
                excl_attr_strings.change_time_attr = "CHANGE_TIME";
        } else {
                /* old-style names */
                attr_strings.create_time_attr = NULL;
                attr_strings.access_time_attr = "A_TIME";
                attr_strings.write_time_attr = "M_TIME";
                attr_strings.change_time_attr = "C_TIME";

                excl_attr_strings.create_time_attr = NULL;
                excl_attr_strings.access_time_attr = "dos_attr.A_TIME";
                excl_attr_strings.write_time_attr = "dos_attr.M_TIME";
                excl_attr_strings.change_time_attr = "dos_attr.C_TIME";
        }

        /* Copy name so we can strip off exclusions (if any are specified) */
        strncpy(name_sandbox, attr_name, sizeof(name_sandbox) - 1);

        /* Ensure name is null terminated */
        name_sandbox[sizeof(name_sandbox) - 1] = '\0';

        /* Play in the sandbox */
        name = name_sandbox;

        /* If there are any exclusions, point to them and mask them from name */
        if ((pExclude = strchr(name, '!')) != NULL)
        {
                *pExclude++ = '\0';
        }

        all = (StrnCaseCmp(name, "system.*", 8) == 0);
        all_nt = (StrnCaseCmp(name, "system.nt_sec_desc.*", 20) == 0);
        all_nt_acls = (StrnCaseCmp(name, "system.nt_sec_desc.acl.*", 24) == 0);
        all_dos = (StrnCaseCmp(name, "system.dos_attr.*", 17) == 0);
        some_nt = (StrnCaseCmp(name, "system.nt_sec_desc.", 19) == 0);
        some_dos = (StrnCaseCmp(name, "system.dos_attr.", 16) == 0);
        numeric = (* (name + strlen(name) - 1) != '+');

        /* Look for exclusions from "all" requests */
        if (all || all_nt || all_dos) {

                /* Exclusions are delimited by '!' */
                for (;
                     pExclude != NULL;
                     pExclude = (p == NULL ? NULL : p + 1)) {

                /* Find end of this exclusion name */
                if ((p = strchr(pExclude, '!')) != NULL)
                {
                    *p = '\0';
                }

                /* Which exclusion name is this? */
                if (StrCaseCmp(pExclude, "nt_sec_desc.revision") == 0) {
                    exclude_nt_revision = True;
                }
                else if (StrCaseCmp(pExclude, "nt_sec_desc.owner") == 0) {
                    exclude_nt_owner = True;
                }
                else if (StrCaseCmp(pExclude, "nt_sec_desc.group") == 0) {
                    exclude_nt_group = True;
                }
                else if (StrCaseCmp(pExclude, "nt_sec_desc.acl") == 0) {
                    exclude_nt_acl = True;
                }
                else if (StrCaseCmp(pExclude, "dos_attr.mode") == 0) {
                    exclude_dos_mode = True;
                }
                else if (StrCaseCmp(pExclude, "dos_attr.size") == 0) {
                    exclude_dos_size = True;
                }
                else if (excl_attr_strings.create_time_attr != NULL &&
                         StrCaseCmp(pExclude,
                                    excl_attr_strings.change_time_attr) == 0) {
                    exclude_dos_create_time = True;
                }
                else if (StrCaseCmp(pExclude,
                                    excl_attr_strings.access_time_attr) == 0) {
                    exclude_dos_access_time = True;
                }
                else if (StrCaseCmp(pExclude,
                                    excl_attr_strings.write_time_attr) == 0) {
                    exclude_dos_write_time = True;
                }
                else if (StrCaseCmp(pExclude,
                                    excl_attr_strings.change_time_attr) == 0) {
                    exclude_dos_change_time = True;
                }
                else if (StrCaseCmp(pExclude, "dos_attr.inode") == 0) {
                    exclude_dos_inode = True;
                }
                else {
                    DEBUG(5, ("cacl_get received unknown exclusion: %s\n",
                              pExclude));
                    errno = ENOATTR;
                    return -1;
                }
            }
        }

        n_used = 0;

        /*
         * If we are (possibly) talking to an NT or new system and some NT
         * attributes have been requested...
         */
        if (ipc_cli && (all || some_nt || all_nt_acls)) {
                /* Point to the portion after "system.nt_sec_desc." */
                name += 19;     /* if (all) this will be invalid but unused */

                /* ... then obtain any NT attributes which were requested */
                fnum = cli_nt_create(cli, filename, CREATE_ACCESS_READ);

                if (fnum == -1) {
                        DEBUG(5, ("cacl_get failed to open %s: %s\n",
                                  filename, cli_errstr(cli)));
                        errno = 0;
                        return -1;
                }

                sd = cli_query_secdesc(cli, fnum, ctx);

                if (!sd) {
                        DEBUG(5,
                              ("cacl_get Failed to query old descriptor\n"));
                        errno = 0;
                        return -1;
                }

                cli_close(cli, fnum);

                if (! exclude_nt_revision) {
                        if (all || all_nt) {
                                if (determine_size) {
                                        p = talloc_asprintf(ctx,
                                                            "REVISION:%d",
                                                            sd->revision);
                                        if (!p) {
                                                errno = ENOMEM;
                                                return -1;
                                        }
                                        n = strlen(p);
                                } else {
                                        n = snprintf(buf, bufsize,
                                                     "REVISION:%d",
                                                     sd->revision);
                                }
                        } else if (StrCaseCmp(name, "revision") == 0) {
                                if (determine_size) {
                                        p = talloc_asprintf(ctx, "%d",
                                                            sd->revision);
                                        if (!p) {
                                                errno = ENOMEM;
                                                return -1;
                                        }
                                        n = strlen(p);
                                } else {
                                        n = snprintf(buf, bufsize, "%d",
                                                     sd->revision);
                                }
                        }

                        if (!determine_size && n > bufsize) {
                                errno = ERANGE;
                                return -1;
                        }
                        buf += n;
                        n_used += n;
                        bufsize -= n;
                        n = 0;
                }

                if (! exclude_nt_owner) {
                        /* Get owner and group sid */
                        if (sd->owner_sid) {
                                convert_sid_to_string(ipc_cli, pol,
                                                      sidstr,
                                                      numeric,
                                                      sd->owner_sid);
                        } else {
                                fstrcpy(sidstr, "");
                        }

                        if (all || all_nt) {
                                if (determine_size) {
                                        p = talloc_asprintf(ctx, ",OWNER:%s",
                                                            sidstr);
                                        if (!p) {
                                                errno = ENOMEM;
                                                return -1;
                                        }
                                        n = strlen(p);
                                } else if (sidstr[0] != '\0') {
                                        n = snprintf(buf, bufsize,
                                                     ",OWNER:%s", sidstr);
                                }
                        } else if (StrnCaseCmp(name, "owner", 5) == 0) {
                                if (determine_size) {
                                        p = talloc_asprintf(ctx, "%s", sidstr);
                                        if (!p) {
                                                errno = ENOMEM;
                                                return -1;
                                        }
                                        n = strlen(p);
                                } else {
                                        n = snprintf(buf, bufsize, "%s",
                                                     sidstr);
                                }
                        }

                        if (!determine_size && n > bufsize) {
                                errno = ERANGE;
                                return -1;
                        }
                        buf += n;
                        n_used += n;
                        bufsize -= n;
                        n = 0;
                }

                if (! exclude_nt_group) {
                        if (sd->group_sid) {
                                convert_sid_to_string(ipc_cli, pol,
                                                      sidstr, numeric,
                                                      sd->group_sid);
                        } else {
                                fstrcpy(sidstr, "");
                        }

                        if (all || all_nt) {
                                if (determine_size) {
                                        p = talloc_asprintf(ctx, ",GROUP:%s",
                                                            sidstr);
                                        if (!p) {
                                                errno = ENOMEM;
                                                return -1;
                                        }
                                        n = strlen(p);
                                } else if (sidstr[0] != '\0') {
                                        n = snprintf(buf, bufsize,
                                                     ",GROUP:%s", sidstr);
                                }
                        } else if (StrnCaseCmp(name, "group", 5) == 0) {
                                if (determine_size) {
                                        p = talloc_asprintf(ctx, "%s", sidstr);
                                        if (!p) {
                                                errno = ENOMEM;
                                                return -1;
                                        }
                                        n = strlen(p);
                                } else {
                                        n = snprintf(buf, bufsize,
                                                     "%s", sidstr);
                                }
                        }

                        if (!determine_size && n > bufsize) {
                                errno = ERANGE;
                                return -1;
                        }
                        buf += n;
                        n_used += n;
                        bufsize -= n;
                        n = 0;
                }

                if (! exclude_nt_acl) {
                        /* Add aces to value buffer  */
                        for (i = 0; sd->dacl && i < sd->dacl->num_aces; i++) {

                                SEC_ACE *ace = &sd->dacl->aces[i];
                                convert_sid_to_string(ipc_cli, pol,
                                                      sidstr, numeric,
                                                      &ace->trustee);

                                if (all || all_nt) {
                                        if (determine_size) {
                                                p = talloc_asprintf(
                                                        ctx, 
                                                        ",ACL:"
                                                        "%s:%d/%d/0x%08x", 
                                                        sidstr,
                                                        ace->type,
                                                        ace->flags,
                                                        ace->access_mask);
                                                if (!p) {
                                                        errno = ENOMEM;
                                                        return -1;
                                                }
                                                n = strlen(p);
                                        } else {
                                                n = snprintf(
                                                        buf, bufsize,
                                                        ",ACL:%s:%d/%d/0x%08x", 
                                                        sidstr,
                                                        ace->type,
                                                        ace->flags,
                                                        ace->access_mask);
                                        }
                                } else if ((StrnCaseCmp(name, "acl", 3) == 0 &&
                                            StrCaseCmp(name+3, sidstr) == 0) ||
                                           (StrnCaseCmp(name, "acl+", 4) == 0 &&
                                            StrCaseCmp(name+4, sidstr) == 0)) {
                                        if (determine_size) {
                                                p = talloc_asprintf(
                                                        ctx, 
                                                        "%d/%d/0x%08x", 
                                                        ace->type,
                                                        ace->flags,
                                                        ace->access_mask);
                                                if (!p) {
                                                        errno = ENOMEM;
                                                        return -1;
                                                }
                                                n = strlen(p);
                                        } else {
                                                n = snprintf(buf, bufsize,
                                                             "%d/%d/0x%08x", 
                                                             ace->type,
                                                             ace->flags,
                                                             ace->access_mask);
                                        }
                                } else if (all_nt_acls) {
                                        if (determine_size) {
                                                p = talloc_asprintf(
                                                        ctx, 
                                                        "%s%s:%d/%d/0x%08x",
                                                        i ? "," : "",
                                                        sidstr,
                                                        ace->type,
                                                        ace->flags,
                                                        ace->access_mask);
                                                if (!p) {
                                                        errno = ENOMEM;
                                                        return -1;
                                                }
                                                n = strlen(p);
                                        } else {
                                                n = snprintf(buf, bufsize,
                                                             "%s%s:%d/%d/0x%08x",
                                                             i ? "," : "",
                                                             sidstr,
                                                             ace->type,
                                                             ace->flags,
                                                             ace->access_mask);
                                        }
                                }
                                if (!determine_size && n > bufsize) {
                                        errno = ERANGE;
                                        return -1;
                                }
                                buf += n;
                                n_used += n;
                                bufsize -= n;
                                n = 0;
                        }
                }

                /* Restore name pointer to its original value */
                name -= 19;
        }

        if (all || some_dos) {
                /* Point to the portion after "system.dos_attr." */
                name += 16;     /* if (all) this will be invalid but unused */

                /* Obtain the DOS attributes */
                if (!smbc_getatr(context, srv, filename, &mode, &size, 
                                 &create_time_ts,
                                 &access_time_ts,
                                 &write_time_ts,
                                 &change_time_ts,
                                 &ino)) {

                        errno = smbc_errno(context, srv->cli);
                        return -1;

                }

                create_time = convert_timespec_to_time_t(create_time_ts);
                access_time = convert_timespec_to_time_t(access_time_ts);
                write_time = convert_timespec_to_time_t(write_time_ts);
                change_time = convert_timespec_to_time_t(change_time_ts);

                if (! exclude_dos_mode) {
                        if (all || all_dos) {
                                if (determine_size) {
                                        p = talloc_asprintf(ctx,
                                                            "%sMODE:0x%x",
                                                            (ipc_cli &&
                                                             (all || some_nt)
                                                             ? ","
                                                             : ""),
                                                            mode);
                                        if (!p) {
                                                errno = ENOMEM;
                                                return -1;
                                        }
                                        n = strlen(p);
                                } else {
                                        n = snprintf(buf, bufsize,
                                                     "%sMODE:0x%x",
                                                     (ipc_cli &&
                                                      (all || some_nt)
                                                      ? ","
                                                      : ""),
                                                     mode);
                                }
                        } else if (StrCaseCmp(name, "mode") == 0) {
                                if (determine_size) {
                                        p = talloc_asprintf(ctx, "0x%x", mode);
                                        if (!p) {
                                                errno = ENOMEM;
                                                return -1;
                                        }
                                        n = strlen(p);
                                } else {
                                        n = snprintf(buf, bufsize,
                                                     "0x%x", mode);
                                }
                        }

                        if (!determine_size && n > bufsize) {
                                errno = ERANGE;
                                return -1;
                        }
                        buf += n;
                        n_used += n;
                        bufsize -= n;
                        n = 0;
                }

                if (! exclude_dos_size) {
                        if (all || all_dos) {
                                if (determine_size) {
                                        p = talloc_asprintf(
                                                ctx,
                                                ",SIZE:%.0f",
                                                (double)size);
                                        if (!p) {
                                                errno = ENOMEM;
                                                return -1;
                                        }
                                        n = strlen(p);
                                } else {
                                        n = snprintf(buf, bufsize,
                                                     ",SIZE:%.0f",
                                                     (double)size);
                                }
                        } else if (StrCaseCmp(name, "size") == 0) {
                                if (determine_size) {
                                        p = talloc_asprintf(
                                                ctx,
                                                "%.0f",
                                                (double)size);
                                        if (!p) {
                                                errno = ENOMEM;
                                                return -1;
                                        }
                                        n = strlen(p);
                                } else {
                                        n = snprintf(buf, bufsize,
                                                     "%.0f",
                                                     (double)size);
                                }
                        }

                        if (!determine_size && n > bufsize) {
                                errno = ERANGE;
                                return -1;
                        }
                        buf += n;
                        n_used += n;
                        bufsize -= n;
                        n = 0;
                }

                if (! exclude_dos_create_time &&
                    attr_strings.create_time_attr != NULL) {
                        if (all || all_dos) {
                                if (determine_size) {
                                        p = talloc_asprintf(ctx,
                                                            ",%s:%lu",
                                                            attr_strings.create_time_attr,
                                                            create_time);
                                        if (!p) {
                                                errno = ENOMEM;
                                                return -1;
                                        }
                                        n = strlen(p);
                                } else {
                                        n = snprintf(buf, bufsize,
                                                     ",%s:%lu",
                                                     attr_strings.create_time_attr,
                                                     create_time);
                                }
                        } else if (StrCaseCmp(name, attr_strings.create_time_attr) == 0) {
                                if (determine_size) {
                                        p = talloc_asprintf(ctx, "%lu", create_time);
                                        if (!p) {
                                                errno = ENOMEM;
                                                return -1;
                                        }
                                        n = strlen(p);
                                } else {
                                        n = snprintf(buf, bufsize,
                                                     "%lu", create_time);
                                }
                        }

                        if (!determine_size && n > bufsize) {
                                errno = ERANGE;
                                return -1;
                        }
                        buf += n;
                        n_used += n;
                        bufsize -= n;
                        n = 0;
                }

                if (! exclude_dos_access_time) {
                        if (all || all_dos) {
                                if (determine_size) {
                                        p = talloc_asprintf(ctx,
                                                            ",%s:%lu",
                                                            attr_strings.access_time_attr,
                                                            access_time);
                                        if (!p) {
                                                errno = ENOMEM;
                                                return -1;
                                        }
                                        n = strlen(p);
                                } else {
                                        n = snprintf(buf, bufsize,
                                                     ",%s:%lu",
                                                     attr_strings.access_time_attr,
                                                     access_time);
                                }
                        } else if (StrCaseCmp(name, attr_strings.access_time_attr) == 0) {
                                if (determine_size) {
                                        p = talloc_asprintf(ctx, "%lu", access_time);
                                        if (!p) {
                                                errno = ENOMEM;
                                                return -1;
                                        }
                                        n = strlen(p);
                                } else {
                                        n = snprintf(buf, bufsize,
                                                     "%lu", access_time);
                                }
                        }

                        if (!determine_size && n > bufsize) {
                                errno = ERANGE;
                                return -1;
                        }
                        buf += n;
                        n_used += n;
                        bufsize -= n;
                        n = 0;
                }

                if (! exclude_dos_write_time) {
                        if (all || all_dos) {
                                if (determine_size) {
                                        p = talloc_asprintf(ctx,
                                                            ",%s:%lu",
                                                            attr_strings.write_time_attr,
                                                            write_time);
                                        if (!p) {
                                                errno = ENOMEM;
                                                return -1;
                                        }
                                        n = strlen(p);
                                } else {
                                        n = snprintf(buf, bufsize,
                                                     ",%s:%lu",
                                                     attr_strings.write_time_attr,
                                                     write_time);
                                }
                        } else if (StrCaseCmp(name, attr_strings.write_time_attr) == 0) {
                                if (determine_size) {
                                        p = talloc_asprintf(ctx, "%lu", write_time);
                                        if (!p) {
                                                errno = ENOMEM;
                                                return -1;
                                        }
                                        n = strlen(p);
                                } else {
                                        n = snprintf(buf, bufsize,
                                                     "%lu", write_time);
                                }
                        }

                        if (!determine_size && n > bufsize) {
                                errno = ERANGE;
                                return -1;
                        }
                        buf += n;
                        n_used += n;
                        bufsize -= n;
                        n = 0;
                }

                if (! exclude_dos_change_time) {
                        if (all || all_dos) {
                                if (determine_size) {
                                        p = talloc_asprintf(ctx,
                                                            ",%s:%lu",
                                                            attr_strings.change_time_attr,
                                                            change_time);
                                        if (!p) {
                                                errno = ENOMEM;
                                                return -1;
                                        }
                                        n = strlen(p);
                                } else {
                                        n = snprintf(buf, bufsize,
                                                     ",%s:%lu",
                                                     attr_strings.change_time_attr,
                                                     change_time);
                                }
                        } else if (StrCaseCmp(name, attr_strings.change_time_attr) == 0) {
                                if (determine_size) {
                                        p = talloc_asprintf(ctx, "%lu", change_time);
                                        if (!p) {
                                                errno = ENOMEM;
                                                return -1;
                                        }
                                        n = strlen(p);
                                } else {
                                        n = snprintf(buf, bufsize,
                                                     "%lu", change_time);
                                }
                        }

                        if (!determine_size && n > bufsize) {
                                errno = ERANGE;
                                return -1;
                        }
                        buf += n;
                        n_used += n;
                        bufsize -= n;
                        n = 0;
                }

                if (! exclude_dos_inode) {
                        if (all || all_dos) {
                                if (determine_size) {
                                        p = talloc_asprintf(
                                                ctx,
                                                ",INODE:%.0f",
                                                (double)ino);
                                        if (!p) {
                                                errno = ENOMEM;
                                                return -1;
                                        }
                                        n = strlen(p);
                                } else {
                                        n = snprintf(buf, bufsize,
                                                     ",INODE:%.0f",
                                                     (double) ino);
                                }
                        } else if (StrCaseCmp(name, "inode") == 0) {
                                if (determine_size) {
                                        p = talloc_asprintf(
                                                ctx,
                                                "%.0f",
                                                (double) ino);
                                        if (!p) {
                                                errno = ENOMEM;
                                                return -1;
                                        }
                                        n = strlen(p);
                                } else {
                                        n = snprintf(buf, bufsize,
                                                     "%.0f",
                                                     (double) ino);
                                }
                        }

                        if (!determine_size && n > bufsize) {
                                errno = ERANGE;
                                return -1;
                        }
                        buf += n;
                        n_used += n;
                        bufsize -= n;
                        n = 0;
                }

                /* Restore name pointer to its original value */
                name -= 16;
        }

        if (n_used == 0) {
                errno = ENOATTR;
                return -1;
        }

	return n_used;
}

/*****************************************************
set the ACLs on a file given an ascii description
*******************************************************/
static int
cacl_set(TALLOC_CTX *ctx,
         struct cli_state *cli,
         struct cli_state *ipc_cli,
         POLICY_HND *pol,
         const char *filename,
         const char *the_acl,
         int mode,
         int flags)
{
	int fnum;
        int err = 0;
	SEC_DESC *sd = NULL, *old;
        SEC_ACL *dacl = NULL;
	DOM_SID *owner_sid = NULL;
	DOM_SID *group_sid = NULL;
	uint32 i, j;
	size_t sd_size;
	int ret = 0;
        char *p;
        bool numeric = True;

        /* the_acl will be null for REMOVE_ALL operations */
        if (the_acl) {
                numeric = ((p = strchr(the_acl, ':')) != NULL &&
                           p > the_acl &&
                           p[-1] != '+');

                /* if this is to set the entire ACL... */
                if (*the_acl == '*') {
                        /* ... then increment past the first colon */
                        the_acl = p + 1;
                }

                sd = sec_desc_parse(ctx, ipc_cli, pol, numeric,
                                    CONST_DISCARD(char *, the_acl));

                if (!sd) {
			errno = EINVAL;
			return -1;
                }
        }

	/* SMBC_XATTR_MODE_REMOVE_ALL is the only caller
	   that doesn't deref sd */

	if (!sd && (mode != SMBC_XATTR_MODE_REMOVE_ALL)) {
		errno = EINVAL;
		return -1;
	}

	/* The desired access below is the only one I could find that works
	   with NT4, W2KP and Samba */

	fnum = cli_nt_create(cli, filename, CREATE_ACCESS_READ);

	if (fnum == -1) {
                DEBUG(5, ("cacl_set failed to open %s: %s\n",
                          filename, cli_errstr(cli)));
                errno = 0;
		return -1;
	}

	old = cli_query_secdesc(cli, fnum, ctx);

	if (!old) {
                DEBUG(5, ("cacl_set Failed to query old descriptor\n"));
                errno = 0;
		return -1;
	}

	cli_close(cli, fnum);

	switch (mode) {
	case SMBC_XATTR_MODE_REMOVE_ALL:
                old->dacl->num_aces = 0;
                dacl = old->dacl;
                break;

        case SMBC_XATTR_MODE_REMOVE:
		for (i=0;sd->dacl && i<sd->dacl->num_aces;i++) {
			bool found = False;

			for (j=0;old->dacl && j<old->dacl->num_aces;j++) {
                                if (sec_ace_equal(&sd->dacl->aces[i],
                                                  &old->dacl->aces[j])) {
					uint32 k;
					for (k=j; k<old->dacl->num_aces-1;k++) {
						old->dacl->aces[k] =
                                                        old->dacl->aces[k+1];
					}
					old->dacl->num_aces--;
					found = True;
                                        dacl = old->dacl;
					break;
				}
			}

			if (!found) {
                                err = ENOATTR;
                                ret = -1;
                                goto failed;
			}
		}
		break;

	case SMBC_XATTR_MODE_ADD:
		for (i=0;sd->dacl && i<sd->dacl->num_aces;i++) {
			bool found = False;

			for (j=0;old->dacl && j<old->dacl->num_aces;j++) {
				if (sid_equal(&sd->dacl->aces[i].trustee,
					      &old->dacl->aces[j].trustee)) {
                                        if (!(flags & SMBC_XATTR_FLAG_CREATE)) {
                                                err = EEXIST;
                                                ret = -1;
                                                goto failed;
                                        }
                                        old->dacl->aces[j] = sd->dacl->aces[i];
                                        ret = -1;
					found = True;
				}
			}

			if (!found && (flags & SMBC_XATTR_FLAG_REPLACE)) {
                                err = ENOATTR;
                                ret = -1;
                                goto failed;
			}

                        for (i=0;sd->dacl && i<sd->dacl->num_aces;i++) {
                                add_ace(&old->dacl, &sd->dacl->aces[i], ctx);
                        }
		}
                dacl = old->dacl;
		break;

	case SMBC_XATTR_MODE_SET:
 		old = sd;
                owner_sid = old->owner_sid;
                group_sid = old->group_sid;
                dacl = old->dacl;
		break;

        case SMBC_XATTR_MODE_CHOWN:
                owner_sid = sd->owner_sid;
                break;

        case SMBC_XATTR_MODE_CHGRP:
                group_sid = sd->group_sid;
                break;
	}

	/* Denied ACE entries must come before allowed ones */
	sort_acl(old->dacl);

	/* Create new security descriptor and set it */
	sd = make_sec_desc(ctx, old->revision, SEC_DESC_SELF_RELATIVE,
			   owner_sid, group_sid, NULL, dacl, &sd_size);

	fnum = cli_nt_create(cli, filename,
                             WRITE_DAC_ACCESS | WRITE_OWNER_ACCESS);

	if (fnum == -1) {
		DEBUG(5, ("cacl_set failed to open %s: %s\n",
                          filename, cli_errstr(cli)));
                errno = 0;
		return -1;
	}

	if (!cli_set_secdesc(cli, fnum, sd)) {
		DEBUG(5, ("ERROR: secdesc set failed: %s\n", cli_errstr(cli)));
		ret = -1;
	}

	/* Clean up */

 failed:
	cli_close(cli, fnum);

        if (err != 0) {
                errno = err;
        }

	return ret;
}


static int
smbc_setxattr_ctx(SMBCCTX *context,
                  const char *fname,
                  const char *name,
                  const void *value,
                  size_t size,
                  int flags)
{
        int ret;
        int ret2;
        SMBCSRV *srv = NULL;
        SMBCSRV *ipc_srv = NULL;
	char *server = NULL;
	char *share = NULL;
	char *user = NULL;
	char *password = NULL;
	char *workgroup = NULL;
	char *path = NULL;
        DOS_ATTR_DESC *dad = NULL;
        struct {
                const char * create_time_attr;
                const char * access_time_attr;
                const char * write_time_attr;
                const char * change_time_attr;
        } attr_strings;
        TALLOC_CTX *frame = talloc_stackframe();

	if (!context || !context->internal ||
	    !context->internal->_initialized) {
		errno = EINVAL;  /* Best I can think of ... */
		TALLOC_FREE(frame);
		return -1;
	}

	if (!fname) {
		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;
	}

	DEBUG(4, ("smbc_setxattr(%s, %s, %.*s)\n",
                  fname, name, (int) size, (const char*)value));

	if (smbc_parse_path(frame,
				context,
				fname,
				&workgroup,
				&server,
				&share,
				&path,
				&user,
				&password,
				NULL)) {
		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;
        }

	if (!user || user[0] == (char)0) {
		user = talloc_strdup(frame, context->user);
		if (!user) {
			errno = ENOMEM;
			TALLOC_FREE(frame);
			return -1;
		}
	}

	srv = smbc_server(frame, context, True,
                          server, share, &workgroup, &user, &password);
	if (!srv) {
		TALLOC_FREE(frame);
		return -1;  /* errno set by smbc_server */
	}

        if (! srv->no_nt_session) {
                ipc_srv = smbc_attr_server(frame, context, server, share,
                                           &workgroup, &user, &password);
                if (! ipc_srv) {
                        srv->no_nt_session = True;
                }
        } else {
                ipc_srv = NULL;
        }

        /*
         * Are they asking to set the entire set of known attributes?
         */
        if (StrCaseCmp(name, "system.*") == 0 ||
            StrCaseCmp(name, "system.*+") == 0) {
                /* Yup. */
                char *namevalue =
                        talloc_asprintf(talloc_tos(), "%s:%s",
                                        name+7, (const char *) value);
                if (! namevalue) {
                        errno = ENOMEM;
                        ret = -1;
			TALLOC_FREE(frame);
                        return -1;
                }

                if (ipc_srv) {
                        ret = cacl_set(talloc_tos(), srv->cli,
                                       ipc_srv->cli, &ipc_srv->pol, path,
                                       namevalue,
                                       (*namevalue == '*'
                                        ? SMBC_XATTR_MODE_SET
                                        : SMBC_XATTR_MODE_ADD),
                                       flags);
                } else {
                        ret = 0;
                }

                /* get a DOS Attribute Descriptor with current attributes */
                dad = dos_attr_query(context, talloc_tos(), path, srv);
                if (dad) {
                        /* Overwrite old with new, using what was provided */
                        dos_attr_parse(context, dad, srv, namevalue);

                        /* Set the new DOS attributes */
                        if (! smbc_setatr(context, srv, path,
                                          dad->create_time,
                                          dad->access_time,
                                          dad->write_time,
                                          dad->change_time,
                                          dad->mode)) {

                                /* cause failure if NT failed too */
                                dad = NULL; 
                        }
                }

                /* we only fail if both NT and DOS sets failed */
                if (ret < 0 && ! dad) {
                        ret = -1; /* in case dad was null */
                }
                else {
                        ret = 0;
                }

		TALLOC_FREE(frame);
                return ret;
        }

        /*
         * Are they asking to set an access control element or to set
         * the entire access control list?
         */
        if (StrCaseCmp(name, "system.nt_sec_desc.*") == 0 ||
            StrCaseCmp(name, "system.nt_sec_desc.*+") == 0 ||
            StrCaseCmp(name, "system.nt_sec_desc.revision") == 0 ||
            StrnCaseCmp(name, "system.nt_sec_desc.acl", 22) == 0 ||
            StrnCaseCmp(name, "system.nt_sec_desc.acl+", 23) == 0) {

                /* Yup. */
                char *namevalue =
                        talloc_asprintf(talloc_tos(), "%s:%s",
                                        name+19, (const char *) value);

                if (! ipc_srv) {
                        ret = -1; /* errno set by smbc_server() */
                }
                else if (! namevalue) {
                        errno = ENOMEM;
                        ret = -1;
                } else {
                        ret = cacl_set(talloc_tos(), srv->cli,
                                       ipc_srv->cli, &ipc_srv->pol, path,
                                       namevalue,
                                       (*namevalue == '*'
                                        ? SMBC_XATTR_MODE_SET
                                        : SMBC_XATTR_MODE_ADD),
                                       flags);
                }
		TALLOC_FREE(frame);
                return ret;
        }

        /*
         * Are they asking to set the owner?
         */
        if (StrCaseCmp(name, "system.nt_sec_desc.owner") == 0 ||
            StrCaseCmp(name, "system.nt_sec_desc.owner+") == 0) {

                /* Yup. */
                char *namevalue =
                        talloc_asprintf(talloc_tos(), "%s:%s",
                                        name+19, (const char *) value);

                if (! ipc_srv) {
                        ret = -1; /* errno set by smbc_server() */
                }
                else if (! namevalue) {
                        errno = ENOMEM;
                        ret = -1;
                } else {
                        ret = cacl_set(talloc_tos(), srv->cli,
                                       ipc_srv->cli, &ipc_srv->pol, path,
                                       namevalue, SMBC_XATTR_MODE_CHOWN, 0);
                }
		TALLOC_FREE(frame);
                return ret;
        }

        /*
         * Are they asking to set the group?
         */
        if (StrCaseCmp(name, "system.nt_sec_desc.group") == 0 ||
            StrCaseCmp(name, "system.nt_sec_desc.group+") == 0) {

                /* Yup. */
                char *namevalue =
                        talloc_asprintf(talloc_tos(), "%s:%s",
                                        name+19, (const char *) value);

                if (! ipc_srv) {
                        /* errno set by smbc_server() */
                        ret = -1;
                }
                else if (! namevalue) {
                        errno = ENOMEM;
                        ret = -1;
                } else {
                        ret = cacl_set(talloc_tos(), srv->cli,
                                       ipc_srv->cli, &ipc_srv->pol, path,
                                       namevalue, SMBC_XATTR_MODE_CHGRP, 0);
                }
		TALLOC_FREE(frame);
                return ret;
        }

        /* Determine whether to use old-style or new-style attribute names */
        if (context->internal->_full_time_names) {
                /* new-style names */
                attr_strings.create_time_attr = "system.dos_attr.CREATE_TIME";
                attr_strings.access_time_attr = "system.dos_attr.ACCESS_TIME";
                attr_strings.write_time_attr = "system.dos_attr.WRITE_TIME";
                attr_strings.change_time_attr = "system.dos_attr.CHANGE_TIME";
        } else {
                /* old-style names */
                attr_strings.create_time_attr = NULL;
                attr_strings.access_time_attr = "system.dos_attr.A_TIME";
                attr_strings.write_time_attr = "system.dos_attr.M_TIME";
                attr_strings.change_time_attr = "system.dos_attr.C_TIME";
        }

        /*
         * Are they asking to set a DOS attribute?
         */
        if (StrCaseCmp(name, "system.dos_attr.*") == 0 ||
            StrCaseCmp(name, "system.dos_attr.mode") == 0 ||
            (attr_strings.create_time_attr != NULL &&
             StrCaseCmp(name, attr_strings.create_time_attr) == 0) ||
            StrCaseCmp(name, attr_strings.access_time_attr) == 0 ||
            StrCaseCmp(name, attr_strings.write_time_attr) == 0 ||
            StrCaseCmp(name, attr_strings.change_time_attr) == 0) {

                /* get a DOS Attribute Descriptor with current attributes */
                dad = dos_attr_query(context, talloc_tos(), path, srv);
                if (dad) {
                        char *namevalue =
                                talloc_asprintf(talloc_tos(), "%s:%s",
                                                name+16, (const char *) value);
                        if (! namevalue) {
                                errno = ENOMEM;
                                ret = -1;
                        } else {
                                /* Overwrite old with provided new params */
                                dos_attr_parse(context, dad, srv, namevalue);

                                /* Set the new DOS attributes */
                                ret2 = smbc_setatr(context, srv, path,
                                                   dad->create_time,
                                                   dad->access_time,
                                                   dad->write_time,
                                                   dad->change_time,
                                                   dad->mode);

                                /* ret2 has True (success) / False (failure) */
                                if (ret2) {
                                        ret = 0;
                                } else {
                                        ret = -1;
                                }
                        }
                } else {
                        ret = -1;
                }

		TALLOC_FREE(frame);
                return ret;
        }

        /* Unsupported attribute name */
        errno = EINVAL;
	TALLOC_FREE(frame);
        return -1;
}

static int
smbc_getxattr_ctx(SMBCCTX *context,
                  const char *fname,
                  const char *name,
                  const void *value,
                  size_t size)
{
        int ret;
        SMBCSRV *srv = NULL;
        SMBCSRV *ipc_srv = NULL;
	char *server = NULL;
	char *share = NULL;
	char *user = NULL;
	char *password = NULL;
	char *workgroup = NULL;
	char *path = NULL;
        struct {
                const char * create_time_attr;
                const char * access_time_attr;
                const char * write_time_attr;
                const char * change_time_attr;
        } attr_strings;
	TALLOC_CTX *frame = talloc_stackframe();

        if (!context || !context->internal ||
            !context->internal->_initialized) {
                errno = EINVAL;  /* Best I can think of ... */
		TALLOC_FREE(frame);
                return -1;
        }

        if (!fname) {
                errno = EINVAL;
		TALLOC_FREE(frame);
                return -1;
        }

        DEBUG(4, ("smbc_getxattr(%s, %s)\n", fname, name));

        if (smbc_parse_path(frame,
				context,
				fname,
				&workgroup,
				&server,
				&share,
				&path,
				&user,
				&password,
				NULL)) {
		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;
        }

        if (!user || user[0] == (char)0) {
		user = talloc_strdup(frame, context->user);
		if (!user) {
			errno = ENOMEM;
			TALLOC_FREE(frame);
			return -1;
		}
	}

        srv = smbc_server(frame, context, True,
                          server, share, &workgroup, &user, &password);
        if (!srv) {
		TALLOC_FREE(frame);
                return -1;  /* errno set by smbc_server */
        }

        if (! srv->no_nt_session) {
                ipc_srv = smbc_attr_server(frame, context, server, share,
                                           &workgroup, &user, &password);
                if (! ipc_srv) {
                        srv->no_nt_session = True;
                }
        } else {
                ipc_srv = NULL;
        }

        /* Determine whether to use old-style or new-style attribute names */
        if (context->internal->_full_time_names) {
                /* new-style names */
                attr_strings.create_time_attr = "system.dos_attr.CREATE_TIME";
                attr_strings.access_time_attr = "system.dos_attr.ACCESS_TIME";
                attr_strings.write_time_attr = "system.dos_attr.WRITE_TIME";
                attr_strings.change_time_attr = "system.dos_attr.CHANGE_TIME";
        } else {
                /* old-style names */
                attr_strings.create_time_attr = NULL;
                attr_strings.access_time_attr = "system.dos_attr.A_TIME";
                attr_strings.write_time_attr = "system.dos_attr.M_TIME";
                attr_strings.change_time_attr = "system.dos_attr.C_TIME";
        }

        /* Are they requesting a supported attribute? */
        if (StrCaseCmp(name, "system.*") == 0 ||
            StrnCaseCmp(name, "system.*!", 9) == 0 ||
            StrCaseCmp(name, "system.*+") == 0 ||
            StrnCaseCmp(name, "system.*+!", 10) == 0 ||
            StrCaseCmp(name, "system.nt_sec_desc.*") == 0 ||
            StrnCaseCmp(name, "system.nt_sec_desc.*!", 21) == 0 ||
            StrCaseCmp(name, "system.nt_sec_desc.*+") == 0 ||
            StrnCaseCmp(name, "system.nt_sec_desc.*+!", 22) == 0 ||
            StrCaseCmp(name, "system.nt_sec_desc.revision") == 0 ||
            StrCaseCmp(name, "system.nt_sec_desc.owner") == 0 ||
            StrCaseCmp(name, "system.nt_sec_desc.owner+") == 0 ||
            StrCaseCmp(name, "system.nt_sec_desc.group") == 0 ||
            StrCaseCmp(name, "system.nt_sec_desc.group+") == 0 ||
            StrnCaseCmp(name, "system.nt_sec_desc.acl", 22) == 0 ||
            StrnCaseCmp(name, "system.nt_sec_desc.acl+", 23) == 0 ||
            StrCaseCmp(name, "system.dos_attr.*") == 0 ||
            StrnCaseCmp(name, "system.dos_attr.*!", 18) == 0 ||
            StrCaseCmp(name, "system.dos_attr.mode") == 0 ||
            StrCaseCmp(name, "system.dos_attr.size") == 0 ||
            (attr_strings.create_time_attr != NULL &&
             StrCaseCmp(name, attr_strings.create_time_attr) == 0) ||
            StrCaseCmp(name, attr_strings.access_time_attr) == 0 ||
            StrCaseCmp(name, attr_strings.write_time_attr) == 0 ||
            StrCaseCmp(name, attr_strings.change_time_attr) == 0 ||
            StrCaseCmp(name, "system.dos_attr.inode") == 0) {

                /* Yup. */
                ret = cacl_get(context, talloc_tos(), srv,
                               ipc_srv == NULL ? NULL : ipc_srv->cli, 
                               &ipc_srv->pol, path,
                               CONST_DISCARD(char *, name),
                               CONST_DISCARD(char *, value), size);
                if (ret < 0 && errno == 0) {
                        errno = smbc_errno(context, srv->cli);
                }
		TALLOC_FREE(frame);
                return ret;
        }

        /* Unsupported attribute name */
        errno = EINVAL;
	TALLOC_FREE(frame);
        return -1;
}


static int
smbc_removexattr_ctx(SMBCCTX *context,
                     const char *fname,
                     const char *name)
{
        int ret;
        SMBCSRV *srv = NULL;
        SMBCSRV *ipc_srv = NULL;
	char *server = NULL;
	char *share = NULL;
	char *user = NULL;
	char *password = NULL;
	char *workgroup = NULL;
	char *path = NULL;
	TALLOC_CTX *frame = talloc_stackframe();

        if (!context || !context->internal ||
            !context->internal->_initialized) {
                errno = EINVAL;  /* Best I can think of ... */
		TALLOC_FREE(frame);
                return -1;
        }

        if (!fname) {
                errno = EINVAL;
		TALLOC_FREE(frame);
                return -1;
        }

        DEBUG(4, ("smbc_removexattr(%s, %s)\n", fname, name));

	if (smbc_parse_path(frame,
				context,
				fname,
				&workgroup,
				&server,
				&share,
				&path,
				&user,
				&password,
				NULL)) {
		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;
        }

        if (!user || user[0] == (char)0) {
		user = talloc_strdup(frame, context->user);
		if (!user) {
			errno = ENOMEM;
			TALLOC_FREE(frame);
			return -1;
		}
	}

        srv = smbc_server(frame, context, True,
                          server, share, &workgroup, &user, &password);
        if (!srv) {
		TALLOC_FREE(frame);
                return -1;  /* errno set by smbc_server */
        }

        if (! srv->no_nt_session) {
                ipc_srv = smbc_attr_server(frame, context, server, share,
                                           &workgroup, &user, &password);
                if (! ipc_srv) {
                        srv->no_nt_session = True;
                }
        } else {
                ipc_srv = NULL;
        }

        if (! ipc_srv) {
		TALLOC_FREE(frame);
                return -1; /* errno set by smbc_attr_server */
        }

        /* Are they asking to set the entire ACL? */
        if (StrCaseCmp(name, "system.nt_sec_desc.*") == 0 ||
            StrCaseCmp(name, "system.nt_sec_desc.*+") == 0) {

                /* Yup. */
                ret = cacl_set(talloc_tos(), srv->cli,
                               ipc_srv->cli, &ipc_srv->pol, path,
                               NULL, SMBC_XATTR_MODE_REMOVE_ALL, 0);
		TALLOC_FREE(frame);
                return ret;
        }

        /*
         * Are they asking to remove one or more spceific security descriptor
         * attributes?
         */
        if (StrCaseCmp(name, "system.nt_sec_desc.revision") == 0 ||
            StrCaseCmp(name, "system.nt_sec_desc.owner") == 0 ||
            StrCaseCmp(name, "system.nt_sec_desc.owner+") == 0 ||
            StrCaseCmp(name, "system.nt_sec_desc.group") == 0 ||
            StrCaseCmp(name, "system.nt_sec_desc.group+") == 0 ||
            StrnCaseCmp(name, "system.nt_sec_desc.acl", 22) == 0 ||
            StrnCaseCmp(name, "system.nt_sec_desc.acl+", 23) == 0) {

                /* Yup. */
                ret = cacl_set(talloc_tos(), srv->cli,
                               ipc_srv->cli, &ipc_srv->pol, path,
                               name + 19, SMBC_XATTR_MODE_REMOVE, 0);
		TALLOC_FREE(frame);
                return ret;
        }

        /* Unsupported attribute name */
        errno = EINVAL;
	TALLOC_FREE(frame);
        return -1;
}

static int
smbc_listxattr_ctx(SMBCCTX *context,
                   const char *fname,
                   char *list,
                   size_t size)
{
        /*
         * This isn't quite what listxattr() is supposed to do.  This returns
         * the complete set of attribute names, always, rather than only those
         * attribute names which actually exist for a file.  Hmmm...
         */
        size_t retsize;
        const char supported_old[] =
                "system.*\0"
                "system.*+\0"
                "system.nt_sec_desc.revision\0"
                "system.nt_sec_desc.owner\0"
                "system.nt_sec_desc.owner+\0"
                "system.nt_sec_desc.group\0"
                "system.nt_sec_desc.group+\0"
                "system.nt_sec_desc.acl.*\0"
                "system.nt_sec_desc.acl\0"
                "system.nt_sec_desc.acl+\0"
                "system.nt_sec_desc.*\0"
                "system.nt_sec_desc.*+\0"
                "system.dos_attr.*\0"
                "system.dos_attr.mode\0"
                "system.dos_attr.c_time\0"
                "system.dos_attr.a_time\0"
                "system.dos_attr.m_time\0"
                ;
        const char supported_new[] =
                "system.*\0"
                "system.*+\0"
                "system.nt_sec_desc.revision\0"
                "system.nt_sec_desc.owner\0"
                "system.nt_sec_desc.owner+\0"
                "system.nt_sec_desc.group\0"
                "system.nt_sec_desc.group+\0"
                "system.nt_sec_desc.acl.*\0"
                "system.nt_sec_desc.acl\0"
                "system.nt_sec_desc.acl+\0"
                "system.nt_sec_desc.*\0"
                "system.nt_sec_desc.*+\0"
                "system.dos_attr.*\0"
                "system.dos_attr.mode\0"
                "system.dos_attr.create_time\0"
                "system.dos_attr.access_time\0"
                "system.dos_attr.write_time\0"
                "system.dos_attr.change_time\0"
                ;
        const char * supported;

        if (context->internal->_full_time_names) {
                supported = supported_new;
                retsize = sizeof(supported_new);
        } else {
                supported = supported_old;
                retsize = sizeof(supported_old);
        }

        if (size == 0) {
                return retsize;
        }

        if (retsize > size) {
                errno = ERANGE;
                return -1;
        }

        /* this can't be strcpy() because there are embedded null characters */
        memcpy(list, supported, retsize);
        return retsize;
}


/*
 * Open a print file to be written to by other calls
 */

static SMBCFILE *
smbc_open_print_job_ctx(SMBCCTX *context,
                        const char *fname)
{
	char *server = NULL;
	char *share = NULL;
	char *user = NULL;
	char *password = NULL;
	char *path = NULL;
	TALLOC_CTX *frame = talloc_stackframe();

        if (!context || !context->internal ||
            !context->internal->_initialized) {
                errno = EINVAL;
		TALLOC_FREE(frame);
                return NULL;
        }

        if (!fname) {
                errno = EINVAL;
		TALLOC_FREE(frame);
                return NULL;
        }

        DEBUG(4, ("smbc_open_print_job_ctx(%s)\n", fname));

        if (smbc_parse_path(frame,
				context,
				fname,
				NULL,
				&server,
				&share,
				&path,
				&user,
				&password,
				NULL)) {
                errno = EINVAL;
		TALLOC_FREE(frame);
                return NULL;
        }

        /* What if the path is empty, or the file exists? */

	TALLOC_FREE(frame);
        return (context->open)(context, fname, O_WRONLY, 666);
}

/*
 * Routine to print a file on a remote server ...
 *
 * We open the file, which we assume to be on a remote server, and then
 * copy it to a print file on the share specified by printq.
 */

static int
smbc_print_file_ctx(SMBCCTX *c_file,
                    const char *fname,
                    SMBCCTX *c_print,
                    const char *printq)
{
        SMBCFILE *fid1;
        SMBCFILE *fid2;
        int bytes;
        int saverr;
        int tot_bytes = 0;
        char buf[4096];
	TALLOC_CTX *frame = talloc_stackframe();

        if (!c_file || !c_file->internal->_initialized || !c_print ||
            !c_print->internal->_initialized) {

                errno = EINVAL;
		TALLOC_FREE(frame);
                return -1;

        }

        if (!fname && !printq) {

                errno = EINVAL;
		TALLOC_FREE(frame);
                return -1;

        }

        /* Try to open the file for reading ... */

        if ((long)(fid1 = (c_file->open)(c_file, fname, O_RDONLY, 0666)) < 0) {
                DEBUG(3, ("Error, fname=%s, errno=%i\n", fname, errno));
		TALLOC_FREE(frame);
                return -1;  /* smbc_open sets errno */
        }

        /* Now, try to open the printer file for writing */

        if ((long)(fid2 = (c_print->open_print_job)(c_print, printq)) < 0) {

                saverr = errno;  /* Save errno */
                (c_file->close_fn)(c_file, fid1);
                errno = saverr;
		TALLOC_FREE(frame);
                return -1;

        }

        while ((bytes = (c_file->read)(c_file, fid1, buf, sizeof(buf))) > 0) {

                tot_bytes += bytes;

                if (((c_print->write)(c_print, fid2, buf, bytes)) < 0) {

                        saverr = errno;
                        (c_file->close_fn)(c_file, fid1);
                        (c_print->close_fn)(c_print, fid2);
                        errno = saverr;

                }

        }

        saverr = errno;

        (c_file->close_fn)(c_file, fid1);  /* We have to close these anyway */
        (c_print->close_fn)(c_print, fid2);

        if (bytes < 0) {

                errno = saverr;
		TALLOC_FREE(frame);
                return -1;

        }

	TALLOC_FREE(frame);
        return tot_bytes;

}

/*
 * Routine to list print jobs on a printer share ...
 */

static int
smbc_list_print_jobs_ctx(SMBCCTX *context,
                         const char *fname,
                         smbc_list_print_job_fn fn)
{
	SMBCSRV *srv = NULL;
	char *server = NULL;
	char *share = NULL;
	char *user = NULL;
	char *password = NULL;
	char *workgroup = NULL;
	char *path = NULL;
	TALLOC_CTX *frame = talloc_stackframe();

        if (!context || !context->internal ||
            !context->internal->_initialized) {
                errno = EINVAL;
		TALLOC_FREE(frame);
                return -1;
        }

        if (!fname) {
                errno = EINVAL;
		TALLOC_FREE(frame);
                return -1;
        }

        DEBUG(4, ("smbc_list_print_jobs(%s)\n", fname));

        if (smbc_parse_path(frame,
				context,
				fname,
				&workgroup,
				&server,
				&share,
				&path,
				&user,
				&password,
				NULL)) {
		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;
        }

        if (!user || user[0] == (char)0) {
		user = talloc_strdup(frame, context->user);
		if (!user) {
			errno = ENOMEM;
			TALLOC_FREE(frame);
			return -1;
		}
	}

        srv = smbc_server(frame, context, True,
                          server, share, &workgroup, &user, &password);

        if (!srv) {
		TALLOC_FREE(frame);
                return -1;  /* errno set by smbc_server */
        }

        if (cli_print_queue(srv->cli,
                            (void (*)(struct print_job_info *))fn) < 0) {
                errno = smbc_errno(context, srv->cli);
		TALLOC_FREE(frame);
                return -1;
        }

	TALLOC_FREE(frame);
        return 0;

}

/*
 * Delete a print job from a remote printer share
 */

static int
smbc_unlink_print_job_ctx(SMBCCTX *context,
                          const char *fname,
                          int id)
{
	SMBCSRV *srv = NULL;
	char *server = NULL;
	char *share = NULL;
	char *user = NULL;
	char *password = NULL;
	char *workgroup = NULL;
	char *path = NULL;
        int err;
	TALLOC_CTX *frame = talloc_stackframe();

        if (!context || !context->internal ||
            !context->internal->_initialized) {
                errno = EINVAL;
		TALLOC_FREE(frame);
                return -1;
        }

        if (!fname) {
                errno = EINVAL;
		TALLOC_FREE(frame);
                return -1;
        }

        DEBUG(4, ("smbc_unlink_print_job(%s)\n", fname));

        if (smbc_parse_path(frame,
				context,
				fname,
				&workgroup,
				&server,
				&share,
				&path,
				&user,
				&password,
				NULL)) {
		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;
        }

        if (!user || user[0] == (char)0) {
		user = talloc_strdup(frame, context->user);
		if (!user) {
			errno = ENOMEM;
			TALLOC_FREE(frame);
			return -1;
		}
	}

        srv = smbc_server(frame, context, True,
                          server, share, &workgroup, &user, &password);

        if (!srv) {

		TALLOC_FREE(frame);
                return -1;  /* errno set by smbc_server */

        }

        if ((err = cli_printjob_del(srv->cli, id)) != 0) {

                if (err < 0)
                        errno = smbc_errno(context, srv->cli);
                else if (err == ERRnosuchprintjob)
                        errno = EINVAL;
		TALLOC_FREE(frame);
                return -1;

        }

	TALLOC_FREE(frame);
        return 0;

}

/*
 * Get a new empty handle to fill in with your own info
 */
SMBCCTX *
smbc_new_context(void)
{
        SMBCCTX *context;

        context = SMB_MALLOC_P(SMBCCTX);
        if (!context) {
                errno = ENOMEM;
                return NULL;
        }

        ZERO_STRUCTP(context);

        context->internal = SMB_MALLOC_P(struct smbc_internal_data);
        if (!context->internal) {
		SAFE_FREE(context);
                errno = ENOMEM;
                return NULL;
        }

        ZERO_STRUCTP(context->internal);

        /* ADD REASONABLE DEFAULTS */
        context->debug            = 0;
        context->timeout          = 20000; /* 20 seconds */

	context->options.browse_max_lmb_count      = 3;    /* # LMBs to query */
	context->options.urlencode_readdir_entries = False;/* backward compat */
	context->options.one_share_per_server      = False;/* backward compat */
        context->internal->_share_mode             = SMBC_SHAREMODE_DENY_NONE;
                                /* backward compat */

        context->open                              = smbc_open_ctx;
        context->creat                             = smbc_creat_ctx;
        context->read                              = smbc_read_ctx;
        context->write                             = smbc_write_ctx;
        context->close_fn                          = smbc_close_ctx;
        context->unlink                            = smbc_unlink_ctx;
        context->rename                            = smbc_rename_ctx;
        context->lseek                             = smbc_lseek_ctx;
        context->stat                              = smbc_stat_ctx;
        context->fstat                             = smbc_fstat_ctx;
        context->opendir                           = smbc_opendir_ctx;
        context->closedir                          = smbc_closedir_ctx;
        context->readdir                           = smbc_readdir_ctx;
        context->getdents                          = smbc_getdents_ctx;
        context->mkdir                             = smbc_mkdir_ctx;
        context->rmdir                             = smbc_rmdir_ctx;
        context->telldir                           = smbc_telldir_ctx;
        context->lseekdir                          = smbc_lseekdir_ctx;
        context->fstatdir                          = smbc_fstatdir_ctx;
        context->chmod                             = smbc_chmod_ctx;
        context->utimes                            = smbc_utimes_ctx;
        context->setxattr                          = smbc_setxattr_ctx;
        context->getxattr                          = smbc_getxattr_ctx;
        context->removexattr                       = smbc_removexattr_ctx;
        context->listxattr                         = smbc_listxattr_ctx;
        context->open_print_job                    = smbc_open_print_job_ctx;
        context->print_file                        = smbc_print_file_ctx;
        context->list_print_jobs                   = smbc_list_print_jobs_ctx;
        context->unlink_print_job                  = smbc_unlink_print_job_ctx;

        context->callbacks.check_server_fn         = smbc_check_server;
        context->callbacks.remove_unused_server_fn = smbc_remove_unused_server;

        smbc_default_cache_functions(context);

        return context;
}

/*
 * Free a context
 *
 * Returns 0 on success. Otherwise returns 1, the SMBCCTX is _not_ freed
 * and thus you'll be leaking memory if not handled properly.
 *
 */
int
smbc_free_context(SMBCCTX *context,
                  int shutdown_ctx)
{
        if (!context) {
                errno = EBADF;
                return 1;
        }

        if (shutdown_ctx) {
                SMBCFILE * f;
                DEBUG(1,("Performing aggressive shutdown.\n"));

                f = context->internal->_files;
                while (f) {
                        (context->close_fn)(context, f);
                        f = f->next;
                }
                context->internal->_files = NULL;

                /* First try to remove the servers the nice way. */
                if (context->callbacks.purge_cached_fn(context)) {
                        SMBCSRV * s;
                        SMBCSRV * next;
                        DEBUG(1, ("Could not purge all servers, "
                                  "Nice way shutdown failed.\n"));
                        s = context->internal->_servers;
                        while (s) {
                                DEBUG(1, ("Forced shutdown: %p (fd=%d)\n",
                                          s, s->cli->fd));
                                cli_shutdown(s->cli);
                                (context->callbacks.remove_cached_srv_fn)(context,
                                                                          s);
                                next = s->next;
                                DLIST_REMOVE(context->internal->_servers, s);
                                SAFE_FREE(s);
                                s = next;
                        }
                        context->internal->_servers = NULL;
                }
        }
        else {
                /* This is the polite way */
                if ((context->callbacks.purge_cached_fn)(context)) {
                        DEBUG(1, ("Could not purge all servers, "
                                  "free_context failed.\n"));
                        errno = EBUSY;
                        return 1;
                }
                if (context->internal->_servers) {
                        DEBUG(1, ("Active servers in context, "
                                  "free_context failed.\n"));
                        errno = EBUSY;
                        return 1;
                }
                if (context->internal->_files) {
                        DEBUG(1, ("Active files in context, "
                                  "free_context failed.\n"));
                        errno = EBUSY;
                        return 1;
                }
        }

        /* Things we have to clean up */
        SAFE_FREE(context->workgroup);
        SAFE_FREE(context->netbios_name);
        SAFE_FREE(context->user);

        DEBUG(3, ("Context %p succesfully freed\n", context));
        SAFE_FREE(context->internal);
        SAFE_FREE(context);
        return 0;
}


/*
 * Each time the context structure is changed, we have binary backward
 * compatibility issues.  Instead of modifying the public portions of the
 * context structure to add new options, instead, we put them in the internal
 * portion of the context structure and provide a set function for these new
 * options.
 */
void
smbc_option_set(SMBCCTX *context,
                char *option_name,
                ... /* option_value */)
{
        va_list ap;
        union {
                int i;
                bool b;
                smbc_get_auth_data_with_context_fn auth_fn;
                smbc_get_auth_data_with_ccname_fn auth_ccname_fn;
                void *v;
		const char *s;
        } option_value;

        va_start(ap, option_name);

        if (strcmp(option_name, "debug_to_stderr") == 0) {
                /*
                 * Log to standard error instead of standard output.
                 */
                option_value.b = (bool) va_arg(ap, int);
                context->internal->_debug_stderr = option_value.b;

        } else if (strcmp(option_name, "full_time_names") == 0) {
                /*
                 * Use new-style time attribute names, e.g. WRITE_TIME rather
                 * than the old-style names such as M_TIME.  This allows also
                 * setting/getting CREATE_TIME which was previously
                 * unimplemented.  (Note that the old C_TIME was supposed to
                 * be CHANGE_TIME but was confused and sometimes referred to
                 * CREATE_TIME.)
                 */
                option_value.b = (bool) va_arg(ap, int);
                context->internal->_full_time_names = option_value.b;

        } else if (strcmp(option_name, "open_share_mode") == 0) {
                /*
                 * The share mode to use for files opened with
                 * smbc_open_ctx().  The default is SMBC_SHAREMODE_DENY_NONE.
                 */
                option_value.i = va_arg(ap, int);
                context->internal->_share_mode =
                        (smbc_share_mode) option_value.i;

        } else if (strcmp(option_name, "auth_function") == 0) {
                /*
                 * Use the new-style authentication function which includes
                 * the context.
                 */
                option_value.auth_fn =
                        va_arg(ap, smbc_get_auth_data_with_context_fn);
                context->internal->_auth_fn_with_context =
                        option_value.auth_fn;
        } else if (strcmp(option_name, "auth_ccname_function") == 0) {
                /*
                 * Use the newer-style authentication function which includes
                 * the context and ccname.
                 */
                option_value.auth_ccname_fn =
                        va_arg(ap, smbc_get_auth_data_with_ccname_fn);
                context->internal->_auth_fn_with_ccname =
                        option_value.auth_ccname_fn;
        } else if (strcmp(option_name, "user_data") == 0) {
                /*
                 * Save a user data handle which may be retrieved by the user
                 * with smbc_option_get()
                 */
                option_value.v = va_arg(ap, void *);
                context->internal->_user_data = option_value.v;
        } else if (strcmp(option_name, "smb_encrypt_level") == 0) {
                /*
                 * Save an encoded value for encryption level.
                 * 0 = off, 1 = attempt, 2 = required.
                 */
                option_value.s = va_arg(ap, const char *);
		if (strcmp(option_value.s, "none") == 0) {
			context->internal->_smb_encryption_level = 0;
		} else if (strcmp(option_value.s, "request") == 0) {
			context->internal->_smb_encryption_level = 1;
		} else if (strcmp(option_value.s, "require") == 0) {
			context->internal->_smb_encryption_level = 2;
		}
        }

        va_end(ap);
}


/*
 * Retrieve the current value of an option
 */
void *
smbc_option_get(SMBCCTX *context,
                char *option_name)
{
        if (strcmp(option_name, "debug_stderr") == 0) {
                /*
                 * Log to standard error instead of standard output.
                 */
#if defined(__intptr_t_defined) || defined(HAVE_INTPTR_T)
		return (void *) (intptr_t) context->internal->_debug_stderr;
#else
		return (void *) context->internal->_debug_stderr;
#endif
        } else if (strcmp(option_name, "full_time_names") == 0) {
                /*
                 * Use new-style time attribute names, e.g. WRITE_TIME rather
                 * than the old-style names such as M_TIME.  This allows also
                 * setting/getting CREATE_TIME which was previously
                 * unimplemented.  (Note that the old C_TIME was supposed to
                 * be CHANGE_TIME but was confused and sometimes referred to
                 * CREATE_TIME.)
                 */
#if defined(__intptr_t_defined) || defined(HAVE_INTPTR_T)
		return (void *) (intptr_t) context->internal->_full_time_names;
#else
		return (void *) context->internal->_full_time_names;
#endif

        } else if (strcmp(option_name, "auth_function") == 0) {
                /*
                 * Use the new-style authentication function which includes
                 * the context.
                 */
                return (void *) context->internal->_auth_fn_with_context;
        } else if (strcmp(option_name, "auth_ccname_function") == 0) {
                /*
                 * Use the newer-style authentication function which includes
                 * the context and ccname.
                 */
                return (void *) context->internal->_auth_fn_with_ccname;
        } else if (strcmp(option_name, "user_data") == 0) {
                /*
                 * Save a user data handle which may be retrieved by the user
                 * with smbc_option_get()
                 */
                return context->internal->_user_data;
        } else if (strcmp(option_name, "smb_encrypt_level") == 0) {
		/*
		 * Return the current smb encrypt negotiate option as a string.
		 */
		switch (context->internal->_smb_encryption_level) {
		case 0:
			return (void *) "none";
		case 1:
			return (void *) "request";
		case 2:
			return (void *) "require";
		}
        } else if (strcmp(option_name, "smb_encrypt_on") == 0) {
		/*
		 * Return the current smb encrypt status option as a bool.
		 * false = off, true = on. We don't know what server is
		 * being requested, so we only return true if all servers
		 * are using an encrypted connection.
		 */
		SMBCSRV *s;
		unsigned int num_servers = 0;

		for (s = context->internal->_servers; s; s = s->next) {
			num_servers++;
			if (s->cli->trans_enc_state == NULL) {
				return (void *)false;
			}
		}
		return (void *) (bool) (num_servers > 0);
        }

        return NULL;
}


/*
 * Initialise the library etc
 *
 * We accept a struct containing handle information.
 * valid values for info->debug from 0 to 100,
 * and insist that info->fn must be non-null.
 */
SMBCCTX *
smbc_init_context(SMBCCTX *context)
{
        int pid;
        char *user = NULL;
        char *home = NULL;

        if (!context || !context->internal) {
                errno = EBADF;
                return NULL;
        }

        /* Do not initialise the same client twice */
        if (context->internal->_initialized) {
                return 0;
        }

        if ((!context->callbacks.auth_fn &&
             !context->internal->_auth_fn_with_context &&
	     !context->internal->_auth_fn_with_ccname) ||
            context->debug < 0 ||
            context->debug > 100) {

                errno = EINVAL;
                return NULL;

        }

        if (!smbc_initialized) {
                /*
                 * Do some library-wide intializations the first time we get
                 * called
                 */
		bool conf_loaded = False;
		TALLOC_CTX *frame = talloc_stackframe();

                /* Set this to what the user wants */
                DEBUGLEVEL = context->debug;

                load_case_tables();

                setup_logging("libsmbclient", True);
                if (context->internal->_debug_stderr) {
                        dbf = x_stderr;
                        x_setbuf(x_stderr, NULL);
                }

                /* Here we would open the smb.conf file if needed ... */

                in_client = True; /* FIXME, make a param */

                home = getenv("HOME");
		if (home) {
       			char *conf = NULL;
			if (asprintf(&conf, "%s/.smb/smb.conf", home) > 0) {
				if (lp_load(conf, True, False, False, True)) {
					conf_loaded = True;
				} else {
					DEBUG(5, ("Could not load config file: %s\n",
						conf));
				}
				SAFE_FREE(conf);
			}
               	}

		if (!conf_loaded) {
                        /*
                         * Well, if that failed, try the get_dyn_CONFIGFILE
                         * Which points to the standard locn, and if that
                         * fails, silently ignore it and use the internal
                         * defaults ...
                         */

                        if (!lp_load(get_dyn_CONFIGFILE(), True, False, False, False)) {
                                DEBUG(5, ("Could not load config file: %s\n",
                                          get_dyn_CONFIGFILE()));
                        } else if (home) {
        			char *conf;
                                /*
                                 * We loaded the global config file.  Now lets
                                 * load user-specific modifications to the
                                 * global config.
                                 */
				if (asprintf(&conf,
						"%s/.smb/smb.conf.append",
						home) > 0) {
	                                if (!lp_load(conf, True, False, False, False)) {
						DEBUG(10,
						("Could not append config file: "
						"%s\n",
						conf));
                                	}
					SAFE_FREE(conf);
				}
                        }
                }

                load_interfaces();  /* Load the list of interfaces ... */

                reopen_logs();  /* Get logging working ... */

                /*
                 * Block SIGPIPE (from lib/util_sock.c: write())
                 * It is not needed and should not stop execution
                 */
                BlockSignals(True, SIGPIPE);

                /* Done with one-time initialisation */
                smbc_initialized = 1;

		TALLOC_FREE(frame);
        }

        if (!context->user) {
                /*
                 * FIXME: Is this the best way to get the user info?
                 */
                user = getenv("USER");
                /* walk around as "guest" if no username can be found */
                if (!user) context->user = SMB_STRDUP("guest");
                else context->user = SMB_STRDUP(user);
        }

        if (!context->netbios_name) {
                /*
                 * We try to get our netbios name from the config. If that
                 * fails we fall back on constructing our netbios name from
                 * our hostname etc
                 */
                if (global_myname()) {
                        context->netbios_name = SMB_STRDUP(global_myname());
                }
                else {
                        /*
                         * Hmmm, I want to get hostname as well, but I am too
                         * lazy for the moment
                         */
                        pid = sys_getpid();
                        context->netbios_name = (char *)SMB_MALLOC(17);
                        if (!context->netbios_name) {
                                errno = ENOMEM;
                                return NULL;
                        }
                        slprintf(context->netbios_name, 16,
                                 "smbc%s%d", context->user, pid);
                }
        }

        DEBUG(1, ("Using netbios name %s.\n", context->netbios_name));

        if (!context->workgroup) {
                if (lp_workgroup()) {
                        context->workgroup = SMB_STRDUP(lp_workgroup());
                }
                else {
                        /* TODO: Think about a decent default workgroup */
                        context->workgroup = SMB_STRDUP("samba");
                }
        }

        DEBUG(1, ("Using workgroup %s.\n", context->workgroup));

        /* shortest timeout is 1 second */
        if (context->timeout > 0 && context->timeout < 1000)
                context->timeout = 1000;

        /*
         * FIXME: Should we check the function pointers here?
         */

        context->internal->_initialized = True;

        return context;
}


/* Return the verion of samba, and thus libsmbclient */
const char *
smbc_version(void)
{
        return likewise_version_string();
}


/*
 * Routine to create a file using the full
 * NT API
 */

SMBCFILE *
smbc_nt_create(SMBCCTX *context,
	       const char *fname,
	       uint32 CreatFlags, uint32 DesiredAccess,
	       uint32 FileAttributes, uint32 ShareAccess,
	       uint32 CreateDisposition, uint32 CreateOptions,
	       uint8 SecurityFlags)
{
	int fd;
	SMBCSRV *srv   = NULL;
	SMBCFILE *file = NULL;
	char *server = NULL;
	char *share = NULL;
	char *user = NULL;
	char *password = NULL;
	char *workgroup = NULL;
	char *path = NULL;
	char *targetpath = NULL;
	struct cli_state *targetcli = NULL;
	TALLOC_CTX *frame = talloc_stackframe();

	if (!context || !context->internal ||
	    !context->internal->_initialized) {

		errno = EINVAL;  /* Best I can think of ... */
		return NULL;

	}

	if (!fname) {

		errno = EINVAL;
		TALLOC_FREE(frame);
		return NULL;

	}

	if (smbc_parse_path(frame, context, fname,
                            &workgroup, &server,
                            &share, &path,
                            &user, &password,
                            NULL)) {
                errno = EINVAL;
                return NULL;
        }

	if (!user || user[0] == (char)0) {
		user = talloc_strdup(frame, context->user);
		if (!user) {
                	errno = ENOMEM;
			TALLOC_FREE(frame);
			return NULL;
		}
	}

	srv = smbc_server(frame, context, True,
                          server, share, &workgroup, &user, &password);

	if (!srv) {

		if (errno == EPERM) errno = EACCES;
		TALLOC_FREE(frame);
		return NULL;  /* smbc_server sets errno */
    
	}

	/* Hmmm, the test for a directory is suspect here ... FIXME */

	if (strlen(path) > 0 && path[strlen(path) - 1] == '\\') {
    
		fd = -1;

	}
	else {
	  
		file = SMB_MALLOC_P(SMBCFILE);

		if (!file) {

			errno = ENOMEM;
			TALLOC_FREE(frame);
			return NULL;

		}

		ZERO_STRUCTP(file);

		/*d_printf(">>>open: resolving %s\n", path);*/
		if (!cli_resolve_path(frame, "", srv->cli, path, &targetcli, &targetpath))
		{
			d_printf("Could not resolve %s\n", path);
			SAFE_FREE(file);
			TALLOC_FREE(frame);
			return NULL;
		}
		/*d_printf(">>>open: resolved %s as %s\n", path, targetpath);*/

		if ((fd = 
		     cli_nt_create_full(targetcli, targetpath,
					CreatFlags, DesiredAccess,
					FileAttributes, ShareAccess,
					CreateDisposition, CreateOptions,
					SecurityFlags)) < 0) {

			/* Handle the error ... */

			SAFE_FREE(file);
			errno = smbc_errno(context, targetcli);
			TALLOC_FREE(frame);
			return NULL;

		}

		/* Fill in file struct */

		file->cli_fd  = fd;
		file->fname   = SMB_STRDUP(fname);
		file->srv     = srv;
		file->offset  = 0;
		file->file    = True;

		DLIST_ADD(context->internal->_files, file);

		return file;
	}

	/* Check if opendir needed ... */

	if (fd == -1) {
		int eno = 0;

		eno = smbc_errno(context, srv->cli);
		file = context->opendir(context, fname);
		if (!file) errno = eno;
		TALLOC_FREE(frame);
		return file;

	}

	errno = EINVAL; /* FIXME, correct errno ? */
	TALLOC_FREE(frame);
	return NULL;
}


void
smbc_get_session_key(SMBCFILE *file, unsigned char **key, size_t *keylen)
{
	DATA_BLOB blob;
	if (file == NULL || key == NULL || keylen == NULL) return;

	blob    = file->srv->cli->user_session_key;
	*key    = (unsigned char*)blob.data;
	*keylen = blob.length;
}
