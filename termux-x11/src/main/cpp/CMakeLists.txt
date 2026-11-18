cmake_minimum_required(VERSION 3.22)
project(lorie)

set(can_use_assembler TRUE)
enable_language(ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE RelWithDebInfo)

include(TestBigEndian)
include(CheckIncludeFile)
include(CheckSourceCompiles)
include(CheckCCompilerFlag)
include(CheckSymbolExists)
include(CheckFunctionExists)
include(CheckTypeSize)

find_package(Python3 REQUIRED)
find_package(BISON REQUIRED)

function(target_apply_patch patch_target d p)
    execute_process(COMMAND "bash" "-c" "patch -p1 -d ${d} -i ${p} -R --dry-run > /dev/null 2>&1 || patch -p1 -N -d ${d} -i ${p} -r -")
endfunction()

set(test_compile_options
        "-Wall"
        "-Wpointer-arith"
        "-Wmissing-declarations"
        "-Wformat=2"
        "-Winvalid-pch"
        "-Wstrict-prototypes"
        "-Wmissing-prototypes"
        "-Wnested-externs"
        "-Wbad-function-cast"
        "-Wold-style-definition"
        "-Wunused"
        "-Wuninitialized"
        "-Wshadow"
        "-Wmissing-noreturn"
        "-Wmissing-format-attribute"
        "-Wredundant-decls"
        "-Wlogical-op"
        "-Werror=implicit"
        "-Werror=nonnull"
        "-Werror=init-self"
        "-Werror=main"
        "-Werror=missing-braces"
        "-Werror=sequence-point"
        "-Werror=return-type"
        "-Werror=trigraphs"
        "-Werror=array-bounds"
        "-Werror=write-strings"
        "-Werror=address"
        "-Werror=int-to-pointer-cast"
        "-Werror=pointer-to-int-cast"
        "-fno-strict-aliasing"
        "-Wno-unused-but-set-variable"
        "-Wno-unused-parameter"
        "-Wno-unused-variable"
        "-Wno-unused-function"
        "-Wno-missing-prototypes"
        "-Wno-macro-redefined"
        "-Wno-uninitialized"
        "-Wno-shadow")

set(CMAKE_REQUIRED_QUIET_OLD ${CMAKE_REQUIRED_QUIET})
set(CMAKE_REQUIRED_QUIET 1)
set(common_compile_options)
set(I 0)
foreach (option ${test_compile_options})
    check_c_compiler_flag(${option} COMPILER_TEST_${I})
    if (COMPILER_TEST_${I})
        set(common_compile_options ${common_compile_options} ${option})
    endif()
    MATH(EXPR I "${I}+1")
endforeach ()
set(CMAKE_REQUIRED_QUIET ${CMAKE_REQUIRED_QUIET_OLD})

foreach(project xorgproto fontenc md pixman tirpc xshmfence Xdmcp Xau Xfont2 xkbcomp xserver)
    include(recipes/${project}.cmake)
endforeach()


# ---------------------------------------------------------------------------
# A4K 画质增强模块（注入式适配层）
#
# 说明与接入现状见 a4k/README_INTEGRATION.md。
# 输出 libA4KIntegrated.so，由 com.a4k.integrated.Bridge 通过 System.loadLibrary 加载，
# 在 LorieView 的静态初始化里 boot()；用 PLT 钩子接管 libXlorie.so 的
# glDrawArrays / eglSwapBuffers，在 eglSwapBuffers（X 服务器锁之外）执行增强。
# ---------------------------------------------------------------------------
add_library(A4KIntegrated SHARED
        a4k/adapter.cpp
        a4k/bridge.cpp
        a4k/hook.cpp
        lorie/a4k/pipeline.cpp
        lorie/a4k/fsr_constants.cpp)
target_include_directories(A4KIntegrated PRIVATE a4k lorie lorie/a4k)
target_link_libraries(A4KIntegrated PRIVATE EGL GLESv3 log android dl)
target_link_options(A4KIntegrated PRIVATE
        -Wl,-z,max-page-size=16384
        -Wl,-z,common-page-size=16384)

