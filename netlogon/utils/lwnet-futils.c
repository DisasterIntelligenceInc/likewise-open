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
 *        lwnet-futils.c
 *
 * Abstract:
 *
 *        Likewise Site Manager
 *
 *        File Utilities
 *
 * Authors: Krishna Ganugapati (krishnag@likewisesoftware.com)
 *          Sriram Nambakam (snambakam@likewisesoftware.com)
 * 
 */
#include "includes.h"

DWORD
LWNetRemoveFile(
    PCSTR pszPath
    )
{
    DWORD dwError = 0;

    while (1) {
        if (unlink(pszPath) < 0) {
            if (errno == EINTR) {
                continue;
            }
            dwError = errno;
            BAIL_ON_LWNET_ERROR(dwError);
        } else {
            break;
        }
    }

error:

    return dwError;
}

DWORD
LWNetCheckFileExists(
    PCSTR pszPath,
    PBOOLEAN pbFileExists
    )
{
    DWORD dwError = 0;

    struct stat statbuf;

    memset(&statbuf, 0, sizeof(struct stat));

    while (1) {
        if (stat(pszPath, &statbuf) < 0) {
           if (errno == EINTR) {
              continue;
           } else if (errno == ENOENT) {
             *pbFileExists = 0;
             break;
           }
           dwError = errno;
           BAIL_ON_LWNET_ERROR(dwError);
        } else {
          *pbFileExists = 1;
          break;
        }
    }

error:

    return dwError;
}

DWORD
LWNetCheckSockExists(
    PCSTR pszPath,
    PBOOLEAN pbSockExists
    )
{
    DWORD dwError = 0;

    struct stat statbuf;

    memset(&statbuf, 0, sizeof(struct stat));

    while (1) {
        if (stat(pszPath, &statbuf) < 0) {
           if (errno == EINTR) {
              continue;
           } else if (errno == ENOENT || errno == ENOTDIR) {
             *pbSockExists = 0;
             break;
           }
           dwError = errno;
           BAIL_ON_LWNET_ERROR(dwError);
        } else {
          *pbSockExists = (((statbuf.st_mode & S_IFMT) == S_IFSOCK) ? TRUE : FALSE);
          break;
        }
    }

error:

    return dwError;
}

DWORD
LWNetMoveFile(    
    PCSTR pszSrcPath,
    PCSTR pszDstPath
    )
{
    DWORD dwError = 0;

    if (rename(pszSrcPath, pszDstPath) < 0) {
        dwError = errno;
    }

    return dwError;
}

DWORD
LWNetChangePermissions(
    PCSTR pszPath,
    mode_t dwFileMode
    )
{
    DWORD dwError = 0;

    while (1) {
        if (chmod(pszPath, dwFileMode) < 0) {
            if (errno == EINTR) {
                continue;
            }
            dwError = errno;
            BAIL_ON_LWNET_ERROR(dwError);
        } else {
            break;
        }
    }

error:

    return dwError;
}

DWORD
LWNetChangeOwner(
    PCSTR pszPath,
    uid_t uid,
    gid_t gid
    )
{
    DWORD dwError = 0;
    struct stat statbuf = {0};
    
    if (lstat(pszPath, &statbuf) < 0) {
        dwError = errno;
        BAIL_ON_LWNET_ERROR(dwError);
    }
    
    while (1) {

        if (S_ISLNK(statbuf.st_mode)) {
            
            if (lchown(pszPath, uid, gid) < 0) {
                if (errno == EINTR) {
                    continue;
                }
                dwError = errno;
                BAIL_ON_LWNET_ERROR(dwError);
            } else {
                break;
            }
            
        } else {
            
            if (chown(pszPath, uid, gid) < 0) {
                if (errno == EINTR) {
                    continue;
                }
                dwError = errno;
                BAIL_ON_LWNET_ERROR(dwError);
            } else {
                break;
            }
        }
    }

error:

    return dwError;
}

