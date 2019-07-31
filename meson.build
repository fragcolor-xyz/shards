project('cbjs', 'cpp')

buildType = get_option('buildtype')
nimgc = get_variable('nimgc', false)
targetOs = target_machine.system()

build = meson.build_root()
root = meson.source_root()

## nim general opts

nim = find_program('nim')
nimopts = [
  'cpp',
  '--nimcache:' + build + '/nimcache',
  '-d:meson'
]

if buildType == 'debug'
  nimopts = nimopts + ['--passC:-ggdb']
else
  nimopts = nimopts + ['-d:danger', '-d:release']
endif

if not nimgc
  nimopts = nimopts + ['--newruntime', '-d:useWinAnsi', '-d:useMalloc']
endif

## ~ nim general opts

## runtime

cbruntimeIncludes = include_directories(
  '../chainblocks/src/core/',
  '../chainblocks/src/core/3rdparty'
)

cbruntime = static_library(
  'cbruntime',
  [
    '../chainblocks/src/core/runtime.cpp',
    '../chainblocks/src/core/3rdparty/easylogging++.cc'
  ],
  include_directories: cbruntimeIncludes,
  cpp_args: [
    '-DELPP_FEATURE_CRASH_LOG',
    '-static',
    '-ffast-math'
  ]
)

## ~ runtime

## nim runtime

boostContext = '-lboost_context'
if targetOs == 'windows'
  boostContext = '-lboost_context-mt'
endif

nimoptsRuntime = [
  '--passL:-static',
  '--passC:-static',
  '--passC:-DCHAINBLOCKS_RUNTIME'
]

if targetOs != 'windows'
  nimoptsRuntime = nimoptsRuntime + '--passC:-pthread' + '--passL:-pthread'
endif

build_chainblocks = custom_target(
  'Build libchainblocks.a',
  output: ['libchainblocks.a'],
  input: 'src/nim/chainblocks.nim',
  build_always_stale : true,
  depends: cbruntime,
  command: [
    nim,
    nimopts,
    nimoptsRuntime,
    '-d:blocksTesting',
    '--app:staticlib',
    '-p:' + root + '/src/nim',
    '-o:@OUTPUT0@',
    '@INPUT@'
  ]
)

exe_chainblocks = executable(
  'chainblocks',
  build_chainblocks,
  link_with: cbruntime,
  link_args: [
    '-fuse-ld=lld',
    boostContext
  ]
)

test('chainblocks general test', exe_chainblocks)

# ~ nim runtime