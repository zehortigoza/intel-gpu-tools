lib_sources = [
	'drmtest.c',
	'huc_copy.c',
	'i915/gem.c',
	'i915/gem_context.c',
	'i915/gem_create.c',
	'i915/gem_engine_topology.c',
	'i915/gem_scheduler.c',
	'i915/gem_submission.c',
	'i915/gem_ring.c',
	'i915/gem_mman.c',
	'i915/gem_vm.c',
	'i915/intel_memory_region.c',
	'i915/intel_mocs.c',
	'i915/i915_blt.c',
	'igt_collection.c',
	'igt_color_encoding.c',
	'igt_debugfs.c',
	'igt_device.c',
	'igt_device_scan.c',
	'igt_drm_fdinfo.c',
	'igt_aux.c',
	'igt_gt.c',
	'igt_halffloat.c',
	'igt_io.c',
	'igt_matrix.c',
	'igt_params.c',
	'igt_perf.c',
	'igt_primes.c',
	'igt_rand.c',
	'igt_rapl.c',
	'igt_stats.c',
	'igt_syncobj.c',
	'igt_sysfs.c',
	'igt_sysrq.c',
	'igt_taints.c',
	'igt_thread.c',
	'igt_vec.c',
	'igt_vgem.c',
	'igt_x86.c',
	'instdone.c',
	'intel_allocator.c',
	'intel_allocator_msgchannel.c',
	'intel_allocator_random.c',
	'intel_allocator_reloc.c',
	'intel_allocator_simple.c',
	'intel_batchbuffer.c',
	'intel_bufops.c',
	'intel_chipset.c',
	'intel_ctx.c',
	'intel_device_info.c',
	'intel_os.c',
	'intel_mmio.c',
	'ioctl_wrappers.c',
	'media_spin.c',
	'media_fill.c',
	'gpgpu_fill.c',
	'gpu_cmds.c',
	'rendercopy_i915.c',
	'rendercopy_i830.c',
	'rendercopy_gen4.c',
	'rendercopy_gen6.c',
	'rendercopy_gen7.c',
	'rendercopy_gen8.c',
	'rendercopy_gen9.c',
	'sw_sync.c',
	'intel_aux_pgtable.c',
	'intel_reg_map.c',
	'intel_iosf.c',
	'igt_kms.c',
	'igt_fb.c',
	'igt_core.c',
	'igt_draw.c',
	'igt_list.c',
	'igt_map.c',
	'igt_pm.c',
	'igt_dummyload.c',
	'igt_store.c',
	'uwildmat/uwildmat.c',
	'igt_kmod.c',
	'igt_panfrost.c',
	'igt_v3d.c',
	'igt_vc4.c',
	'igt_psr.c',
	'igt_amd.c',
	'igt_edid.c',
	'igt_eld.c',
	'igt_infoframe.c',
	'veboxcopy_gen12.c',
	'igt_msm.c',
]

lib_deps = [
	cairo,
	glib,
	libatomic,
	libdrm,
	libdw,
	libkmod,
	libprocps,
	libudev,
	math,
	pciaccess,
	pixman,
	pthreads,
	realtime,
	zlib
]

if libdrm_intel.found()
	lib_deps += libdrm_intel
else
	lib_sources += 'stubs/drm/intel_bufmgr.c'
	inc = [ inc, include_directories('stubs/drm') ]
endif

if libdrm_nouveau.found()
	lib_deps += libdrm_nouveau
	lib_sources += [
		'igt_nouveau.c',
		'nouveau/cea0b5.c'
	]
endif

if libunwind.found()
	lib_deps += libunwind
else
	inc = [ inc, include_directories('stubs/libunwind') ]
endif

if valgrind.found()
	lib_deps += valgrind
endif

if gsl.found()
	lib_deps += gsl
	lib_sources += [ 'igt_frame.c', 'igt_audio.c' ]
endif

if alsa.found()
	lib_deps += alsa
	lib_sources += 'igt_alsa.c'
endif

if chamelium.found()
	lib_deps += chamelium
	lib_sources += 'igt_chamelium.c'
	lib_sources += 'igt_chamelium_stream.c'
endif

if get_option('srcdir') != ''
    srcdir = join_paths(get_option('srcdir'), 'tests')
else
    srcdir = join_paths(meson.source_root(), 'tests')
endif

lib_version = vcs_tag(input : 'version.h.in', output : 'version.h',
		      fallback : 'NO-GIT',
		      command : [ 'git', 'log', '-n1', '--pretty=format:g%h' ] )

lib_intermediates = []
foreach f: lib_sources
    name = f.underscorify()
    lib = static_library('igt-' + name,
	[ f, lib_version ],
	include_directories: inc,
	dependencies : lib_deps,
	c_args : [
	    '-DIGT_DATADIR="@0@"'.format(join_paths(prefix, datadir)),
	    '-DIGT_SRCDIR="@0@"'.format(srcdir),
	    '-DIGT_LOG_DOMAIN="@0@"'.format(f.split('.')[0]),
	])

    lib_intermediates += lib
endforeach

