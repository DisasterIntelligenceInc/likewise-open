/* Editor Settings: expandtabs and use 4 spaces for indentation
* ex: set softtabstop=4 tabstop=8 expandtab shiftwidth=4: *
* -*- mode: c, c-basic-offset: 4 -*- */

#include "domainjoin.h"

int
main(int argc, char* argv[])
{
    CENTERROR ceError = CENTERROR_SUCCESS;

    if (argc < 3) {
       printf("Usage: test_dhcpname_change <file path> <dhcp name>");
       exit(1);
    }
 
    ceError = DJFixDHCPHost(argv[1], argv[2]);
    BAIL_ON_CENTERIS_ERROR(ceError);
 
error:

    return(ceError);
}