DWORD
LWNetChangeOwnerAndPermissions(
    PCSTR pszPath,
    uid_t uid,
    gid_t gid,
    mode_t dwFileMode
    )
{
    DWORD dwError = 0;

    dwError = LWNetChangeOwner(pszPath, uid, gid);
    BAIL_ON_LWNET_ERROR(dwError);

    dwError = LWNetChangePermissions(pszPath, dwFileMode);
    BAIL_ON_LWNET_ERROR(dwError);

error:

    return dwError;
}

DWORD
LWNetChangeDirectory(
    PSTR pszPath
    )
{
    if (pszPath == NULL || *pszPath == '\0')
        return EINVAL;

    if (chdir(pszPath) < 0)
        return errno;

    return 0;
}

/*
// TODO: Check access and removability before actual deletion
*/
DWORD
LWNetRemoveDirectory(
    PCSTR pszPath
    )
{
    DWORD dwError = 0;
    DIR* pDir = NULL;
    struct dirent* pDirEntry = NULL;
    struct stat statbuf;
    CHAR szBuf[PATH_MAX+1];

    if ((pDir = opendir(pszPath)) == NULL) {
        dwError = errno;
        BAIL_ON_LWNET_ERROR(dwError);
    }

    while ((pDirEntry = readdir(pDir)) != NULL) {

        if (!strcmp(pDirEntry->d_name, "..") ||
            !strcmp(pDirEntry->d_name, "."))
            continue;

        sprintf(szBuf, "%s/%s", pszPath, pDirEntry->d_name);

        memset(&statbuf, 0, sizeof(struct stat));

        if (stat(szBuf, &statbuf) < 0) {
            dwError = errno;
            BAIL_ON_LWNET_ERROR(dwError);
        }

        if ((statbuf.st_mode & S_IFMT) == S_IFDIR) {
            dwError = LWNetRemoveDirectory(szBuf);
            BAIL_ON_LWNET_ERROR(dwError);

            if (rmdir(szBuf) < 0) {
                dwError = errno;
                BAIL_ON_LWNET_ERROR(dwError);
            }

        } else {

            dwError = LWNetRemoveFile(szBuf);
            BAIL_ON_LWNET_ERROR(dwError);

        }
    }

error:

    if (pDir)
        closedir(pDir);

    return dwError;
}

DWORD
LWNetCheckDirectoryExists(
    PCSTR pszPath,
    PBOOLEAN pbDirExists
    )
{
    DWORD dwError = 0;

    struct stat statbuf;

    while (1) {

        memset(&statbuf, 0, sizeof(struct stat));

        if (stat(pszPath, &statbuf) < 0) {

            if (errno == EINTR) {
                continue;
            }
            else if (errno == ENOENT || errno == ENOTDIR) {
                *pbDirExists = FALSE;
                break;
            }
            dwError = errno;
            BAIL_ON_LWNET_ERROR(dwError);

        }

        /*
           The path exists. Is it a directory?
         */

        *pbDirExists = (((statbuf.st_mode & S_IFMT) == S_IFDIR) ? TRUE : FALSE);
        break;
    }

error:

    return dwError;
}

DWORD
LWNetGetCurrentDirectoryPath(
    PSTR* ppszPath
    )
{
    DWORD dwError = 0;
    CHAR szBuf[PATH_MAX+1];
    PSTR pszPath = NULL;

    if (getcwd(szBuf, PATH_MAX) == NULL) {
        dwError = errno;
        BAIL_ON_LWNET_ERROR(dwError);
    }

    dwError = LWNetAllocateString(szBuf, &pszPath);
    BAIL_ON_LWNET_ERROR(dwError);

    *ppszPath = pszPath;

    return dwError;

error:

    if (pszPath) {
        LWNetFreeString(pszPath);
    }

    return dwError;
}

