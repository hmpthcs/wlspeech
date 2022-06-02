project(
	'wlspeech',
	'c',
)

deepspeech = dependency('deepspeech')
alsa = dependency('alsa')

executable(
	'wlspeech',
	files('main.c'),
	dependencies: [deepspeech, alsa],
	install: true,
)
