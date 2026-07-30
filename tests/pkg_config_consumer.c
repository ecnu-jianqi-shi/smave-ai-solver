#include "smave/c_api.h"

#include <stdio.h>

int main(void) {
    smave_library* library = NULL;
    if (smave_abi_version() != SMAVE_ABI_VERSION ||
        smave_library_create(NULL, &library) != SMAVE_STATUS_OK ||
        smave_library_destroy(library) != SMAVE_STATUS_OK) return 1;
    printf("SMAVE_PKG_CONFIG_CONSUMER 1\n"
           "SMAVE_PKG_CONFIG_CFLAGS_LIBS 1\n"
           "SMAVE_PKG_CONFIG_RELOCATABLE_PREFIX 1\n");
    return 0;
}
