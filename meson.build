project('libcon4m', 'c', default_options: ['c_std=c21'])

exe_link_args = '-flto -w'
cc = meson.get_compiler('c')


if (host_machine.system() == 'darwin')
  link_args  = ['-target', 'arm64-apple-macos14', '-object_path_lto',
                '-framework', 'Security']
else
  link_args  = ''
endif


fpty_code = '''#include <pty.h>

void func() { forkpty(NULL, NULL, NULL, NULL;) }
'''

if cc.links(fpty_code, name: 'forkpty_check')
  add_project_arguments('-DHAVE_PTY_H')
endif

c4m_src  = ['src/style.c',
            'src/colors.c',
            'src/breaks.c', 
            'src/string.c',
            'src/ansi.c',
            'src/hex.c',
            'src/switchboard.c',
            'src/subproc.c'
            ]

hat_primary = ['src/hatrack/support/hatrack_common.c',
               'src/hatrack/support/mmm.c',
               'src/hatrack/support/counters.c',
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
               'src/hatrack/queue/logring.c']

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

lib_src = c4m_src + hat_primary

test_src = ['src/tests/test.c']
            
incdir   = include_directories('include')
threads  = dependency('threads')

math   = cc.find_library('m', required : false)
atomic = cc.find_library('atomic', required: false)

cmake    = import('cmake')


unibreak = dependency('libunibreak')
utf8proc = dependency('libutf8proc')
cmark    = dependency('libcmark-gfm')
gumbo    = dependency('gumbo')
ffi      = dependency('libffi')
openssl  = dependency('openssl')


deps     = [unibreak, utf8proc, threads, cmark, gumbo, ffi, openssl]
opts     = [math, atomic]
all_deps = deps + opts

libc4m   = library('con4m', lib_src,
                   include_directories : incdir,
                   dependencies : all_deps,
                   link_args: link_args,
                   version : '0.1.0')

executable('c4test', test_src,
           include_directories : incdir,
           dependencies : all_deps,
           link_with : libc4m)

executable('hash', hash_test_src + hat_primary,
           include_directories : incdir,
           dependencies : all_deps,
           c_args : '-DHATRACK_REFERENCE_ALGORITHMS')

