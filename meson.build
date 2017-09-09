project('listbox-c', 'c')

gtk_dep = dependency('gtk+-4.0')
sysprof_dep = dependency('sysprof-2')


demo_sources = files(
  'gd-model-list-box.c',
  'demo.c',
)

executable(
  'demo',
  sources: demo_sources,
  dependencies: [gtk_dep, sysprof_dep],
)



test_exec = executable(
  'tests',
  ['tests.c', 'gd-model-list-box.c'],
  dependencies: [gtk_dep]
)
test('main', test_exec)
