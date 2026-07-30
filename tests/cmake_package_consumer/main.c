#include "smave/c_api.h"

#include <stdio.h>

int main(void) {
    smave_library* library = NULL;
    if (smave_abi_version() != SMAVE_ABI_VERSION ||
        smave_library_create(NULL, &library) != SMAVE_STATUS_OK ||
        smave_library_destroy(library) != SMAVE_STATUS_OK) return 1;
    printf("SMAVE_CMAKE_PACKAGE_CONSUMER 1\n"
           "SMAVE_IMPORTED_TARGET 1\n"
           "SMAVE_RELOCATABLE_CONFIG 1\n");
    return 0;
}
