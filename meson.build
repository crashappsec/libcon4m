project('libcon4m', 'c',
        default_options: ['c_std=c2x',
                          'default_library=static'
                         ],
        version : '0.1.0',
        license: 'MIT')


cc              = meson.get_compiler('c')
incdir          = include_directories('include')
using_osx       = false
glibc_check       = '''
#ifndef __GLIBC__
#error NO GLIBC   
'''

if cc.compiles(glibc_check)
  using_glibc = true
else
  using_glibc = false
endif

c_args        = ['-Wextra',
                 '-g',
                 '-Wno-unused-parameter',
                 '-Wno-cast-function-type',
                 '-Wno-unused-value']

if (host_machine.system() == 'darwin')
  using_osx  = true
  link_args  = ['-target', 'arm64-apple-macos14',
                '-framework', 'Security']
else
  using_osx = false
  link_args = []
  c_args    = c_args + ['-D_POSIX_SOURCE',
                        '-D_GNU_SOURCE',
                        '-Wno-builtin-declaration-mismatch'
                       ]
  
endif

exe_link_args = link_args + ['-flto', '-w']
exe_c_args    = c_args + ['-DHATRACK_REFERENCE_ALGORITHMS']

fpty_code = '''#include <pty.h>

void func() { forkpty(NULL, NULL, NULL, NULL;) }
'''

if cc.links(fpty_code, name: 'forkpty_check')
  add_project_arguments('-DHAVE_PTY_H')
endif

c4m_src  = ['src/con4m/style.c',
            'src/con4m/colors.c',
            'src/con4m/breaks.c', 
            'src/con4m/string.c',
            'src/con4m/buffer.c',
            'src/con4m/ansi.c',
            'src/con4m/hex.c',
            'src/con4m/switchboard.c',
            'src/con4m/subproc.c',
            'src/con4m/gc.c',
            'src/con4m/term.c',
            'src/con4m/kargs.c',
            'src/con4m/object.c',
            'src/con4m/dict.c',
            'src/con4m/set.c',
            'src/con4m/lists.c',
            'src/con4m/xlist.c',
            'src/con4m/grid.c',
            'src/con4m/styledb.c',
            'src/con4m/exceptions.c',
            'src/con4m/marshal.c',
            'src/con4m/types.c',
            'src/con4m/tree.c',
            'src/con4m/tree_pattern.c',            
            'src/con4m/numbers.c',
            'src/con4m/mixed.c',
            'src/con4m/conststr.c',
            'src/con4m/tuple.c',
            'src/con4m/ipaddr.c',
            'src/con4m/callback.c',
            'src/con4m/streams.c',
            'src/con4m/richlit.c',
            'src/con4m/literals.c',
            'src/con4m/init.c',
            'src/con4m/attrstore.c',
            'src/con4m/vm.c',
            'src/con4m/vmmarshal.c',
            'src/con4m/format.c',
            'src/con4m/fptostr.c',
            'src/con4m/path.c',
            'src/con4m/crypto/sha.c',
            'src/con4m/frontend/compile.c',
            'src/con4m/frontend/lex.c',
            'src/con4m/frontend/parse.c',
            'src/con4m/frontend/errors.c',
            'src/con4m/frontend/scope.c',
            'src/con4m/frontend/specs.c',
            'src/con4m/frontend/cfg_repr.c',            
            'src/con4m/frontend/cfg.c',
            'src/con4m/frontend/treematch.c',
            'src/con4m/frontend/decl_pass.c',
            'src/con4m/frontend/check_pass.c',
            ]

hat_primary = ['src/hatrack/support/hatrack_common.c',
               'src/hatrack/support/mmm.c',
               'src/hatrack/support/counters.c',
               'src/hatrack/support/malloc.c',
               'src/hatrack/hash/crown.c',
               'src/hatrack/hash/dict.c',
               'src/hatrack/hash/xxhash.c',
               'src/hatrack/hash/set.c',
               'src/hatrack/hash/woolhat.c',
               'src/hatrack/array/flexarray.c',
               'src/hatrack/queue/queue.c',
               'src/hatrack/queue/hatring.c',
               'src/hatrack/queue/stack.c',
               'src/hatrack/queue/debug.c',
               'src/hatrack/queue/logring.c'
               ]

hat_hashref = ['src/hatrack/hash/ballcap.c',
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
               'src/hatrack/hash/witchhat.c'
]

hash_test_src = hat_hashref + [
                 'src/tests/hash/test.c',
                 'src/tests/hash/default.c',
                 'src/tests/hash/performance.c',
                 'src/tests/hash/config.c',
                 'src/tests/hash/functional.c',
                 'src/tests/hash/rand.c',
                 'src/tests/hash/testhat.c']

if not using_osx and not using_glibc
  target      = build_machine.cpu_family()
  quark_files = ['src/quark/quark.c', 'src/quark' / target + '.s']
  hat_primary = hat_primary + quark_files
endif

lib_src = c4m_src + hat_primary

test_src = ['src/tests/test.c']
            
threads  = dependency('threads')
math     = cc.find_library('m', required : false)
ffi      = cc.find_library('ffi')

crypto   = cc.find_library('crypto')
ssl      = cc.find_library('ssl')

unibreak = dependency('libunibreak')
utf8proc = dependency('libutf8proc')

deps     = [unibreak, utf8proc, threads, ffi, ssl, crypto]
opts     = [math]
all_deps = deps + opts

if using_glibc
  opts   = opts + [cc.find_library('atomic')]
endif

libc4m   = static_library('con4m',
                          lib_src,
                          include_directories : incdir,
                          dependencies : all_deps,
                          c_args : c_args,
                          link_args: link_args)
# library('con4m-dll',
#         lib_src,
#         include_directories : incdir,
#         dependencies : all_deps,
#         c_args : c_args,
#         link_args: link_args)

executable('c4test', test_src,
                      include_directories : incdir,
                      dependencies : [all_deps],
                      c_args : c_args,
                      link_args : exe_link_args,
                      link_with : libc4m)

executable('hash', hash_test_src,
           include_directories : incdir,
           dependencies : all_deps,
           link_args : exe_link_args,
           link_with : libc4m,
           c_args : exe_c_args)