static
DWORD
LWNetCreateDirectoryRecursive(
    PSTR pszCurDirPath,
    PSTR pszTmpPath,
    PSTR *ppszTmp,
    DWORD dwFileMode,
    DWORD dwWorkingFileMode,
    int  iPart
    )
{
    DWORD dwError = 0;
    PSTR pszDirPath = NULL;
    BOOLEAN bDirCreated = FALSE;
    BOOLEAN bDirExists = FALSE;
    CHAR szDelimiters[] = "/";

    PSTR pszToken = strtok_r((iPart ? NULL : pszTmpPath), szDelimiters, ppszTmp);

    if (pszToken != NULL) {

        dwError = LWNetAllocateMemory(strlen(pszCurDirPath)+strlen(pszToken)+2,
                                   (PVOID*)&pszDirPath);
        BAIL_ON_LWNET_ERROR(dwError);

        sprintf(pszDirPath,
                "%s/%s",
                (!strcmp(pszCurDirPath, "/") ? "" : pszCurDirPath),
                pszToken);


        dwError = LWNetCheckDirectoryExists(pszDirPath, &bDirExists);
        BAIL_ON_LWNET_ERROR(dwError);

        if (!bDirExists) {
            if (mkdir(pszDirPath, dwWorkingFileMode) < 0) {
                dwError = errno;
                BAIL_ON_LWNET_ERROR(dwError);
            }
            bDirCreated = TRUE;
        }

        dwError = LWNetChangeDirectory(pszDirPath);
        BAIL_ON_LWNET_ERROR(dwError);

        dwError = LWNetCreateDirectoryRecursive(
            pszDirPath,
            pszTmpPath,
            ppszTmp,
            dwFileMode,
            dwWorkingFileMode,
            iPart+1
            );
        BAIL_ON_LWNET_ERROR(dwError);
    }

    if (bDirCreated && (dwFileMode != dwWorkingFileMode)) {
        dwError = LWNetChangePermissions(pszDirPath, dwFileMode);
        BAIL_ON_LWNET_ERROR(dwError);
    }
    if (pszDirPath) {
        LWNetFreeMemory(pszDirPath);
    }

    return dwError;

error:

    if (bDirCreated) {
        LWNetRemoveDirectory(pszDirPath);
    }

    if (pszDirPath) {
        LWNetFreeMemory(pszDirPath);
    }

    return dwError;
}

DWORD
LWNetCreateDirectory(
    PCSTR pszPath,
    mode_t dwFileMode
    )
{
    DWORD dwError = 0;
    PSTR pszCurDirPath = NULL;
    PSTR pszTmpPath = NULL;
    PSTR pszTmp = NULL;
    mode_t dwWorkingFileMode;

    if (pszPath == NULL || *pszPath == '\0') {
        dwError = EINVAL;
        BAIL_ON_LWNET_ERROR(dwError);
    }

    dwWorkingFileMode = dwFileMode;
    if (!(dwFileMode & S_IXUSR)) {
        /*
         * This is so that we can navigate the folders
         * when we are creating the subfolders
         */
        dwWorkingFileMode |= S_IXUSR;
    }

    dwError = LWNetGetCurrentDirectoryPath(&pszCurDirPath);
    BAIL_ON_LWNET_ERROR(dwError);

    dwError = LWNetAllocateString(pszPath, &pszTmpPath);
    BAIL_ON_LWNET_ERROR(dwError);

    if (*pszPath == '/') {
        dwError = LWNetChangeDirectory("/");
        BAIL_ON_LWNET_ERROR(dwError);

        dwError = LWNetCreateDirectoryRecursive("/", pszTmpPath, &pszTmp, dwFileMode, dwWorkingFileMode, 0);
        BAIL_ON_LWNET_ERROR(dwError);

    } else {

        dwError = LWNetCreateDirectoryRecursive(pszCurDirPath, pszTmpPath, &pszTmp, dwFileMode, dwWorkingFileMode, 0);
        BAIL_ON_LWNET_ERROR(dwError);

    }

error:

    if (pszCurDirPath) {

        LWNetChangeDirectory(pszCurDirPath);

        LWNetFreeMemory(pszCurDirPath);

    }

    if (pszTmpPath) {
        LWNetFreeMemory(pszTmpPath);
    }

    return dwError;
}

