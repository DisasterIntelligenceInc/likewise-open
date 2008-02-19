#!/bin/sh

VERSION_FILE=$1
OUTPUT_FILE=$2

if test -z "$VERSION_FILE";then
	VERSION_FILE="VERSION"
fi

if test -z "$OUTPUT_FILE";then
	OUTPUT_FILE="include/version.h"
fi

SOURCE_DIR=$3

SAMBA_VERSION_MAJOR=`sed -n 's/^SAMBA_VERSION_MAJOR=//p' $SOURCE_DIR$VERSION_FILE`
SAMBA_VERSION_MINOR=`sed -n 's/^SAMBA_VERSION_MINOR=//p' $SOURCE_DIR$VERSION_FILE`
SAMBA_VERSION_RELEASE=`sed -n 's/^SAMBA_VERSION_RELEASE=//p' $SOURCE_DIR$VERSION_FILE`

SAMBA_VERSION_REVISION=`sed -n 's/^SAMBA_VERSION_REVISION=//p' $SOURCE_DIR$VERSION_FILE`

SAMBA_VERSION_TP_RELEASE=`sed -n 's/^SAMBA_VERSION_TP_RELEASE=//p' $SOURCE_DIR$VERSION_FILE`
SAMBA_VERSION_PRE_RELEASE=`sed -n 's/^SAMBA_VERSION_PRE_RELEASE=//p' $SOURCE_DIR$VERSION_FILE`
SAMBA_VERSION_RC_RELEASE=`sed -n 's/^SAMBA_VERSION_RC_RELEASE=//p' $SOURCE_DIR$VERSION_FILE`

SAMBA_VERSION_IS_GIT_SNAPSHOT=`sed -n 's/^SAMBA_VERSION_IS_GIT_SNAPSHOT=//p' $SOURCE_DIR$VERSION_FILE`

SAMBA_VERSION_RELEASE_NICKNAME=`sed -n 's/^SAMBA_VERSION_RELEASE_NICKNAME=//p' $SOURCE_DIR$VERSION_FILE`

SAMBA_VERSION_VENDOR_SUFFIX=`sed -n 's/^SAMBA_VERSION_VENDOR_SUFFIX=//p' $SOURCE_DIR$VERSION_FILE`
SAMBA_VERSION_VENDOR_PATCH=`sed -n 's/^SAMBA_VERSION_VENDOR_PATCH=//p' $SOURCE_DIR$VERSION_FILE`

LIKEWISE_AUTH_MAJOR=`sed -n 's/^LIKEWISE_AUTH_MAJOR=//p' $SOURCE_DIR$VERSION_FILE`
LIKEWISE_AUTH_MINOR=`sed -n 's/^LIKEWISE_AUTH_MINOR=//p' $SOURCE_DIR$VERSION_FILE`
LIKEWISE_AUTH_RELEASE=`sed -n 's/^LIKEWISE_AUTH_RELEASE=//p' $SOURCE_DIR$VERSION_FILE`

echo "/* Autogenerated by script/mkversion.sh */" > $OUTPUT_FILE

echo "#define SAMBA_VERSION_MAJOR ${SAMBA_VERSION_MAJOR}" >> $OUTPUT_FILE
echo "#define SAMBA_VERSION_MINOR ${SAMBA_VERSION_MINOR}" >> $OUTPUT_FILE
echo "#define SAMBA_VERSION_RELEASE ${SAMBA_VERSION_RELEASE}" >> $OUTPUT_FILE

echo "#define LIKEWISE_AUTH_MAJOR ${LIKEWISE_AUTH_MAJOR}" >> $OUTPUT_FILE
echo "#define LIKEWISE_AUTH_MINOR ${LIKEWISE_AUTH_MINOR}" >> $OUTPUT_FILE
echo "#define LIKEWISE_AUTH_RELEASE ${LIKEWISE_AUTH_RELEASE}" >> $OUTPUT_FILE


##
## start with "3.0.22"
##
SAMBA_VERSION_STRING="${SAMBA_VERSION_MAJOR}.${SAMBA_VERSION_MINOR}.${SAMBA_VERSION_RELEASE}"

LIKEWISE_VERSION_STRING="${LIKEWISE_AUTH_MAJOR}.${LIKEWISE_AUTH_MINOR}.${LIKEWISE_AUTH_RELEASE}"


##
## maybe add "3.0.22a" or "4.0.0tp11" or "3.0.22pre1" or "3.0.22rc1"
## We do not do pre or rc version on patch/letter releases
##
if test -n "${SAMBA_VERSION_REVISION}";then
    SAMBA_VERSION_STRING="${SAMBA_VERSION_STRING}${SAMBA_VERSION_REVISION}"
    echo "#define SAMBA_VERSION_REVISION \"${SAMBA_VERSION_REVISION}\"" >> $OUTPUT_FILE
elif test -n "${SAMBA_VERSION_TP_RELEASE}";then
    SAMBA_VERSION_STRING="${SAMBA_VERSION_STRING}tp${SAMBA_VERSION_TP_RELEASE}"
    echo "#define SAMBA_VERSION_TP_RELEASE ${SAMBA_VERSION_TP_RELEASE}" >> $OUTPUT_FILE
