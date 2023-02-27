project(
	'mcwayface',
	'c',
	version: '0.0.1',
	license: 'MIT',
	meson_version: '>=0.47.0',
	default_options: [
		'c_std=c11',
		'warning_level=2',
		'werror=true',
	],
)

add_project_arguments('-DWLR_USE_UNSTABLE', language: 'c')
add_project_arguments('-Wno-unused-parameter', language: 'c')
add_project_arguments('-D_POSIX_C_SOURCE=200112L', language: 'c')
add_project_arguments('-Og', language: 'c')

cc = meson.get_compiler('c')
# Clang complains about some zeroed initializer lists (= {0}), even though they
# are valid
if cc.get_id() == 'clang'
	add_project_arguments('-Wno-missing-field-initializers', language: 'c')
	add_project_arguments('-Wno-missing-braces', language: 'c')
endif

# Avoid wl_buffer deprecation warnings
add_project_arguments('-DWL_HIDE_DEPRECATED', language: 'c')

pixman         = dependency('pixman-1')
wlroots        = dependency('wlroots')
wayland_server = dependency('wayland-server')

dep_scanner = dependency('wayland-scanner', native: true)
prog_scanner = find_program('wayland-scanner', dep_scanner.get_pkgconfig_variable('wayland_scanner'))

dep_wp = dependency('wayland-protocols', version: '>= 1.30')
dir_wp_base = dep_wp.get_pkgconfig_variable('pkgdatadir')
xml_path = '@0@/stable/xdg-shell/xdg-shell.xml'.format(dir_wp_base)

run_command(prog_scanner, 'server-header', xml_path, 'xdg-shell-protocol.h', check: true)


executable(
	'mcwayface',
	[
                'xdg-shell-protocol.h',
		'src/main.c'
	],
	dependencies: [
		pixman,
		wlroots,
		wayland_server,
	]
)