DWORD
LWNetGetOwnerAndPermissions(
    PCSTR pszSrcPath,
    uid_t * uid,
    gid_t * gid,
    mode_t * mode
    )
{
    DWORD dwError = 0;
    struct stat statbuf;

    memset(&statbuf, 0, sizeof(struct stat));

    if (stat(pszSrcPath, &statbuf) < 0) {
        dwError = errno;
        BAIL_ON_LWNET_ERROR(dwError);
    }

    *uid = statbuf.st_uid;
    *gid = statbuf.st_gid;
    *mode = statbuf.st_mode;

error:
 
    return dwError;
}

DWORD
LWNetCopyFileWithPerms(
    PCSTR pszSrcPath,
    PCSTR pszDstPath,
    mode_t dwPerms
    )
{
    DWORD dwError = 0;
    PCSTR pszTmpSuffix = ".tmp_likewise_lwnet";
    PSTR pszTmpPath = NULL;
    BOOLEAN bRemoveFile = FALSE;
    CHAR szBuf[1024+1];
    int  iFd = -1;
    int  oFd = -1;
    DWORD dwBytesRead = 0;

    if (IsNullOrEmptyString(pszSrcPath) ||
        IsNullOrEmptyString(pszDstPath)) {
        dwError = EINVAL;
        BAIL_ON_LWNET_ERROR(dwError);
    }

    dwError = LWNetAllocateMemory(strlen(pszDstPath)+strlen(pszTmpSuffix)+2,
                               (PVOID*)&pszTmpPath);
    BAIL_ON_LWNET_ERROR(dwError);

    strcpy(pszTmpPath, pszDstPath);
    strcat(pszTmpPath, pszTmpSuffix);

    if ((iFd = open(pszSrcPath, O_RDONLY, S_IRUSR)) < 0) {
        dwError = errno;
        BAIL_ON_LWNET_ERROR(dwError);
    }

    if ((oFd = open(pszTmpPath, O_WRONLY|O_TRUNC|O_CREAT, S_IRUSR|S_IWUSR)) < 0) {
        dwError = errno;
        BAIL_ON_LWNET_ERROR(dwError);
    }

    bRemoveFile = TRUE;

    while (1) {
        if ((dwBytesRead = read(iFd, szBuf, 1024)) < 0) {

            if (errno == EINTR)
                continue;

            dwError = errno;
            BAIL_ON_LWNET_ERROR(dwError);
        }

        if (dwBytesRead == 0)
            break;

        if (write(oFd, szBuf, dwBytesRead) != dwBytesRead) {

            if (errno == EINTR)
                continue;

            dwError = errno;
            BAIL_ON_LWNET_ERROR(dwError);

        }

    }

    close(iFd); iFd = -1;
    close(oFd); oFd = -1;

    dwError = LWNetMoveFile(pszTmpPath, pszDstPath);
    BAIL_ON_LWNET_ERROR(dwError);

    bRemoveFile = FALSE;

    dwError = LWNetChangePermissions(pszDstPath, dwPerms);
    BAIL_ON_LWNET_ERROR(dwError);

error:

    if (iFd >= 0)
        close(iFd);

    if (oFd >= 0)
        close(oFd);


    if (bRemoveFile) {
        LWNetRemoveFile(pszTmpPath);
    }

    LWNET_SAFE_FREE_STRING (pszTmpPath);

    return dwError;
}


DWORD
LWNetCopyFileWithOriginalPerms(
    PCSTR pszSrcPath,
    PCSTR pszDstPath
    )
{
    DWORD dwError = 0;
    uid_t uid;
    gid_t gid;
    mode_t mode;

    dwError = LWNetGetOwnerAndPermissions(pszSrcPath, &uid, &gid, &mode);
    BAIL_ON_LWNET_ERROR(dwError);

    dwError = LWNetCopyFileWithPerms(pszSrcPath, pszDstPath, mode);
    BAIL_ON_LWNET_ERROR(dwError);

    dwError = LWNetChangeOwnerAndPermissions(pszDstPath, uid, gid, mode);
    BAIL_ON_LWNET_ERROR(dwError);

error:

    return dwError;
}