elif test -n "${SAMBA_VERSION_PRE_RELEASE}";then
    ## maybe add "3.0.22pre2"
    SAMBA_VERSION_STRING="${SAMBA_VERSION_STRING}pre${SAMBA_VERSION_PRE_RELEASE}"
    echo "#define SAMBA_VERSION_PRE_RELEASE ${SAMBA_VERSION_PRE_RELEASE}" >> $OUTPUT_FILE
elif test -n "${SAMBA_VERSION_RC_RELEASE}";then
    SAMBA_VERSION_STRING="${SAMBA_VERSION_STRING}rc${SAMBA_VERSION_RC_RELEASE}"
    echo "#define SAMBA_VERSION_RC_RELEASE ${SAMBA_VERSION_RC_RELEASE}" >> $OUTPUT_FILE
fi

##
## GIT commit details
##
if test x"${SAMBA_VERSION_IS_GIT_SNAPSHOT}" = x"yes";then
    _SAVE_LANG=${LANG}
    LANG="C"
    HAVEVER="no"

    if test x"${HAVEVER}" != x"yes" -a -d "${SOURCE_DIR}../.git";then
	HAVEGIT=no
	GIT_INFO=`git show --pretty=format:"%h%n%ct%n%H%n%cd" --stat HEAD 2>/dev/null`
	GIT_COMMIT_ABBREV=`echo -e "${GIT_INFO}" | sed -n 1p`
	GIT_COMMIT_TIME=`echo -e "${GIT_INFO}" | sed -n 2p`
	GIT_COMMIT_FULLREV=`echo -e "${GIT_INFO}" | sed -n 3p`
	GIT_COMMIT_DATE=`echo -e "${GIT_INFO}" | sed -n 4p`
	if test -n "${GIT_COMMIT_ABBREV}";then
	    HAVEGIT=yes
            HAVEVER=yes
	fi
    fi

    if test x"${HAVEGIT}" = x"yes";then
	SAMBA_VERSION_STRING="${SAMBA_VERSION_STRING}-GIT-${GIT_COMMIT_ABBREV}"

	echo "#define SAMBA_VERSION_GIT_COMMIT_ABBREV \"${GIT_COMMIT_ABBREV}\"" >> $OUTPUT_FILE
	echo "#define SAMBA_VERSION_GIT_COMMIT_TIME ${GIT_COMMIT_TIME}" >> $OUTPUT_FILE
	echo "#define SAMBA_VERSION_GIT_COMMIT_FULLREV \"${GIT_COMMIT_FULLREV}\"" >> $OUTPUT_FILE
	echo "#define SAMBA_VERSION_GIT_COMMIT_DATE \"${GIT_COMMIT_DATE}\"" >> $OUTPUT_FILE
    else
	SAMBA_VERSION_STRING="${SAMBA_VERSION_STRING}-GIT-UNKNOWN"
    fi
    LANG=${_SAVE_LANG}
fi

echo "#define SAMBA_VERSION_OFFICIAL_STRING \"${SAMBA_VERSION_STRING}\"" >> $OUTPUT_FILE
echo "#define LIKEWISE_VERSION_OFFICIAL_STRING \"${LIKEWISE_VERSION_STRING}\"" >> $OUTPUT_FILE

##
## Add the vendor string if present
##
if test -n "${SAMBA_VERSION_VENDOR_SUFFIX}";then
    echo "#define SAMBA_VERSION_VENDOR_SUFFIX ${SAMBA_VERSION_VENDOR_SUFFIX}" >> $OUTPUT_FILE
    SAMBA_VERSION_STRING="${SAMBA_VERSION_STRING}-${SAMBA_VERSION_VENDOR_SUFFIX}"
    if test -n "${SAMBA_VERSION_VENDOR_PATCH}";then
        echo "#define SAMBA_VERSION_VENDOR_PATCH ${SAMBA_VERSION_VENDOR_PATCH}" >> $OUTPUT_FILE
        SAMBA_VERSION_STRING="${SAMBA_VERSION_STRING}-${SAMBA_VERSION_VENDOR_PATCH}"
    fi
fi

##
## Add a release nickname
##
if test -n "${SAMBA_VERSION_RELEASE_NICKNAME}";then
    echo "#define SAMBA_VERSION_RELEASE_NICKNAME ${SAMBA_VERSION_RELEASE_NICKNAME}" >> $OUTPUT_FILE
    SAMBA_VERSION_STRING="${SAMBA_VERSION_STRING} (${SAMBA_VERSION_RELEASE_NICKNAME})"
fi

echo "#define SAMBA_VERSION_STRING samba_version_string()" >> $OUTPUT_FILE
echo "#define LIKEWISE_VERSION_STRING likewise_version_string()" >> $OUTPUT_FILE

echo "$0: '$OUTPUT_FILE' created for Likewise-Identity(\"${LIKEWISE_VERSION_STRING}\")"

exit 0



