project(
  'test-vpn', 'c',
  version : '0.1',
  default_options : ['warning_level=3']
)

sources = [
  'src/main.c',
]

dependencies = [dependency('raylib'), dependency('blas'), dependency('lapacke')]

client = executable(
  'main', 
  sources,
  dependencies : dependencies,
  link_args: ['-lm']
)