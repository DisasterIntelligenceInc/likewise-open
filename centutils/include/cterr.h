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

#ifndef _CENTERROR_H
#define _CENTERROR_H

/*
 * Likewise software error code definitions.
 *
 * Current list of components with specific error codes:
 *     Group Policy component of Likewise Identity
 *     Some other component of Likewise Identity
 *
 * Error code field structure format:
 *     32 bit length total length
 *         0x00000000 - 0x0000FFFF = Likewise System error range
 *         0x00010000 - 0x0001FFFF = Likewise ErrNo error range
 *         0x00050000 - 0x0005FFFF = Likewise Group Policy error range
 *         0x00070000 - 0x0007FFFF = Likewise Registry CSE error range
 *         0x00080000 - 0x0008FFFF = Likewise Domainjoin error range
 *         0x00060000 - 0x000FFFFF = Reserved for future Likewise software components
 */


typedef uint32_t CENTERROR;


#define CENTERROR_IS_OK(x) (x == 0)
#define CENTERROR_EQUAL(x,y) (x == y)

#define CENTERROR_COMPONENT(x) ((x & 0x0FFF0000) >> 16)

#define CENTERROR_COMPONENT_SYSTEM     0x000 /* Likewise software System */
#define CENTERROR_COMPONENT_ERRNO      0x001 /* Likewise software ErrNo */
#define CENTERROR_COMPONENT_GP         0x005 /* Likewise software Group Policy Agent (Centeris-GPAgent) */
#define CENTERROR_COMPONENT_REGISTRY   0x007 /* Likewise software Elektra */
#define CENTERROR_COMPONENT_DOMAINJOIN 0x008 /* Likewise software Domainjoin */

/*
 * #define CENTERROR_COMPONENT_Other  0x006 // Example: Other Likewise software Component
 */

/*
 * Likewise software errno error macros
 */
#define CENTERROR_ERRNO_MASK (CENTERROR_COMPONENT_ERRNO << 16)
#define CENTERROR_ERRNO(code) (CENTERROR_ERRNO_MASK | ((code) & 0xFFFF))
#define CENTERROR_IS_ERRNO(x) ((x & 0x0FFF0000) == CENTERROR_ERRNO_MASK)
#define CENTERROR_ERRNO_CODE(x) (x & 0x0000FFFF)
/*
 * Likewise software system error macros
 */
#define CENTERROR_SYSTEM_MASK (CENTERROR_COMPONENT_SYSTEM << 16)
#define CENTERROR_SYSTEM(code) (CENTERROR_SYSTEM_MASK | code)
#define CENTERROR_IS_SYSTEM(x) ((x & 0x0FFF0000) == CENTERROR_SYSTEM_MASK)
#define CENTERROR_SYSTEM_CODE(x) (x & 0x0000FFFF)
/*
 * Group Policy error macros
 */
#define CENTERROR_GP_MASK (CENTERROR_COMPONENT_GP << 16)
#define CENTERROR_GP(code) (CENTERROR_GP_MASK | code)
#define CENTERROR_IS_GP(x) ((x & 0x0FFF0000) == CENTERROR_GP_MASK)
#define CENTERROR_GP_CODE(x) (x & 0x0000FFFF)
/*
 * Elektra error macros
 */
#define CENTERROR_REGISTRY_MASK (CENTERROR_COMPONENT_REGISTRY << 16)
#define CENTERROR_REGISTRY(code) (CENTERROR_REGISTRY_MASK | code)
#define CENTERROR_IS_REGISTRY(x) ((x & 0x0FFF0000) == CENTERROR_REGISTRY_MASK)
#define CENTERROR_REGISTRY_CODE(x) (x & 0x0000FFFF)

/*
 * DomainJoin error macros
 */
#define CENTERROR_DOMAINJOIN_MASK (CENTERROR_COMPONENT_DOMAINJOIN << 16)
#define CENTERROR_DOMAINJOIN(code) (CENTERROR_DOMAINJOIN_MASK | code)
#define CENTERROR_IS_DOMAINJOIN(x) ((x & 0x0FFF0000) == CENTERROR_DOMAINJOIN_MASK)
#define CENTERROR_DOMAINJOIN_CODE(x) (x & 0x0000FFFF)

