project('listbox-c', 'c')

gtk_dep = dependency('gtk+-4.0')
deps = [gtk_dep]

srcdir = include_directories('src/')

listbox = subproject('liblistbox')
listbox_dep = listbox.get_variable('liblistbox_dep')

deps = [gtk_dep, listbox_dep]

executable(
  'demo',
  sources: 'demo.c',
  dependencies: deps,
  c_args: [
    '-Werror=implicit-function-declaration'
  ]
)


executable(
  'imagelist',
  sources: 'imagelist.c',
  dependencies: deps,
  c_args: [
    '-Werror=implicit-function-declaration'
  ]
)