DWORD
LWNetBackupFile(
    PCSTR pszPath
    )
{
    DWORD dwError = 0;
    BOOLEAN bExists = FALSE;
    CHAR    szBackupPath[PATH_MAX];

    dwError = LWNetCheckFileExists(pszPath, &bExists);
    BAIL_ON_LWNET_ERROR(dwError);

    if (!bExists)
    {
        /* Do not need to backup, since the file does not yet exist. */
        goto done;
    }

    sprintf(szBackupPath, "%s.likewise_lwnet.orig", pszPath);

    dwError = LWNetCheckFileExists(szBackupPath, &bExists);
    BAIL_ON_LWNET_ERROR(dwError);

    if (bExists)
    {
       sprintf(szBackupPath, "%s.likewise_lwnet.bak", pszPath);
    }

    dwError = LWNetCopyFileWithOriginalPerms(pszPath, szBackupPath);
    BAIL_ON_LWNET_ERROR(dwError);

done:
error:

    return dwError;
}

DWORD
LWNetGetSymlinkTarget(
   PCSTR pszPath,
   PSTR* ppszTargetPath
   )
{
    DWORD dwError = 0;
    CHAR szBuf[PATH_MAX+1];
    PSTR pszTargetPath = NULL;

    memset(szBuf, 0, sizeof(szBuf));

    while (1) {

       if (readlink(pszPath, szBuf, PATH_MAX) < 0) {
          if (errno == EINTR)
             continue;

          dwError = errno;
          BAIL_ON_LWNET_ERROR(dwError);
       }

       break;
    }
    
    dwError = LWNetAllocateString(
                    szBuf,
                    &pszTargetPath);
    BAIL_ON_LWNET_ERROR(dwError);
    
    *ppszTargetPath = pszTargetPath;
    
cleanup:

    return dwError;
    
error:

    *ppszTargetPath = NULL;
    
    LWNET_SAFE_FREE_STRING(pszTargetPath);

    goto cleanup;
}

DWORD
LWNetCreateSymlink(
   PCSTR pszOldPath,
   PCSTR pszNewPath
   )
{
    return ((symlink(pszOldPath, pszNewPath) < 0) ? errno : 0);
}

DWORD
LWNetCopyDirectory(
    PCSTR pszSourceDirPath,
    uid_t ownerUid,
    gid_t ownerGid,
    PCSTR pszDestDirPath
    )
{
    DWORD dwError = 0;
    DIR*  pDir = NULL;
    struct dirent* pDirEntry = NULL;
    struct stat statbuf = {0};
    CHAR  szSrcPath[PATH_MAX+1];
    CHAR  szDstPath[PATH_MAX+1];
    PSTR  pszTargetPath = NULL;
    
    if (NULL == (pDir = opendir(pszSourceDirPath))) {
       dwError = errno;
       BAIL_ON_LWNET_ERROR(dwError);
    }
    
    while (NULL != (pDirEntry = readdir(pDir)))
    {
        if (!strcmp(pDirEntry->d_name, ".") ||
            !strcmp(pDirEntry->d_name, "..")||
            !strcmp(pDirEntry->d_name, "lost+found"))
            continue;

        memset(&statbuf, 0, sizeof(statbuf));
        
        sprintf(szSrcPath, "%s/%s", pszSourceDirPath, pDirEntry->d_name);
        
        if (lstat(szSrcPath, &statbuf) < 0) {
            dwError = errno;
            BAIL_ON_LWNET_ERROR(dwError);
        }

        sprintf(szDstPath, "%s/%s", pszDestDirPath, pDirEntry->d_name);
        
        if (S_ISDIR(statbuf.st_mode)) {
            
            dwError = LWNetCreateDirectory(
                            szDstPath,
                            statbuf.st_mode);
            BAIL_ON_LWNET_ERROR(dwError);
            
            dwError = LWNetChangeOwner(
                            szDstPath,
                            ownerUid,
                            ownerGid);
            BAIL_ON_LWNET_ERROR(dwError);
            
            dwError = LWNetCopyDirectory(
                            szSrcPath,
                            ownerUid,
                            ownerGid,
                            szDstPath);
            BAIL_ON_LWNET_ERROR(dwError);
            
        } else if (S_ISREG(statbuf.st_mode)) {
            
            dwError = LWNetCopyFileWithOriginalPerms(
                            szSrcPath,
                            szDstPath);
            BAIL_ON_LWNET_ERROR(dwError);
            
            dwError = LWNetChangeOwner(
                            szDstPath,
                            ownerUid,
                            ownerGid);
            BAIL_ON_LWNET_ERROR(dwError);
            
        } else if (S_ISLNK(statbuf.st_mode)) {
            
            dwError = LWNetGetSymlinkTarget(
                            szSrcPath,
                            &pszTargetPath);
            BAIL_ON_LWNET_ERROR(dwError);
            
            dwError = LWNetCreateSymlink(
                            pszTargetPath,
                            szDstPath);
            BAIL_ON_LWNET_ERROR(dwError);
            
            dwError = LWNetChangeOwner(
                            szDstPath,
                            ownerUid,
                            ownerGid);
            BAIL_ON_LWNET_ERROR(dwError);
        }
    }
    
cleanup:

    if (pDir) {
        closedir(pDir);
    }
    
    LWNET_SAFE_FREE_STRING(pszTargetPath);

    return dwError;
    
error:

    goto cleanup;
}