#define CENTERROR_SUCCESS                       0x00000000                    //      0
#define CENTERROR_ACCESS_DENIED                 CENTERROR_SYSTEM(0x00000005)  //      5
#define CENTERROR_OUT_OF_MEMORY                 CENTERROR_SYSTEM(0x0000000E)  //     14
#define CENTERROR_INVALID_PASSWORD              CENTERROR_SYSTEM(0x00000056)  //     86
#define CENTERROR_INVALID_PARAMETER             CENTERROR_SYSTEM(0x00000057)  //     87
#define CENTERROR_INVALID_OPERATION             CENTERROR_SYSTEM(0x00000058)  //     88
#define CENTERROR_INVALID_MESSAGE               CENTERROR_SYSTEM(0x000003EA)  //   1002
#define CENTERROR_CONNECTION_UNAVAIL            CENTERROR_SYSTEM(0x000004B1)  //   1201
#define CENTERROR_INVALID_GROUPNAME             CENTERROR_SYSTEM(0x000004B9)  //   1209
#define CENTERROR_INVALID_COMPUTERNAME          CENTERROR_SYSTEM(0x000004BA)  //   1210
#define CENTERROR_INVALID_DOMAINNAME            CENTERROR_SYSTEM(0x000004BC)  //   1212
#define CENTERROR_INVALID_SERVICENAME           CENTERROR_SYSTEM(0x000004BD)  //   1213
#define CENTERROR_INVALID_NETNAME               CENTERROR_SYSTEM(0x000004BE)  //   1214
#define CENTERROR_INVALID_SHARENAME             CENTERROR_SYSTEM(0x000004BF)  //   1215
#define CENTERROR_INVALID_PASSWORDNAME          CENTERROR_SYSTEM(0x000004C0)  //   1216
#define CENTERROR_INVALID_VALUE                 CENTERROR_SYSTEM(0x000004C1)  //   1217
#define CENTERROR_INVALID_VALUE_TYPE            CENTERROR_SYSTEM(0x000004C2)  //   1218
#define CENTERROR_DUP_DOMAINNAME                CENTERROR_SYSTEM(0x000004C5)  //   1221
#define CENTERROR_NO_NETWORK                    CENTERROR_SYSTEM(0x000004C6)  //   1222
#define CENTERROR_CONNECTION_REFUSED            CENTERROR_SYSTEM(0x000004C9)  //   1225
#define CENTERROR_CONNECTION_INVALID            CENTERROR_SYSTEM(0x000004CD)  //   1229
#define CENTERROR_ILL_FORMED_PASSWORD           CENTERROR_SYSTEM(0x0000052C)  //   1324
#define CENTERROR_PASSWORD_RESTRICTION          CENTERROR_SYSTEM(0x0000052D)  //   1325
#define CENTERROR_LOGON_FAILURE                 CENTERROR_SYSTEM(0x0000052E)  //   1326
#define CENTERROR_PASSWORD_EXPIRED              CENTERROR_SYSTEM(0x00000532)  //   1330
#define CENTERROR_ACCOUNT_DISABLED              CENTERROR_SYSTEM(0x00000533)  //   1331
#define CENTERROR_BAD_TOKEN                     CENTERROR_SYSTEM(0x00000545)  //   1349
#define CENTERROR_BAD_GUID                      CENTERROR_SYSTEM(0x00000546)  //   1350
#define CENTERROR_NO_SUCH_DOMAIN                CENTERROR_SYSTEM(0x0000054B)  //   1355
#define CENTERROR_DOMAIN_EXISTS                 CENTERROR_SYSTEM(0x0000054C)  //   1356
#define CENTERROR_DOMAIN_JOIN_FAILED            CENTERROR_SYSTEM(0x0000054D)  //   1357
#define CENTERROR_DOMAIN_LEAVE_FAILED           CENTERROR_SYSTEM(0x0000054E)  //   1358
#define CENTERROR_LICENSE_INCORRECT             CENTERROR_SYSTEM(0x00002001)  //   8193
#define CENTERROR_LICENSE_EXPIRED               CENTERROR_SYSTEM(0x00002002)  //   8194
#define CENTERROR_LICENSE_ALREADY_IN_USE        CENTERROR_SYSTEM(0x00002003)  //   8195
#define CENTERROR_LICENSE_NOT_AVAILABLE_FOR_USE CENTERROR_SYSTEM(0x00002004)  //   8196
#define CENTERROR_LICENSE_AD_NOT_PROVISIONED    CENTERROR_SYSTEM(0x00002005)  //   8197
#define CENTERROR_LICENSE_DUPLICATE_PROVISIONED CENTERROR_SYSTEM(0x00002006)  //   8198
#define CENTERROR_LICENSE_NO_KEY_IN_DN          CENTERROR_SYSTEM(0x00002007)  //   8199
#define CENTERROR_CFG_INVALID_SECTION_NAME      CENTERROR_SYSTEM(0x00002008)  //   8200
#define CENTERROR_CFG_INVALID_NVPAIR_NAME       CENTERROR_SYSTEM(0x00002009)  //   8201
#define CENTERROR_CFG_SECTION_NOT_FOUND         CENTERROR_SYSTEM(0x0000200A)  //   8202
#define CENTERROR_CFG_VALUE_NOT_FOUND           CENTERROR_SYSTEM(0x0000200B)  //   8203
#define CENTERROR_VALUE_NOT_IN_SECTION          CENTERROR_SYSTEM(0x0000200C)  //   8204
#define CENTERROR_CFG_INVALID_SIGNATURE         CENTERROR_SYSTEM(0x0000200D)  //   8205
#define CENTERROR_CFG_NOT_ENOUGH_BUFFER         CENTERROR_SYSTEM(0x0000200E)  //   8206
#define CENTERROR_REGEX_COMPILE_FAILED          CENTERROR_SYSTEM(0x0000200F)  //   8207
#define CENTERROR_LWIAUTH_NOT_RUNNING           CENTERROR_SYSTEM(0x00002010)  //   8208
#define CENTERROR_NO_SUCH_PROCESS               CENTERROR_SYSTEM(0x00002011)  //   8209
#define CENTERROR_NO_SUCH_SYMBOL                CENTERROR_SYSTEM(0x00002012)  //   8210
#define CENTERROR_PTHREAD_ERROR                 CENTERROR_SYSTEM(0x00002013)  //   8211
#define CENTERROR_COMMAND_FAILED                CENTERROR_SYSTEM(0x00002014)  //   8212
#define CENTERROR_INVALID_UID                   CENTERROR_SYSTEM(0x00002015)  //   8213
#define CENTERROR_INVALID_GID                   CENTERROR_SYSTEM(0x00002016)  //   8214
#define CENTERROR_INVALID_FILENAME              CENTERROR_SYSTEM(0x00002017)  //   8215
//An option passed on the command line was invalid
#define CENTERROR_INVALID_OPTION_VALUE          CENTERROR_SYSTEM(0x00002018)  //   8216
#define CENTERROR_ABNORMAL_TERMINATION          CENTERROR_SYSTEM(0x00002019)  //   8217
#define CENTERROR_SED_NOT_FOUND	                CENTERROR_SYSTEM(0x0000201A)  //   8218
#define CENTERROR_SHELL_VARIABLE_UNKNOWN        CENTERROR_SYSTEM(0x0000201B)  //   8219
#define CENTERROR_FILE_NOT_FOUND        	    CENTERROR_SYSTEM(0x0000201C)  //   8220
#define CENTERROR_NOT_IMPLEMENTED        	    CENTERROR_SYSTEM(0x0000201D)  //   8221
#define CENTERROR_INVALID_DIRECTORY             CENTERROR_SYSTEM(0x0000201E)  //   8222
#define CENTERROR_INCOMPATIBLE_LIBRARY          CENTERROR_SYSTEM(0x0000201F)  //   8223
#define CENTERROR_LSASS_NOT_RUNNING             CENTERROR_SYSTEM(0x00002020)  //   8224


