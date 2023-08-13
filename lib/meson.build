if target_machine.system() == 'linux'
  lib_link_compile_args = '-DLINK_PLATFORM_LINUX'
  lib_rtmidi_system = '-D__LINUX_ALSA__'
  lib_rtmidi_dependencies = dependency('alsa')
elif target_machine.system() == 'windows'
  lib_link_compile_args = '-DLINK_PLATFORM_WINDOWS'
  lib_rtmidi_system = '-D__WINDOWS_MM__'
  lib_rtmidi_dependencies = [
    dependency('winmm'),
    dependency('iphlpapi'),
    dependency('ws2_32')
  ]
elif target_machine.system() == 'darwin'
  lib_link_compile_args = '-DLINK_PLATFORM_MACOSX'
  lib_rtmidi_system = '-D__MACOSX_CORE__'
  lib_rtmidi_dependencies = [
    dependency('CoreMIDI'),
    dependency('CoreAudio'),
    dependency('CoreFoundation')
  ]
else
  error('operating system not supported')
endif

lib_expected = declare_dependency(
  include_directories: 'expected'
)

lib_rtmidi = declare_dependency(
  include_directories: 'rtmidi',
  sources: files('rtmidi/RtMidi.cpp'),
  compile_args: lib_rtmidi_system,
  dependencies: lib_rtmidi_dependencies
)

lib_asio = declare_dependency(
  include_directories: 'asio/include',
)

lib_link = declare_dependency(
  include_directories: 'link/include',
  dependencies: [ lib_asio ],
  compile_args: lib_link_compile_args
)

lib_edit_distance = declare_dependency(
  include_directories: 'edit_distance.cc',
)

lib_replxx = declare_dependency(
  compile_args: '-DREPLXX_STATIC',
  include_directories: 'replxx/include',
  sources: files(
    'replxx/src/conversion.cxx',
    'replxx/src/ConvertUTF.cpp',
    'replxx/src/escape.cxx',
    'replxx/src/history.cxx',
    'replxx/src/prompt.cxx',
    'replxx/src/replxx_impl.cxx',
    'replxx/src/replxx.cxx',
    'replxx/src/terminal.cxx',
    'replxx/src/util.cxx',
    'replxx/src/wcwidth.cpp',
    'replxx/src/windows.cxx',
  )
)

lib_catch2 = declare_dependency(
  include_directories: 'Catch2',
  sources: files(
    'Catch2/catch_amalgamated.cpp'
  )
)