DWORD
LWNetGetMatchingFilePathsInFolder(
    PCSTR pszDirPath,
    PCSTR pszFileNameRegExp,
    PSTR** pppszHostFilePaths,
    PDWORD pdwNPaths
    )
{
    typedef struct __PATHNODE
    {
        PSTR pszPath;
        struct __PATHNODE *pNext;
    } PATHNODE, *PPATHNODE;

    DWORD dwError = 0;
    DIR* pDir = NULL;
    struct dirent*  pDirEntry = NULL;
    regex_t rx;
    BOOLEAN rxAllocated = FALSE;
    regmatch_t* pResult = NULL;
    size_t nMatch = 1;
    DWORD  dwNPaths = 0;
    DWORD  iPath = 0;
    PSTR*  ppszHostFilePaths = NULL;
    CHAR   szBuf[PATH_MAX+1];
    struct stat statbuf;
    PPATHNODE pPathList = NULL;
    PPATHNODE pPathNode = NULL;
    BOOLEAN bDirExists = FALSE;

    dwError = LWNetCheckDirectoryExists(pszDirPath, &bDirExists);
    BAIL_ON_LWNET_ERROR(dwError);

    if(!bDirExists) {
        dwError = ENOENT;
        BAIL_ON_LWNET_ERROR(dwError);
    }

    if (regcomp(&rx, pszFileNameRegExp, REG_EXTENDED) != 0) {
        dwError = LWNET_ERROR_REGEX_COMPILE_FAILED;
        BAIL_ON_LWNET_ERROR(dwError);
    }
    rxAllocated = TRUE;

    dwError = LWNetAllocateMemory(sizeof(regmatch_t), (PVOID*)&pResult);
    BAIL_ON_LWNET_ERROR(dwError);

    pDir = opendir(pszDirPath);
    if (!pDir) {
        dwError = errno;
        BAIL_ON_LWNET_ERROR(dwError);
    }

    while ((pDirEntry = readdir(pDir)) != NULL) {

        int copied = snprintf(
                            szBuf, 
                            sizeof(szBuf), 
                            "%s/%s", 
                            pszDirPath, 
                            pDirEntry->d_name);
        if (copied >= sizeof(szBuf))
        {
            //Skip pathnames that are too long
            continue;
        }
        memset(&statbuf, 0, sizeof(struct stat));
        if (lstat(szBuf, &statbuf) < 0) {
            if(errno == ENOENT)
            {
                //This occurs when there is a symbolic link pointing to a
                //location that doesn't exist, because stat looks at the final
                //file, not the link. Since this file doesn't exist anyway,
                //just skip it.
                continue;
            }
            dwError = errno;
            BAIL_ON_LWNET_ERROR(dwError);
        }

        /*
         * For now, we are searching only for regular files
         * This may be enhanced in the future to support additional
         * file system entry types
         */
        if (((statbuf.st_mode & S_IFMT) == S_IFREG) &&
            (regexec(&rx, pDirEntry->d_name, nMatch, pResult, 0) == 0)) {
            dwNPaths++;

            dwError = LWNetAllocateMemory(sizeof(PATHNODE), (PVOID*)&pPathNode);
            BAIL_ON_LWNET_ERROR(dwError);

            dwError = LWNetAllocateString(szBuf, &pPathNode->pszPath);
            BAIL_ON_LWNET_ERROR(dwError);

            pPathNode->pNext = pPathList;
            pPathList = pPathNode;
            pPathNode = NULL;
        }
    }

    if (pPathList) {
        dwError = LWNetAllocateMemory(sizeof(PSTR)*dwNPaths,
                                    (PVOID*)&ppszHostFilePaths);
        BAIL_ON_LWNET_ERROR(dwError);
        /*
         *  The linked list is in reverse.
         *  Assign values in reverse to get it right
         */
        iPath = dwNPaths-1;
        pPathNode = pPathList;
        while (pPathNode) {
            *(ppszHostFilePaths+iPath) = pPathNode->pszPath;
            pPathNode->pszPath = NULL;
            pPathNode = pPathNode->pNext;
            iPath--;
        }
    }

    *pppszHostFilePaths = ppszHostFilePaths;
    *pdwNPaths = dwNPaths;
    
cleanup:

    if (pPathNode) {
        LWNET_SAFE_FREE_STRING(pPathNode->pszPath);
        LWNetFreeMemory(pPathNode);
    }

    while(pPathList) {
        pPathNode = pPathList;
        pPathList = pPathList->pNext;
        LWNET_SAFE_FREE_STRING(pPathNode->pszPath);
        LWNetFreeMemory(pPathNode);
    }

    if (rxAllocated) {
        regfree(&rx);
    }

    LWNET_SAFE_FREE_MEMORY(pResult);

    if (pDir) {
        closedir(pDir);
    }

    return dwError;

error:

    if (ppszHostFilePaths) {
       LWNetFreeStringArray(ppszHostFilePaths, dwNPaths);
    }

    goto cleanup;
}

