include(FetchContent)

# Header-only / small libraries via FetchContent
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.15.2
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG 0.8.0
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.18.3
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    sqlite
    GIT_REPOSITORY https://github.com/nicktrav/sqlite-amalgamation.git
    GIT_TAG 3.46.0
    GIT_SHALLOW TRUE
)

# Google Test (only when tests enabled)
if(BUILD_TESTS)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.15.2
        GIT_SHALLOW TRUE
    )
endif()

# Download all at configure time
message(STATUS "Fetching dependencies...")
FetchContent_MakeAvailable(nlohmann_json spdlog yaml-cpp httplib)

if(BUILD_TESTS)
    FetchContent_MakeAvailable(googletest)
endif()

# SQLite is a single C file - create a library manually
FetchContent_GetProperties(sqlite)
if(NOT sqlite_POPULATED)
    FetchContent_Populate(sqlite)
endif()
add_library(sqlite3 STATIC ${sqlite_SOURCE_DIR}/sqlite3.c)
target_include_directories(sqlite3 PUBLIC ${sqlite_SOURCE_DIR})
target_compile_definitions(sqlite3 PRIVATE
    SQLITE_THREADSAFE=2
    SQLITE_DEFAULT_WAL_MODE=1
)

message(STATUS "Dependencies fetched successfully")