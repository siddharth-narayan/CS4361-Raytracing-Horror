project('3d-maze-game', 'c',
  version: '0.1',
  default_options: ['c_std=c11', 'warning_level=3'])

cc = meson.get_compiler('c')

# Source files
sources = [
  'src/main.c',
  'src/maze.c',
  'src/assets.c'
]

# Include directory
include_dir = include_directories('include')

# Dependencies
raylib = dependency('raylib', required: true)

# Executable
executable(
  'main',
  sources,
  include_directories: include_dir,
  dependencies: [raylib],
  link_args: ['-lm']
)
