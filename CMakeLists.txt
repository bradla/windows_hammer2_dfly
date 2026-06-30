cmake_minimum_required(VERSION 3.16)
project(hammer2_windows_port LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

#
# Core HAMMER2 sources that compile cleanly against the Windows shim layer
# (hammer2_windows_port.h / hammer2_compat.h / hammer2_os_windows.h /
# hammer2_rbtree_windows.h + the winshim/ stub kernel headers).
#
# 23 of the 24 original source files build under MSVC.  hammer2_linux_vfs.c is
# excluded -- it was the Linux-kernel VFS binding and is replaced on Windows by
# the Dokan-based driver in hammer2_windows_vfs.c (see the hammer2_mount target).
#
set(HAMMER2_CORE_SOURCES
    hammer2_ccms.c
    hammer2_thread.c
    hammer2_synchro.c
    kern_dmsg.c
    hammer2_iocom.c
    hammer2_msgops.c
    hammer2_cluster.c
    hammer2_admin.c
    hammer2_subr.c
    hammer2_lz4.c
    hammer2_freemap.c
    hammer2_chain.c
    hammer2_xops.c
    hammer2_flush.c
    hammer2_bulkfree.c
    hammer2_io.c
    hammer2_ondisk.c
    hammer2_ioctl.c
    hammer2_strategy.c
    hammer2_inode.c
    hammer2_vfsops.c
    hammer2_init.c
    hammer2_vnops.c
    hammer2_device_windows.c
    hammer2_kthread_windows.c
)

if(MSVC)
    add_library(hammer2_core STATIC ${HAMMER2_CORE_SOURCES})
    target_include_directories(hammer2_core PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_definitions(hammer2_core PRIVATE _WIN32 KLD_MODULE
        _CRT_SECURE_NO_WARNINGS)
    # /std:clatest -> C23 typeof() used by the BSD TAILQ/LIST macros.
    # /Zc:preprocessor -> conforming preprocessor for , ##__VA_ARGS__.
    target_compile_options(hammer2_core PRIVATE /std:clatest /Zc:preprocessor /W3)

    #
    # Native Windows filesystem driver: mounts a HAMMER2 volume through the core
    # and exposes it as a drive letter via Dokan.  Requires the Dokan 1.x SDK.
    #
    # NOTE: the architecture follows the toolchain in your shell -- build from an
    # x64 Native Tools prompt (vcvars64) for a 64-bit driver.  An x86 toolchain
    # produces a 32-bit binary and needs the x86 Dokan import library, which the
    # hints below select automatically by pointer size.
    #
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        message(WARNING "Building hammer2_mount as 32-bit (x86). "
                        "For a 64-bit driver, configure from an x64 Native Tools "
                        "prompt and delete the build directory first.")
        set(_dokan_arch_sub "x86/")
    else()
        set(_dokan_arch_sub "")
    endif()

    find_path(DOKAN_INCLUDE_DIR dokan/dokan.h
        HINTS "$ENV{DokanLibrary1}/include"
              "C:/Program Files/Dokan/Dokan Library-1.4.0/include")
    find_library(DOKAN_LIBRARY NAMES dokan1
        HINTS "$ENV{DokanLibrary1}/${_dokan_arch_sub}lib"
              "C:/Program Files/Dokan/Dokan Library-1.4.0/${_dokan_arch_sub}lib")

    if(DOKAN_INCLUDE_DIR AND DOKAN_LIBRARY)
        add_executable(hammer2_mount hammer2_windows_vfs.c)
        target_include_directories(hammer2_mount PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR} ${DOKAN_INCLUDE_DIR})
        target_compile_definitions(hammer2_mount PRIVATE _WIN32 KLD_MODULE
            _CRT_SECURE_NO_WARNINGS)
        target_compile_options(hammer2_mount PRIVATE /std:clatest /Zc:preprocessor /W3)
        target_link_libraries(hammer2_mount PRIVATE hammer2_core ${DOKAN_LIBRARY})
    else()
        message(WARNING "Dokan SDK not found; skipping hammer2_mount target. "
                        "Install Dokan 1.x or set DokanLibrary1.")
    endif()
endif()

#
# newfs_hammer2 — offline mkfs tool.  Operates directly on an image file or
# device (no kernel / mount), so it builds from the upstream DragonFly sources
# in lh1/src/sbin/newfs_hammer2 plus a small userland compat layer.
#
if(MSVC)
    add_executable(newfs_hammer2
        lh1/src/sbin/newfs_hammer2/newfs_hammer2.c
        lh1/src/sbin/newfs_hammer2/mkfs_hammer2.c
        lh1/src/sbin/hammer2/ondisk.c
        hammer2_user_subs.c)
    target_include_directories(newfs_hammer2 PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/usershim
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/lh1/src/sbin/hammer2
        ${CMAKE_CURRENT_SOURCE_DIR}/lh1/src/sbin)	# resolves ../lib, ../sys includes
    target_compile_definitions(newfs_hammer2 PRIVATE
        _WIN32 HAMMER2_USERLAND _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS)
    target_compile_options(newfs_hammer2 PRIVATE /std:clatest /Zc:preprocessor /W3)
    target_link_libraries(newfs_hammer2 PRIVATE ole32)
endif()

#
# fsck_hammer2 — offline filesystem checker.  Like newfs it operates directly on
# the image/device and reuses the userland compat layer; additionally it needs a
# SHA256 (sha256_user.c, replacing -lcrypto) and the BSD <sys/tree.h>/<sys/queue.h>
# from lh1/src/sys.
#
if(MSVC)
    add_executable(fsck_hammer2
        lh1/src/sbin/fsck_hammer2/fsck_hammer2.c
        lh1/src/sbin/fsck_hammer2/test.c
        lh1/src/sbin/hammer2/ondisk.c
        hammer2_user_subs.c
        sha256_user.c)
    target_include_directories(fsck_hammer2 PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/usershim
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/lh1/src/sbin/hammer2
        ${CMAKE_CURRENT_SOURCE_DIR}/lh1/src/sbin
        ${CMAKE_CURRENT_SOURCE_DIR}/lh1/src/sys)	# <sys/tree.h>, <sys/queue.h>
    target_compile_definitions(fsck_hammer2 PRIVATE
        _WIN32 HAMMER2_USERLAND _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS)
    target_compile_options(fsck_hammer2 PRIVATE /std:clatest /Zc:preprocessor /W3)
    target_link_libraries(fsck_hammer2 PRIVATE ole32)
endif()

#
# hammer2 (offline subset) — the offline debug subcommands (show / freemap /
# volume-list / hash / dhash) from the upstream hammer2(8) CLI, driven by a
# trimmed Windows dispatcher (hammer2_cli_windows.c) instead of the full main.c
# (which pulls in the networked service / RSA / ioctl commands).
#
if(MSVC)
    add_executable(hammer2_cli
        hammer2_cli_windows.c
        lh1/src/sbin/hammer2/ondisk.c
        hammer2_user_subs.c
        sha256_user.c)
    set_target_properties(hammer2_cli PROPERTIES OUTPUT_NAME hammer2)
    target_include_directories(hammer2_cli PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/usershim
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/lh1/src/sbin/hammer2
        ${CMAKE_CURRENT_SOURCE_DIR}/lh1/src/sbin
        ${CMAKE_CURRENT_SOURCE_DIR}/lh1/src/sys)
    target_compile_definitions(hammer2_cli PRIVATE
        _WIN32 HAMMER2_USERLAND _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS)
    target_compile_options(hammer2_cli PRIVATE /std:clatest /Zc:preprocessor /W3)
    target_link_libraries(hammer2_cli PRIVATE ole32)
endif()
