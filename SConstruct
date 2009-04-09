env = Environment(CCFLAGS = ['-Iinclude', '-ggdb', '-Wall'], LINKFLAGS = ['-ggdb'])

env.Library('src/libotc.a',
            ['src/otc.cc',
             'src/cmap.cc',
             'src/head.cc',
             'src/hhea.cc',
             'src/hmtx.cc',
             'src/maxp.cc',
             'src/name.cc',
             'src/os2.cc',
             'src/post.cc',
             'src/loca.cc',
             'src/glyf.cc'
            ])

env.Program('test/otc-sanitise.cc', LIBS = ['otc'], LIBPATH='src')
env.Program('test/idempotent.cc', LIBS = ['otc'], LIBPATH='src')
