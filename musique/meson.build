musique_sources = files(
  'errors.cc',
  'format.cc',
  'interpreter/builtin_functions.cc',
  'interpreter/builtin_operators.cc',
  'interpreter/context.cc',
  'interpreter/env.cc',
  'interpreter/interpreter.cc',
  'interpreter/starter.cc',
  'lexer/lexer.cc',
  'lexer/lines.cc',
  'location.cc',
  'main.cc',
  'midi/midi.cc',
  'midi/rt_midi.cc',
  'parser/parser.cc',
  'pretty.cc',
  'random.cc',
  'runner.cc',
  'ui/program_arguments.cc',
  'unicode_tables.cc',
  'unicode.cc',
  'user_directory.cc',
  'value/array.cc',
  'value/block.cc',
  'value/chord.cc',
  'value/intrinsic.cc',
  'value/note.cc',
  'value/number.cc',
  'value/value.cc'
)

python = find_program('python3')

builtin_function_documentation_cc = custom_target(
  'builtin_function_documentation.cc',
  input: 'interpreter/builtin_functions.cc',
  output: 'builtin_function_documentation.cc',
  command: [
    python,
    files('../scripts/document-builtin.py'),
    '-f', 'cpp',
    '-o', '@OUTPUT@',
    '@INPUT@'
  ]
) 

lib_threads = dependency('threads')

if target_machine.system() == 'linux' or target_machine.system() == 'darwin'
  musique_warnings = [
    '-Werror=switch',
    '-Werror=return-type',
    '-Werror=unused-result',
  ]
else
  musique_warnings = []
endif

# TODO: Version control information - mention which commit this build was based on
musique = executable(
  'musique',
  [musique_sources, builtin_function_documentation_cc],
  include_directories: include_directories('..'),
  dependencies: [
    lib_expected,
    lib_rtmidi,
    lib_link,
    lib_edit_distance,
    lib_replxx,
    lib_threads
  ],
  override_options: ['cpp_std=c++20'],
  cpp_args: [
    '-DMusique_Version="@0@"'.format(meson.project_version()),
    musique_warnings,
  ]
)

musique_unit_test = executable(
  'musique-unit-test',
  [musique_sources, builtin_function_documentation_cc],
  include_directories: include_directories('..'),
  dependencies: [
    lib_expected,
    lib_rtmidi,
    lib_link,
    lib_edit_distance,
    lib_replxx,
    lib_catch2,
    lib_threads
  ],
  override_options: ['cpp_std=c++20'],
  cpp_args: [
    '-DMUSIQUE_UNIT_TESTING',
    '-DMusique_Version="@0@"'.format(meson.project_version()),
  ],
  build_by_default: false
)

test('Unit tests', musique_unit_test)

custom_target(
  'musique_man',
  depends: musique,
  output: 'musique.1',
  command: [musique, 'man'],
  capture: true,
)