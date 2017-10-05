project('listbox-c', 'c')

gtk_dep = dependency('gtk+-4.0')

srcdir = include_directories('src/')

liblistbox = library(
  'listbox',
  ['src/gd-model-list-box.c'],
  include_directories: srcdir,
  dependencies: gtk_dep,
)

listbox_dep = declare_dependency(
  link_with: liblistbox,
  include_directories: srcdir,
)

executable(
  'demo',
  sources: 'demo.c',
  dependencies: [gtk_dep, listbox_dep],
  c_args: [
    '-Werror=implicit-function-declaration'
  ]
)


executable(
  'imagelist',
  sources: 'imagelist.c',
  dependencies: [gtk_dep, listbox_dep],
  c_args: [
    '-Werror=implicit-function-declaration'
  ]
)


test_exec = executable(
  'tests',
  ['tests.c'],
  dependencies: [gtk_dep, listbox_dep]
)
test('main', test_exec)
