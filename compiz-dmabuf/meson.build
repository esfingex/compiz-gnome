project('compiz-dmabuf', 'c',
  version: '0.1.0',
  default_options: ['c_std=c11', 'warning_level=2', 'werror=false']
)

egl_dep = dependency('egl', required: true)
gobject_dep = dependency('gobject-2.0', required: true)
gio_dep = dependency('gio-2.0', required: true)

incs = include_directories('.')

compiz_dmabuf_lib = library(
  'compiz-dmabuf',
  'compiz-dmabuf.c',
  dependencies: [egl_dep, gobject_dep, gio_dep],
  install: true,
  install_dir: get_option('libdir') / 'gnome-shell',
  c_args: ['-D_GNU_SOURCE', '-fvisibility=hidden']
)

install_headers('compiz-dmabuf.h', subdir: 'compiz-dmabuf')
