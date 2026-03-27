include_guard()

CPMAddPackage(
    NAME googletest
    GITHUB_REPOSITORY google/googletest
    GIT_TAG v1.17.0
    OPTIONS "INSTALL_GTEST OFF"
    SYSTEM
    EXCLUDE_FROM_ALL
)