#define CENTERROR_GP_SYSTEM_CALL_FAILED         CENTERROR_GP(0x00000001)      // 327681
#define CENTERROR_GP_SYSTEM_RESULT_UNEXPECTED   CENTERROR_GP(0x00000002)      // 327682
#define CENTERROR_GP_BAD_GPT_VERSION_NUMBER     CENTERROR_GP(0x00000003)      // 327683
#define CENTERROR_GP_NO_PARENT_DN               CENTERROR_GP(0x00000004)      // 327684
#define CENTERROR_GP_INVALID_GPLINK             CENTERROR_GP(0x00000005)      // 327685
#define CENTERROR_GP_FILE_COPY_FAILED           CENTERROR_GP(0x00000006)      // 327686
#define CENTERROR_GP_LDAP_ERROR                 CENTERROR_GP(0x00000007)      // 327687
#define CENTERROR_GP_LDAP_GETDN_FAILED          CENTERROR_GP(0x00000008)      // 327688
#define CENTERROR_GP_LDAP_NO_SUCH_POLICY        CENTERROR_GP(0x00000009)      // 327689
#define CENTERROR_GP_LDAP_MULTIPLE_POLICIES     CENTERROR_GP(0x0000000A)      // 327690
#define CENTERROR_GP_LDAP_NO_SUCH_GUID          CENTERROR_GP(0x0000000B)      // 327691
#define CENTERROR_GP_LDAP_FOUND_MULTIPLE_GUIDS  CENTERROR_GP(0x0000000C)      // 327692
#define CENTERROR_GP_LDAP_NO_VALUE_FOUND        CENTERROR_GP(0x0000000D)      // 327693
#define CENTERROR_GP_KRB5_CALL_FAILED           CENTERROR_GP(0x0000000E)      // 327694
#define CENTERROR_GP_GSS_CALL_FAILED            CENTERROR_GP(0x0000000F)      // 327695
#define CENTERROR_GP_REFRESH_FAILED             CENTERROR_GP(0x00000010)      // 327696
#define CENTERROR_GP_REGPOL_UNEXPECTED_TOKEN    CENTERROR_GP(0x00000011)      // 327697
#define CENTERROR_GP_REGPOL_NOT_ENOUGH_DATA     CENTERROR_GP(0x00000012)      // 327698
#define CENTERROR_GP_REGPOL_EOF                 CENTERROR_GP(0x00000013)      // 327699
#define CENTERROR_GP_REGPOL_INVALID_SIGNATURE   CENTERROR_GP(0x00000014)      // 327700
#define CENTERROR_GP_PROCESS_NOT_FOUND          CENTERROR_GP(0x00000015)      // 327701
#define CENTERROR_GP_XMLPARSER_CREATE_FAILED    CENTERROR_GP(0x00000016)      // 327702
#define CENTERROR_GP_XMLPARSE_FAILED            CENTERROR_GP(0x00000017)      // 327703
#define CENTERROR_GP_XMLPARSE_BAD_STATE         CENTERROR_GP(0x00000018)      // 327704
#define CENTERROR_GP_XMLPARSE_BAD_SCHEMA        CENTERROR_GP(0x00000019)      // 327705
#define CENTERROR_GP_XMLPARSE_BAD_DATA          CENTERROR_GP(0x0000001A)      // 327706
#define CENTERROR_GP_XPATH_CONTEXT_INIT_ERR     CENTERROR_GP(0x0000001B)      // 327707
#define CENTERROR_GP_XPATH_BAD_EXPRESSION     	CENTERROR_GP(0x0000001C)      // 327708
#define CENTERROR_GP_LWIDATA_NOT_INITIALIZED    CENTERROR_GP(0x0000001D)      // 327709
#define CENTERROR_GP_XML_FAILED_TO_COPY_NODE    CENTERROR_GP(0x0000001E)      // 327710
#define CENTERROR_GP_XML_FAILED_TO_ADD_NODE     CENTERROR_GP(0x0000001F)      // 327711
#define CENTERROR_GP_FILE_OPEN_FAILED           CENTERROR_GP(0x00000020)      // 327712
#define CENTERROR_GP_XML_FAILED_TO_ADD_PROP     CENTERROR_GP(0x00000021)      // 327713
#define CENTERROR_GP_FILE_PARSE_ERROR           CENTERROR_GP(0x00000022)      // 327714
#define CENTERROR_GP_XML_PROPERTY_NOT_FOUND     CENTERROR_GP(0x00000023)      // 327715
#define CENTERROR_GP_XML_NO_NODE_TEXT           CENTERROR_GP(0x00000024)      // 327716
#define CENTERROR_GP_XML_TYPE_MISMATCH          CENTERROR_GP(0x00000025)      // 327717
#define CENTERROR_GP_XML_GPITEM_NOT_FOUND       CENTERROR_GP(0x00000026)      // 327718
#define CENTERROR_GP_XML_NODE_NOT_FOUND         CENTERROR_GP(0x00000027)      // 327719
#define CENTERROR_GP_GNOME_UNKNOWN_SETTING      CENTERROR_GP(0x00000028)      // 327720
#define CENTERROR_GP_GNOME_UNKNOWN_TYPE         CENTERROR_GP(0x00000029)      // 327721
#define CENTERROR_GP_XML_FAILED_TO_CREATE_DOC   CENTERROR_GP(0x0000002A)      // 327722
#define CENTERROR_GP_XML_FAILED_TO_WRITE_DOC    CENTERROR_GP(0x0000002B)      // 327723
#define CENTERROR_GP_XML_FAILED_TO_CREATE_NODE  CENTERROR_GP(0x0000002C)      // 327724
#define CENTERROR_GP_XML_FAILED_TO_CREATE_CDATA CENTERROR_GP(0x0000002D)      // 327725
#define CENTERROR_GP_XML_FAILED_TO_SET_ATTR     CENTERROR_GP(0x0000002E)      // 327726
#define CENTERROR_GP_UNEXPECTED_ACTION_TYPE     CENTERROR_GP(0x0000002F)      // 327727
#define CENTERROR_GP_FAILED_TO_GET_MAC_SETTING  CENTERROR_GP(0x00000030)      // 327728
#define CENTERROR_GP_FAILED_TO_SET_MAC_SETTING  CENTERROR_GP(0x00000031)      // 327729
#define CENTERROR_GP_LOGIN_POLICY_FAILED        CENTERROR_GP(0x00000032)      // 327730
#define CENTERROR_GP_LOGOUT_POLICY_FAILED       CENTERROR_GP(0x00000033)      // 327731
#define CENTERROR_GP_NOT_AD_USER                CENTERROR_GP(0x00000034)      // 327732
#define CENTERROR_GP_FAILED_TO_GET_DOMAIN_SID   CENTERROR_GP(0x00000035)      // 327733
#define CENTERROR_GP_NO_DNS_RECORDS             CENTERROR_GP(0x00000036)      // 327734
#define CENTERROR_GP_NO_SMB_KRB5_INFO           CENTERROR_GP(0x00000037)      // 327735
#define CENTERROR_GP_NO_SMB_KRB5_SITE_INFO      CENTERROR_GP(0x00000038)      // 327736
#define CENTERROR_GP_NO_SMB_KRB5_SITE_KDC_INFO  CENTERROR_GP(0x00000039)      // 327737
#define CENTERROR_GP_GETHOSTNAME_FAILED         CENTERROR_GP(0x00000040)      // 327738
#define CENTERROR_GP_GETHOSTBYNAME_FAILED       CENTERROR_GP(0x00000041)      // 327739
#define CENTERROR_GP_GETHOSTBYADDR_FAILED       CENTERROR_GP(0x00000042)      // 327740
#define CENTERROR_GP_DNS_MESSAGE_REPLY_TOO_SMALL CENTERROR_GP(0x00000043)      // 327741
#define CENTERROR_GP_DNS_COULD_NOT_RESOLVE_NAME CENTERROR_GP(0x00000044)      // 327742
#define CENTERROR_GP_DNS_HOSTENT_NAME_INVALID   CENTERROR_GP(0x00000045)      // 327743
#define CENTERROR_GP_DNS_QUERY_FAILED           CENTERROR_GP(0x00000046)      // 327744
#define CENTERROR_GP_EVENT_LOG_NOT_AVAILABLE    CENTERROR_GP(0x00000047)      // 327745
#define CENTERROR_GP_NO_SUCH_PROVIDER           CENTERROR_GP(0x00000048)      // 327746
#define CENTERROR_GP_STRING_CONVERSION_FAILED   CENTERROR_GP(0x00000049)      // 327747
#define CENTERROR_GP_CREATE_FAILED              CENTERROR_GP(0x0000004A)      // 327748
#define CENTERROR_GP_WRITE_FAILED               CENTERROR_GP(0x0000004B)      // 327749
#define CENTERROR_GP_READ_FAILED                CENTERROR_GP(0x0000004C)      // 327750
#define CENTERROR_GP_QUERY_DIRECTORY            CENTERROR_GP(0x0000004D)      // 327751
#define CENTERROR_GP_PATH_NOT_FOUND             CENTERROR_GP(0x0000004E)      // 327752
#define CENTERROR_GP_SETLOGLEVEL_FAILED         CENTERROR_GP(0x0000004F)      // 327753

