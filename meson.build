project('listbox-c', 'c')

gtk_dep = dependency('gtk+-4.0')
sysprof_dep = dependency('sysprof-2')


demo_sources = files(
  'src/gd-model-list-box.c',
  'demo.c',
)

srcdir = include_directories('src/')

executable(
  'demo',
  sources: demo_sources,
  dependencies: [gtk_dep, sysprof_dep],
  include_directories: srcdir,
  c_args: [
    '-Werror=implicit-function-declaration'
  ]
)



test_exec = executable(
  'tests',
  ['tests.c', 'src/gd-model-list-box.c'],
  include_directories: srcdir,
  dependencies: [gtk_dep]
)
test('main', test_exec)