lib_igt_build = shared_library('igt',
    ['dummy.c'],
    link_whole: lib_intermediates,
    dependencies: lib_deps,
    install : true,
    soversion : '0',
)

lib_igt = declare_dependency(link_with : lib_igt_build,
			    include_directories : inc)

igt_deps = [ lib_igt ] + lib_deps

lin_igt_chipset_build = static_library('igt_chipset',
                                       ['intel_chipset.c',
                                        'intel_device_info.c'],
                                       include_directories : inc)

lib_igt_chipset = declare_dependency(link_with : lin_igt_chipset_build,
                                     include_directories : inc)

lib_igt_perf_build = static_library('igt_perf',
	['igt_perf.c'],
	include_directories : inc)

lib_igt_perf = declare_dependency(link_with : lib_igt_perf_build,
				  include_directories : inc)

scan_dep = [
	glib,
	libudev,
]

lib_igt_device_scan_build = static_library('igt_device_scan',
	['igt_device_scan.c',
	'igt_list.c',
	'igt_tools_stub.c',
	'intel_device_info.c',
	],
	dependencies : scan_dep,
	include_directories : inc)

lib_igt_device_scan = declare_dependency(link_with : lib_igt_device_scan_build,
				  include_directories : inc)

lib_igt_drm_fdinfo_build = static_library('igt_drm_fdinfo',
	['igt_drm_fdinfo.c'],
	include_directories : inc)

lib_igt_drm_fdinfo = declare_dependency(link_with : lib_igt_drm_fdinfo_build,
				  include_directories : inc)
i915_perf_files = [
  'igt_list.c',
  'i915/perf.c',
  'i915/perf_data_reader.c',
]

i915_perf_hardware = [
  'hsw',
  'bdw', 'chv',
  'sklgt2', 'sklgt3', 'sklgt4',
  'kblgt2', 'kblgt3',
  'cflgt2', 'cflgt3',
  'bxt', 'glk',
  'cnl',
  'icl', 'ehl',
  'tglgt1', 'tglgt2', 'rkl', 'dg1', 'adl',
]

i915_xml_files = []
foreach hw : i915_perf_hardware
  i915_xml_files += files('i915/perf-configs/oa-@0@.xml'.format(hw))
endforeach

i915_perf_files += custom_target(
  'i915-perf-equations',
  input : [ 'i915/perf-configs/perf-equations-codegen.py' ] + i915_xml_files,
  output : [ 'i915_perf_equations.c', 'i915_perf_equations.h' ],
  command : [
    python3, '@INPUT0@',
    '--code', '@OUTPUT0@',
    '--header', '@OUTPUT1@',
    i915_xml_files,
  ])

foreach hw : i915_perf_hardware
  i915_perf_files += custom_target(
    'i915-perf-registers-@0@'.format(hw),
    input : [ 'i915/perf-configs/perf-registers-codegen.py',
              'i915/perf-configs/oa-@0@.xml'.format(hw) ],
    output : [ 'i915_perf_registers_@0@.c'.format(hw),
               'i915_perf_registers_@0@.h'.format(hw), ],
    command : [
      python3, '@INPUT0@',
      '--code', '@OUTPUT0@',
      '--header', '@OUTPUT1@',
      '--xml-file', '@INPUT1@'
    ])
  i915_perf_files += custom_target(
    'i915-perf-metrics-@0@'.format(hw),
    input : [ 'i915/perf-configs/perf-metricset-codegen.py',
              'i915/perf-configs/oa-@0@.xml'.format(hw) ],
    output : [ 'i915_perf_metrics_@0@.c'.format(hw),
               'i915_perf_metrics_@0@.h'.format(hw), ],
    command : [
      python3, '@INPUT0@',
      '--code', '@OUTPUT0@',
      '--header', '@OUTPUT1@',
      '--equations-include', 'i915_perf_equations.h',
      '--registers-include', 'i915_perf_registers_@0@.h'.format(hw),
      '--xml-file', '@INPUT1@',
    ])
endforeach

lib_igt_i915_perf_build = shared_library(
  'i915_perf',
  i915_perf_files,
  dependencies: lib_igt_chipset,
  include_directories : inc,
  install: true,
  soversion: '1')

lib_igt_i915_perf = declare_dependency(
  link_with : lib_igt_i915_perf_build,
  include_directories : inc)

install_headers(
  'igt_list.h',
  'intel_chipset.h',
  'i915/perf.h',
  'i915/perf_data.h',
  'i915/perf_data_reader.h',
  subdir : 'i915-perf'
)

pkgconf = configuration_data()

pkgconf.set('prefix', get_option('prefix'))
pkgconf.set('exec_prefix', '${prefix}')
pkgconf.set('libdir', '${prefix}/@0@'.format(get_option('libdir')))
pkgconf.set('includedir', '${prefix}/@0@'.format(get_option('includedir')))
pkgconf.set('i915_perf_version', '1.2.0')

configure_file(
  input : 'i915-perf.pc.in',
  output : 'i915-perf.pc',
  configuration : pkgconf,
  install_dir : pkgconfigdir)

subdir('tests')
