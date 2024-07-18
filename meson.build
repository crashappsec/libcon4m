project(
    'libcon4m',
    'c',
    default_options: ['c_std=c2x', 'default_library=static'],
    version: '0.1.0',
    license: 'MIT',
)


cc = meson.get_compiler('c')
incdir = include_directories('include')
using_osx = false

glibc_check = '''
#include <stdio.h>
#ifndef __GLIBC__
#error NO GLIBC
#endif
'''
if cc.compiles(glibc_check)
    using_glibc = true
else
    using_glibc = false
endif

render_width = get_option('minimum_render_width').to_string()

c_args = [
    '-Wextra',
    '-g',
    '-Wno-unused-parameter',
    '-Wno-cast-function-type',
    '-fno-omit-frame-pointer',
    '-DHATRACK_PER_INSTANCE_AUX',
    '-DC4M_MIN_RENDER_WIDTH=' + render_width,
]

if (host_machine.cpu_family() == 'x86_64' and cc.get_id() == 'clang')
    c_args = c_args + ['-Wno-atomic-alignment']
endif

if (host_machine.system() == 'darwin')
    using_osx = true
    link_args = ['-target', 'arm64-apple-macos14', '-framework', 'Security']

    if not get_option('use_frame_intrinsic').disabled()
        c_args = c_args + ['-DC4M_USE_FRAME_INTRINSIC']
    endif
else
    using_osx = false
    link_args = []
    c_args = c_args + ['-D_GNU_SOURCE']

    # This doesn't seem to always work right on Linux so don't use it by default.
    if get_option('use_frame_intrinsic').enabled()
        c_args = c_args + ['-DC4M_USE_FRAME_INTRINSIC']
    endif
endif

if get_option('forkless_tests').enabled()
    c_args = c_args + ['-DC4M_TEST_WITHOUT_FORK']
endif

backtrace = cc.find_library('backtrace', required: false)

if backtrace.found()
    c_args = c_args + ['-DC4M_BACKTRACE_SUPPORTED']
endif

if get_option('show_preprocessor_config').enabled()
    c_args = c_args + ['-DC4M_SHOW_PREPROC_CONFIG']
endif

fpty_code = '''
#include <stddef.h>
#include <pty.h>

int main(void) { forkpty(NULL, NULL, NULL, NULL); return 0; }
'''

if cc.links(fpty_code, name: 'forkpty_check')
    add_project_arguments('-DHAVE_PTY_H', language: 'c')
endif

if get_option('buildtype') == 'release'
    c_args = c_args + ['-Ofast', '-flto']
    link_args = link_args + ['-flto']
endif

if not get_option('use_gc_ptr_hooks').disabled()
    c_args = c_args + ['-DC4M_USE_GC_HOOKS']
endif

if get_option('keep_alloc_locations') == true and get_option('dev_mode') == false
    c_args = c_args + ['-DHATRACK_ALLOC_PASS_LOCATION']

endif

if get_option('dev_mode') == true
    c_args = c_args + ['-DC4M_DEV']

    if not get_option('exception_traces').disabled()
        c_args = c_args + ['-DC4M_DEBUG']
    endif

    if get_option('use_asan').enabled()
        c_args = c_args + ['-fsanitize=address', '-fsanitize-recover=all']
        link_args = link_args + ['-fsanitize=address', '-fsanitize-recover=all']
    endif

    if get_option('use_ubsan').enabled()
        c_args = c_args + [
            '-fsanitize=undefined',
            '-fno-sanitize=function',
            '-fsanitize-recover=all',
        ]
        link_args = link_args + [
            '-fsanitize=undefined',
            '-fsanitize-recover=all',
        ]
    endif

    memcheck = get_option('use_memcheck')
    if not (memcheck == 'off')
        c_args = c_args + [
            '-DC4M_FULL_MEMCHECK',
            '-DHATRACK_ALLOC_PASS_LOCATION',
        ]

        if memcheck == 'strict'
            c_args = c_args + ['-DC4M_STRICT_MEMCHECK']
        endif

        show_count = get_option('memcheck_show_allocs')
        if show_count != 0
            as_str = show_count.to_string()
            c_args = c_args + ['-DC4M_SHOW_NEXT_ALLOCS=' + as_str]
        endif
    endif

    if get_option('show_gc_stats').enabled()
        c_args = c_args + ['-DC4M_GC_STATS', '-DHATRACK_ALLOC_PASS_LOCATION']
    endif

    vm_debug = get_option('vm_debug')

    if vm_debug != 'always off'
        c_args = c_args + ['-DC4M_VM_DEBUG']
    endif

    if vm_debug == 'default on'
        c_args = c_args + ['-DC4M_VM_DEBUG_DEFAULT=true']
    endif

    if get_option('warn_on_zero_allocs') == true
        c_args = c_args + ['-DC4M_WARN_ON_ZERO_ALLOCS']
    endif


    gctrace = get_option('gc_tracing')

    if gctrace != 'off'
        c_args = c_args + ['-DC4M_GC_FULL_TRACE']
    endif

    if gctrace == 'full'
        c_args = c_args + ['-DC4M_GC_FULL_TRACE', '-DC4M_GC_ALL_ON']
    endif

endif

exe_link_args = link_args + ['-flto', '-w']
exe_c_args = c_args + ['-flto', '-DHATRACK_REFERENCE_ALGORITHMS']

c4m_c_args = c_args


