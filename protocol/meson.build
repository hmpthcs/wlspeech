wayland_scanner = dependency('wayland-scanner', native: true)
wayland_scanner_path = wayland_scanner.get_variable('wayland_scanner')

input_method_xml = 'input-method-unstable-v2.xml'

src += custom_target(
	input: input_method_xml,
	output: '@BASENAME@.c',
	command: [wayland_scanner_path, 'private-code', '@INPUT@', '@OUTPUT@'],
)

src += custom_target(
	input: input_method_xml,
	output: '@BASENAME@.h',
	command: [wayland_scanner_path, 'client-header', '@INPUT@', '@OUTPUT@'],
)
