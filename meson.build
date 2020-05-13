# project name, version and required libraries 
project('vmod_digest', 'c', version: '"1.0.2"')
name = 'digest'
vapi = dependency('varnishapi', version : '>=6.4')
mhash = dependency('mhash')

# find our programs
vtest = find_program('varnishtest')
vmod_tool = find_program(vapi.get_pkgconfig_variable('vmodtool'))
rst2man = find_program('rst2man')

# we need a config.h file only for the version
conf_data = configuration_data()
conf_data.set('VERSION', meson.project_version())
configure_file( output : 'config.h', configuration : conf_data)

# generate the vmod boilerplate from the vcc file
vtool_output = custom_target('vcc_if.[ch] and RSTs',
    input : 'src/vmod_'+name+'.vcc',
    output : ['vcc_if.c', 'vcc_if.h', 'vmod_'+name+'.man.rst', 'vmod_'+name+'.rst'],
    command : [vmod_tool,  '@INPUT@', '-o', 'vcc_if'],
    install: true,
    install_dir: [false, false, false, get_option('datadir')+'/doc/libvmod_'+name])

# declare the actual vmod file, override the install directory
vmod = shared_module('vmod_'+name,
    [vtool_output, 'src/vmod_'+name+'.c' ],
    dependencies:  [vapi, mhash],
    install: true,
    install_dir : 'usr/lib/varnish/vmods' )

# man page and other documentation
man = custom_target('man page',
    input: 'vmod_'+name+'.man.rst',
    output: 'vmod_'+name+'.3',
    command : [ rst2man, '@INPUT@', '@OUTPUT@' ])
install_man('vmod_'+name+'.3')
install_data('LICENSE', install_dir: get_option('datadir')+'/doc/libvmod'+name)

# list all the VTCs
vtcs = run_command([ '/bin/sh', '-c', 'for i in src/tests/*.vtc; do echo `basename $i`; done']).stdout().strip().split('\n')
# declare a test for each of them
foreach vtc: vtcs
test('command line test', vtest,
    args : ['-Dvmod_topbuild='+meson.build_root(), meson.source_root()+'/src/tests/'+vtc])
endforeach

###############################################################################
# this is need only to keep autotools compatibility
# it can go away if the VTCs use:
#     import NAME from "${vmod_topbuild}/libvmod_$NAME.so";
vmod_link = custom_target('vmod link',
    input: vmod,
    output : 'vmod_link', command : [ '/bin/sh', '-c',
        'NAME='+name+''' && mkdir -p src/.libs/ &&
        rm -rf src/.libs/libvmod_$NAME.so &&
        ln -s ../../libvmod_$NAME.so src/.libs/libvmod_$NAME.so &&
        touch vmod_${NAME}_link'''
    ],
    build_by_default: true)
