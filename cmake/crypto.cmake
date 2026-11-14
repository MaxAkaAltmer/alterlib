#=============================================================================
# alterlib/cmake/crypto.cmake
#
# Подключение блока crypto (alt::cryptoHash: CRC32 / MD5 / SHA1) в
# пользовательские CMake-проекты.
#
#   set(ALTERLIB_DIR ${CMAKE_SOURCE_DIR}/../alterlib)   # путь к alterlib
#   include(${ALTERLIB_DIR}/cmake/crypto.cmake)
#
#   add_executable(myapp
#       main.cpp
#       ${ALTERLIB_CRYPTO_SOURCES}        # исходники блока - прямо в цель
#       ${ALTERLIB_CRYPTO_HEADERS}        # (заголовки - только для IDE)
#   )
#   target_link_libraries(myapp PRIVATE ${ALTERLIB_CRYPTO_LIBRARIES})
#
#   # main.cpp: #include "crypto/acrypto.h"  ->  alt::cryptoHash
#
# После include() определены:
#   ALTERLIB_CRYPTO_SOURCES     - исходники crypto/*.cpp; компилируются вашей
#                                 целью, отдельных библиотек не появляется
#   ALTERLIB_CRYPTO_HEADERS     - заголовки блока (только для списков IDE)
#   ALTERLIB_CRYPTO_INCLUDE_DIR - корень alterlib, для "crypto/acrypto.h"
#                                 (нужен, только если он не в include-пути проекта)
#   ALTERLIB_CRYPTO_LIBRARIES   - статическая цель Crypto++ (<cryptopp/md5.h>)
#   ALTERLIB_CRYPTO_DEFINITIONS - определения компилятора Crypto++ (например
#                                 CRYPTOPP_DISABLE_ASM); передайте их своей цели
#
# Переменные (до include()): ALTERLIB_DIR - путь к alterlib (по умолчанию
# каталог выше этого файла), ALTERLIB_CRYPTOOPP_DIR - свой каталог Crypto++.
# Артефакты Crypto++ собираются в build-дерево проекта.
#
# Core alterlib (astring.cpp, abyte_array.cpp) в блок не входит и подключается
# проектом самостоятельно - их коды нужны при линковке alt::byteArray.
#=============================================================================

# путь к alterlib; если не задан - каталог выше этого файла
if(NOT DEFINED ALTERLIB_DIR)
    get_filename_component(ALTERLIB_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()
get_filename_component(ALTERLIB_DIR "${ALTERLIB_DIR}" ABSOLUTE)

if(NOT EXISTS "${ALTERLIB_DIR}/crypto/acrypto.cpp")
    message(FATAL_ERROR
            "crypto: в ${ALTERLIB_DIR} нет crypto/acrypto.cpp.\n"
            "Задайте set(ALTERLIB_DIR <путь к alterlib>) перед include(this).")
endif()

#-----------------------------------------------------------------------------
# Crypto++ (вендорный, статический) - единственный внешний блок, который
# приходится собирать отдельно; цель cryptopp_static PUBLIC-ом даёт и
# заголовки <cryptopp/...>, и линковку.
#-----------------------------------------------------------------------------
if(NOT TARGET cryptopp_static AND NOT TARGET cryptopp_shared)
    include(FetchContent)

    if(NOT DEFINED ALTERLIB_CRYPTOOPP_DIR)
        set(ALTERLIB_CRYPTOOPP_DIR "${ALTERLIB_DIR}/external/cryptopp")
    endif()
    get_filename_component(ALTERLIB_CRYPTOOPP_DIR "${ALTERLIB_CRYPTOOPP_DIR}" ABSOLUTE)

    if(NOT EXISTS "${ALTERLIB_CRYPTOOPP_DIR}/CMakeLists.txt")
        message(FATAL_ERROR
                "crypto: не найден Crypto++ в ${ALTERLIB_CRYPTOOPP_DIR}.\n"
                "Скачайте подмодуль external/cryptopp или задайте свой каталог "
                "переменной ALTERLIB_CRYPTOOPP_DIR.")
    endif()

    # только статика: обычного set() недостаточно, т.к. Crypto++ объявляет
    # option(BUILD_SHARED_LIBS) - на время её добавления значение фиксируется
    # в кэше и затем восстанавливается
    set(_alterlib_crypto_shared_libs ${BUILD_SHARED_LIBS})
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)

    FetchContent_Declare(cryptopp SOURCE_DIR ${ALTERLIB_CRYPTOOPP_DIR})
    FetchContent_MakeAvailable(cryptopp)

    if(_alterlib_crypto_shared_libs)
        set(BUILD_SHARED_LIBS ON CACHE BOOL "Build shared libraries" FORCE)
    else()
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)
    endif()
endif()

#-----------------------------------------------------------------------------
# Исходники блока crypto
#-----------------------------------------------------------------------------
file(GLOB ALTERLIB_CRYPTO_SOURCES CONFIGURE_DEPENDS "${ALTERLIB_DIR}/crypto/*.cpp")
file(GLOB ALTERLIB_CRYPTO_HEADERS CONFIGURE_DEPENDS "${ALTERLIB_DIR}/crypto/*.h")

if(NOT ALTERLIB_CRYPTO_SOURCES)
    message(FATAL_ERROR "crypto: исходники не найдены в ${ALTERLIB_DIR}/crypto")
endif()

set(ALTERLIB_CRYPTO_INCLUDE_DIR "${ALTERLIB_DIR}")

if(TARGET cryptopp_static)
    set(ALTERLIB_CRYPTO_LIBRARIES cryptopp_static)
else()
    set(ALTERLIB_CRYPTO_LIBRARIES cryptopp_shared)
endif()
get_target_property(ALTERLIB_CRYPTO_DEFINITIONS ${ALTERLIB_CRYPTO_LIBRARIES}
                    INTERFACE_COMPILE_DEFINITIONS)
if(NOT ALTERLIB_CRYPTO_DEFINITIONS)
    set(ALTERLIB_CRYPTO_DEFINITIONS "")
endif()

message(STATUS "crypto: ${ALTERLIB_CRYPTO_SOURCES} + ${ALTERLIB_CRYPTO_LIBRARIES}")
