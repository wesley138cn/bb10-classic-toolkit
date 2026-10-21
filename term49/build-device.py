#!/base/usr/bin/python3.2
"""Build-only helper for an existing BerryCore compiler/SDK. Does not install."""
from __future__ import print_function
import argparse, glob, os, subprocess
parser=argparse.ArgumentParser()
parser.add_argument('--core',default='/accounts/1000/shared/misc/berrycore')
parser.add_argument('--external',required=True,help='Term49 external directory with include/ and lib/')
parser.add_argument('--pinyin',required=True,help='Patched libgooglepinyin-0.1.2 source directory')
args=parser.parse_args()
root=os.path.dirname(os.path.abspath(__file__));build=os.path.join(root,'build');core=args.core;sdk=core+'/target_10_3_1_995/qnx6'
for p in [core+'/bin/gcc',core+'/bin/g++',sdk+'/usr/include',args.external+'/include',args.external+'/lib',args.pinyin+'/src']:
    if not os.path.exists(p):raise SystemExit('Missing prerequisite: '+p)
if os.path.basename(os.path.normpath(args.pinyin))!='libgooglepinyin-0.1.2':raise SystemExit('Keep libgooglepinyin-0.1.2 directory name for adapter includes')
if not os.path.isdir(build):os.makedirs(build)
env=os.environ.copy();env['PATH']=core+'/bin:/base/bin:/base/usr/bin';env['LD_LIBRARY_PATH']='/base/lib:/base/usr/lib'
common=['--sysroot='+sdk,'-B'+sdk+'/armle-v7/lib/','-B'+core+'/arm-unknown-nto-qnx8.0.0eabi/bin/','-O2','-D__PLAYBOOK__','-I'+args.external+'/include','-I'+root+'/src','-I'+root,'-I'+os.path.dirname(os.path.abspath(args.pinyin)),'-I'+sdk+'/usr/include/freetype2']
cc=[core+'/bin/gcc','-std=gnu99']+common;cxx=[core+'/bin/g++','-std=gnu++0x','-D_QNX_SOURCE']+common
libs=['-L'+sdk+'/armle-v7/lib','-L'+sdk+'/armle-v7/usr/lib','-L'+args.external+'/lib','-lSDL12','-lTouchControlOverlay','-lconfig','-lbps','-lscreen','-lfreetype','-licui18n','-licuuc','-lclipboard','-lm','-lcpp']
def run(command):
    print('Building:',os.path.basename(command[-1]));subprocess.check_call(command,env=env)
def objects(files,compiler,prefix):
    result=[]
    for source in sorted(files):
        obj=build+'/'+prefix+os.path.basename(source)+'.o';run(compiler+['-c',source,'-o',obj]);result.append(obj)
    return result
engine=objects(glob.glob(args.pinyin+'/src/*.cpp'),cxx,'ime-')
adapter=objects([root+'/ime_engine.cpp'],cxx,'adapter-')
terminal=objects(glob.glob(root+'/src/*.c'),cc,'terminal-')
run(cc+terminal+engine+adapter+libs+['-o',build+'/Term49-Classic'])
run(cxx+engine+[args.pinyin+'/tools/pinyinime_dictbuilder.cpp','-o',build+'/dictbuilder'])
run([build+'/dictbuilder',args.pinyin+'/data/rawdict_utf16_65105_freq.txt',args.pinyin+'/data/valid_utf16.txt',build+'/dict_pinyin.dat'])
run(cxx+engine+[root+'/ime_engine.cpp',root+'/test-engine.cpp','-o',build+'/test-engine'])
run([build+'/test-engine',build+'/dict_pinyin.dat',build+'/test-user.dat'])
print('Build finished; review README before copying runtime files. No launcher changed.')
