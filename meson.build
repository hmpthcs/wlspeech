project(
	'wlspeech',
	'c',
	meson_version: '>=0.60.0',
	default_options: [
		'c_std=c11',
		'warning_level=3',
		'werror=true',
	],
)

add_project_arguments('-D_POSIX_C_SOURCE=200809L', language: 'c')

cc = meson.get_compiler('c')

add_project_arguments(cc.get_supported_arguments([
	'-Wundef',
	'-Wno-unused-parameter',
]), language: 'c')

wayland_client = dependency('wayland-client')
deepspeech = dependency('deepspeech')
alsa = dependency('alsa')

src = files('main.c')

subdir('protocol')

executable(
	'wlspeech',
	src,
	dependencies: [wayland_client, deepspeech, alsa],
	install: true,
)