c4m_core = [
    'src/core/init.c',
    'src/core/gcbase.c',
    'src/core/collect.c',
    'src/core/kargs.c',
    'src/core/exceptions.c',
    'src/core/types.c',
    'src/core/typestore.c',
    'src/core/marshal.c',
    'src/core/literals.c',
    'src/core/attrstore.c',
    'src/core/vm.c',
    'src/core/vmmarshal.c',
    'src/core/ffi.c',
    'src/core/object.c',
]
c4m_adts = [
    'src/adts/string.c',
    'src/adts/buffer.c',
    'src/adts/dict.c',
    'src/adts/set.c',
    'src/adts/hatlists.c',
    'src/adts/list.c',
    'src/adts/grid.c',
    'src/adts/tree.c',
    'src/adts/numbers.c',
    'src/adts/mixed.c',
    'src/adts/tuple.c',
    'src/adts/ipaddr.c',
    'src/adts/callback.c',
    'src/adts/streams.c',
    'src/adts/flags.c',
    'src/adts/box.c',
]

c4m_io = [
    'src/io/ansi.c',
    'src/io/switchboard.c',
    'src/io/subproc.c',
    'src/io/term.c',
    'src/io/http.c',
]

c4m_compiler = [
    'src/compiler/compile.c',
    'src/compiler/lex.c',
    'src/compiler/parse.c',
    'src/compiler/errors.c',
    'src/compiler/scope.c',
    'src/compiler/specs.c',
    'src/compiler/cfg_build.c',
    'src/compiler/cfg.c',
    'src/compiler/ast_utils.c',
    'src/compiler/decl_pass.c',
    'src/compiler/check_pass.c',
    'src/compiler/memory_layout.c',
    'src/compiler/codegen.c',
    'src/compiler/disasm.c',
    'src/compiler/objgen.c',
]

c4m_util = [
    'src/util/style.c',
    'src/util/styledb.c',
    'src/util/colors.c',
    'src/util/breaks.c',
    'src/util/hex.c',
    'src/util/tree_pattern.c',
    'src/util/conststr.c',
    'src/util/richlit.c',
    'src/util/format.c',
    'src/util/fptostr.c',
    'src/util/path.c',
    'src/util/watch.c',
    'src/util/wrappers.c',
    'src/util/ctrace.c',
    'src/util/static_config.c',
]

c4m_crypto = ['src/crypto/sha.c']

c4m_src = c4m_core + c4m_adts + c4m_io + c4m_compiler + c4m_util + c4m_crypto


hat_primary = [
    'src/hatrack/support/hatrack_common.c',
    'src/hatrack/support/mmm.c',
    'src/hatrack/support/counters.c',
    'src/hatrack/support/malloc.c',
    'src/hatrack/hash/crown.c',
    'src/hatrack/hash/dict.c',
    'src/hatrack/hash/xxhash.c',
    'src/hatrack/hash/set.c',
    'src/hatrack/hash/woolhat.c',
    'src/hatrack/array/flexarray.c',
    'src/hatrack/array/zeroarray.c',
    'src/hatrack/queue/queue.c',
    'src/hatrack/queue/hatring.c',
    'src/hatrack/queue/stack.c',
    'src/hatrack/queue/debug.c',
    'src/hatrack/queue/logring.c',
]

hat_hashref = [
    'src/hatrack/hash/ballcap.c',
    'src/hatrack/hash/duncecap.c',
    'src/hatrack/hash/hihat-a.c',
    'src/hatrack/hash/hihat.c',
    'src/hatrack/hash/lohat-a.c',
    'src/hatrack/hash/lohat.c',
    'src/hatrack/hash/newshat.c',
    'src/hatrack/hash/oldhat.c',
    'src/hatrack/hash/refhat.c',
    'src/hatrack/hash/swimcap.c',
    'src/hatrack/hash/tiara.c',
    'src/hatrack/hash/tophat.c',
    'src/hatrack/hash/witchhat.c',
]

hash_test_src = hat_hashref + [
    'src/tests/hash/test.c',
    'src/tests/hash/default.c',
    'src/tests/hash/performance.c',
    'src/tests/hash/config.c',
    'src/tests/hash/functional.c',
    'src/tests/hash/rand.c',
    'src/tests/hash/testhat.c',
]

if not using_osx and not using_glibc
    target = build_machine.cpu_family()
    quark_files = ['src/quark/quark.c', 'src/quark' / target + '.s']
    hat_primary = hat_primary + quark_files
endif

lib_src = c4m_src + hat_primary

test_src = ['src/tests/test.c']

threads = dependency('threads')
math = cc.find_library('m', required: false)
ffi = cc.find_library(
    'ffi',
    required: true
)

crypto = cc.find_library('crypto')
ssl = cc.find_library('ssl')

unibreak = dependency('libunibreak')
utf8proc = dependency('libutf8proc')
libcurl  = dependency('curl')

deps = [unibreak, utf8proc, threads, ffi, ssl, crypto, backtrace, libcurl]
opts = [math]
if using_glibc
    opts = opts + [cc.find_library('atomic')]
endif
all_deps = deps + opts

libc4m = static_library(
    'con4m',
    lib_src,
    include_directories: incdir,
    dependencies: all_deps,
    c_args: c4m_c_args,
    link_args: link_args,
)


if get_option('build_con4m_dll').enabled()
    library(
        'con4m-dll',
        lib_src,
        include_directories: incdir,
        dependencies: all_deps,
        c_args: c4m_c_args,
        link_args: link_args,
    )
endif

executable(
    'c4test',
    test_src,
    include_directories: incdir,
    dependencies: [all_deps],
    c_args: c_args,
    link_args: exe_link_args,
    link_with: libc4m,
)

if get_option('build_hatrack').enabled()
    libhat = static_library(
        'hatrack',
        lib_src,
        include_directories: incdir,
        c_args: c_args,
        link_args: link_args,
    )

    executable(
        'hash',
        hash_test_src,
        include_directories: incdir,
        dependencies: all_deps,
        link_args: exe_link_args,
        link_with: libhat,
        c_args: exe_c_args,
    )
endif
