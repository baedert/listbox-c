project('listbox-c', 'c')

gtk_dep = dependency('gtk+-4.0')


demo_sources = files(
  'gd-model-list-box.c',
  'demo.c'
)

executable(
  'demo',
  sources: demo_sources,
  dependencies: gtk_dep,
)

