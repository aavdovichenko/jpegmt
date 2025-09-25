LIB_DIR=$${ROOT_DIR}/lib

exists(LocalLibs.pri) {
    include(LocalLibs.pri)
}

!defined(HELPER_LIB_DIR, var) {
    HELPER_LIB_DIR=$${LIB_DIR}/Helper
}