#define CENTERROR_REGISTRY_OPEN_KEYDB_FAILED     CENTERROR_REGISTRY(0x00000001) // 458753
#define CENTERROR_REGISTRY_NO_SUCH_KEY           CENTERROR_REGISTRY(0x00000002) // 458754
#define CENTERROR_REGISTRY_GET_CHILD_KEYS_FAILED CENTERROR_REGISTRY(0x00000003) // 458755
#define CENTERROR_REGISTRY_REMOVE_KEY_FAILED     CENTERROR_REGISTRY(0x00000004) // 458756
#define CENTERROR_REGISTRY_REMOVE_VALUE_FAILED   CENTERROR_REGISTRY(0x00000005) // 458757
#define CENTERROR_REGISTRY_UNSUPPORTED_TYPE      CENTERROR_REGISTRY(0x00000006) // 458758
#define CENTERROR_REGISTRY_UNEXPECTED_VALUE_TYPE CENTERROR_REGISTRY(0x00000007) // 458759
#define CENTERROR_REGISTRY_ADD_KEY_FAILED        CENTERROR_REGISTRY(0x00000008) // 458760
#define CENTERROR_REGISTRY_GET_KEYPATHLEN_FAILED CENTERROR_REGISTRY(0x00000009) // 458761
#define CENTERROR_REGISTRY_GET_KEYPATH_FAILED    CENTERROR_REGISTRY(0x0000000A) // 458762
#define CENTERROR_REGISTRY_ROOT_UNDEFINED        CENTERROR_REGISTRY(0x0000000B) // 458763
#define CENTERROR_REGISTRY_INVALID_KDB_HANDLE    CENTERROR_REGISTRY(0x0000000C) // 458764
#define CENTERROR_REGISTRY_INVALID_DATA_TYPE     CENTERROR_REGISTRY(0x0000000D) // 458765
#define CENTERROR_REGISTRY_INVALID_DATA_SIZE     CENTERROR_REGISTRY(0x0000000E) // 458766

