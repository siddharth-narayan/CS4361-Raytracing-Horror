project(
  'test-vpn', 'c',
  version : '0.1',
  default_options : ['warning_level=3']
)

sources = [
  'src/main.c',
]

dependencies = dependency('raylib')

client = executable(
  'main', 
  sources,
  dependencies : dependencies
)