DWORD
LWNetReadNextLine(
    FILE* fp,
    PSTR *output,
    PBOOLEAN pbEndOfFile
    )
{
    DWORD dwError = 0;
    PVOID pBuffer = NULL;
    DWORD dwSize = 0;
    DWORD dwMaxSize = 100;

    *pbEndOfFile = 0;
    *output = NULL;

    dwError = LWNetAllocateMemory(
                  dwMaxSize,
                  &pBuffer);
    BAIL_ON_LWNET_ERROR(dwError);

    while(1)
    {
        if(fgets(pBuffer + dwSize,
                dwMaxSize - dwSize, fp) ==  NULL)
        {
            if (feof(fp)) {
                *pbEndOfFile = 1;
            } else {
                dwError = errno;
                BAIL_ON_LWNET_ERROR(dwError);
            }
        }
        dwSize += strlen(pBuffer + dwSize);

        if (*pbEndOfFile)
        {
            break;
        }
        if (dwSize == dwMaxSize - 1 &&
                ((char *)pBuffer)[dwSize - 1] != '\n')
        {
            dwMaxSize *= 2;
            dwError = LWNetReallocMemory(
                          pBuffer,
                          &pBuffer,
                          dwMaxSize);
            BAIL_ON_LWNET_ERROR(dwError);
        }
        else
        {
            break;
        }
    }

    ((char *)pBuffer)[dwSize] = 0;

    *output = pBuffer;
    pBuffer = NULL;

cleanup:

    LWNET_SAFE_FREE_MEMORY(pBuffer);

    return dwError;

error:

    goto cleanup;
}