#define CENTERROR_DOMAINJOIN_NON_ROOT_USER         CENTERROR_DOMAINJOIN(0x00000001) // 524289
#define CENTERROR_DOMAINJOIN_INVALID_HOSTNAME      CENTERROR_DOMAINJOIN(0x00000002) // 524290
#define CENTERROR_DOMAINJOIN_SYSCONFIG_EDIT_FAIL   CENTERROR_DOMAINJOIN(0x00000003) // 524291
#define CENTERROR_DOMAINJOIN_NO_ETH_ITF_CFG_FILE   CENTERROR_DOMAINJOIN(0x00000004) // 524292
#define CENTERROR_DOMAINJOIN_DHCPINFO_EDIT_FAIL    CENTERROR_DOMAINJOIN(0x00000005) // 524293
#define CENTERROR_DOMAINJOIN_HOSTS_EDIT_FAIL       CENTERROR_DOMAINJOIN(0x00000006) // 524294
#define CENTERROR_DOMAINJOIN_SET_MACHINESID_FAIL   CENTERROR_DOMAINJOIN(0x00000007) // 524295
#define CENTERROR_DOMAINJOIN_NETCONFIGCMD_FAIL     CENTERROR_DOMAINJOIN(0x00000008) // 524296
#define CENTERROR_DOMAINJOIN_DHCPRESTART_SET_FAIL  CENTERROR_DOMAINJOIN(0x00000009) // 524297
#define CENTERROR_DOMAINJOIN_DHCPRESTART_FAIL      CENTERROR_DOMAINJOIN(0x0000000A) // 524298
#define CENTERROR_DOMAINJOIN_KRB5_EDIT_FAIL        CENTERROR_DOMAINJOIN(0x0000000B) // 524299
#define CENTERROR_DOMAINJOIN_NSSWITCH_EDIT_FAIL    CENTERROR_DOMAINJOIN(0x0000000C) // 524300
#define CENTERROR_DOMAINJOIN_PAM_EDIT_FAIL         CENTERROR_DOMAINJOIN(0x0000000D) // 524301
#define CENTERROR_DOMAINJOIN_JOIN_FAILED           CENTERROR_DOMAINJOIN(0x0000000E) // 524302
#define CENTERROR_DOMAINJOIN_JOIN_TIMEDOUT         CENTERROR_DOMAINJOIN(0x0000000F) // 524303
#define CENTERROR_DOMAINJOIN_JOIN_NO_WKGRP         CENTERROR_DOMAINJOIN(0x00000010) // 524304
#define CENTERROR_DOMAINJOIN_SMB_VALUE_NOT_FOUND   CENTERROR_DOMAINJOIN(0x00000011) // 524305
#define CENTERROR_DOMAINJOIN_DOMAIN_NOT_FOUND      CENTERROR_DOMAINJOIN(0x00000012) // 524306
#define CENTERROR_DOMAINJOIN_WORKGROUP_NOT_FOUND   CENTERROR_DOMAINJOIN(0x00000013) // 524307
#define CENTERROR_DOMAINJOIN_DESCRIPTION_NOT_FOUND CENTERROR_DOMAINJOIN(0x00000014) // 524308
#define CENTERROR_DOMAINJOIN_HOSTNAME_CONTAINS_DOT CENTERROR_DOMAINJOIN(0x00000015) // 524309
#define CENTERROR_DOMAINJOIN_MISSING_DAEMON        CENTERROR_DOMAINJOIN(0x00000016) // 524310
#define CENTERROR_DOMAINJOIN_UNEXPECTED_ERRCODE    CENTERROR_DOMAINJOIN(0x00000017) // 524311
#define CENTERROR_DOMAINJOIN_INCORRECT_STATUS      CENTERROR_DOMAINJOIN(0x00000018) // 524312
#define CENTERROR_DOMAINJOIN_CHKCONFIG_FAILED      CENTERROR_DOMAINJOIN(0x00000019) // 524313
#define CENTERROR_DOMAINJOIN_UPDATERCD_FAILED      CENTERROR_DOMAINJOIN(0x0000001A) // 524314
#define CENTERROR_DOMAINJOIN_LICENSE_ERROR         CENTERROR_DOMAINJOIN(0x0000001B) // 524315
#define CENTERROR_DOMAINJOIN_NO_LICENSE_KEY        CENTERROR_DOMAINJOIN(0x0000001C) // 524316
#define CENTERROR_DOMAINJOIN_WRONG_PRODUCT_CODE    CENTERROR_DOMAINJOIN(0x0000001D) // 524317
#define CENTERROR_DOMAINJOIN_WRONG_LICENSE_VERSION CENTERROR_DOMAINJOIN(0x0000001E) // 524318
#define CENTERROR_DOMAINJOIN_NOT_JOINED_TO_AD      CENTERROR_DOMAINJOIN(0x0000001F) // 524319
#define CENTERROR_DOMAINJOIN_GPAGENTD_INCOMMUNICADO CENTERROR_DOMAINJOIN(0x00000020) // 524320
#define CENTERROR_DOMAINJOIN_BAD_LICENSE_KEY        CENTERROR_DOMAINJOIN(0x00000021) // 524321
#define CENTERROR_DOMAINJOIN_INVALID_USERID         CENTERROR_DOMAINJOIN(0x00000022) // 524322
#define CENTERROR_DOMAINJOIN_INVALID_DOMAIN_NAME    CENTERROR_DOMAINJOIN(0x00000023) // 524323
#define CENTERROR_DOMAINJOIN_INVALID_PASSWORD       CENTERROR_DOMAINJOIN(0x00000024) // 524324
#define CENTERROR_DOMAINJOIN_FAILED_TO_LEAVE_DOMAIN CENTERROR_DOMAINJOIN(0x00000025) // 524325
#define CENTERROR_DOMAINJOIN_UNRESOLVED_DOMAIN_NAME CENTERROR_DOMAINJOIN(0x00000026) // 524326
#define CENTERROR_DOMAINJOIN_INVALID_LOG_LEVEL      CENTERROR_DOMAINJOIN(0x00000027) // 524327
#define CENTERROR_DOMAINJOIN_FAILED_SET_HOSTNAME    CENTERROR_DOMAINJOIN(0x00000028) // 524328
#define CENTERROR_DOMAINJOIN_LISTSVCS_FAILED        CENTERROR_DOMAINJOIN(0x00000029) // 524329
#define CENTERROR_DOMAINJOIN_SVC_LOAD_FAILED        CENTERROR_DOMAINJOIN(0x0000002A) // 524330
#define CENTERROR_DOMAINJOIN_SVC_UNLOAD_FAILED      CENTERROR_DOMAINJOIN(0x0000002B) // 524331
#define CENTERROR_DOMAINJOIN_PREPSVC_FAILED         CENTERROR_DOMAINJOIN(0x0000002C) // 524332
#define CENTERROR_DOMAINJOIN_UNKNOWN_DAEMON         CENTERROR_DOMAINJOIN(0x0000002D) // 524333
#define CENTERROR_DOMAINJOIN_INVALID_OU             CENTERROR_DOMAINJOIN(0x0000002E) // 524334
#define CENTERROR_DOMAINJOIN_FAILED_KICKER_INSTALL   CENTERROR_DOMAINJOIN(0x0000002F) // 524335
#define CENTERROR_DOMAINJOIN_FAILED_KICKER_UNINSTALL CENTERROR_DOMAINJOIN(0x00000030) // 524336
#define CENTERROR_DOMAINJOIN_FAILED_SET_SEARCHPATH CENTERROR_DOMAINJOIN(0x00000031) // 524337
#define CENTERROR_DOMAINJOIN_FAILED_REG_OPENDIR    CENTERROR_DOMAINJOIN(0x00000032) // 524338
#define CENTERROR_DOMAINJOIN_FAILED_UNREG_OPENDIR  CENTERROR_DOMAINJOIN(0x00000033) // 524339
#define CENTERROR_DOMAINJOIN_PAM_MISSING_SERVICE   CENTERROR_DOMAINJOIN(0x00000034) // 524340
#define CENTERROR_DOMAINJOIN_PAM_BAD_CONF          CENTERROR_DOMAINJOIN(0x00000035) // 524341
#define CENTERROR_DOMAINJOIN_PAM_MISSING_MODULE    CENTERROR_DOMAINJOIN(0x00000036) // 524342
#define CENTERROR_DOMAINJOIN_FAILED_ADMIN_PRIVS    CENTERROR_DOMAINJOIN(0x00000037) // 524343
#define CENTERROR_DOMAINJOIN_LWINET_TIME_FAILED    CENTERROR_DOMAINJOIN(0x00000038) // 524344
#define CENTERROR_DOMAINJOIN_TIME_NOT_SET	   CENTERROR_DOMAINJOIN(0x00000039) // 524345
#define CENTERROR_DOMAINJOIN_INVALID_FORMAT	   CENTERROR_DOMAINJOIN(0x00000040) // 524346
#define CENTERROR_DOMAINJOIN_INVALID_FIREWALLCFG   CENTERROR_DOMAINJOIN(0x00000041) // 524347
#define CENTERROR_DOMAINJOIN_MODULE_NOT_CONFIGURED CENTERROR_DOMAINJOIN(0x00000042) // 524348
#define CENTERROR_DOMAINJOIN_MODULE_NOT_ENABLED    CENTERROR_DOMAINJOIN(0x00000043) // 524349
#define CENTERROR_DOMAINJOIN_MODULE_ALREADY_DONE   CENTERROR_DOMAINJOIN(0x00000044) // 524350
#define CENTERROR_DOMAINJOIN_SHOW_USAGE            CENTERROR_DOMAINJOIN(0x00000045) // 524351
#define CENTERROR_DOMAINJOIN_WARNING            CENTERROR_DOMAINJOIN(0x00000046) // 524352
#define CENTERROR_DOMAINJOIN_LSASS_ERROR           CENTERROR_DOMAINJOIN(0x00000047) // 524353

const char*
CTErrorName(
    CENTERROR error
    );

const char*
CTErrorDescription(
    CENTERROR error
    );

const char*
CTErrorHelp(
    CENTERROR error
    );

CENTERROR
CTErrorFromName(
    const char* name
    );

CENTERROR
CTMapSystemError(
    int dwError
    );

#endif
