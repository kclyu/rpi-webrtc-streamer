#!/usr/bin/env python

# Copyright (c) 2016, rpi-webrtc-streamer Lyu,KeunChang
#

import fnmatch
import os
import subprocess
import sys
import re

"""
Global variable for peerconnection ninja build file
"""
peerconnection_ninja_file = '/obj/webrtc/peerconnection_client.ninja'

search_path = ""
webrtc_build_path = ""

def displaymatch(match):
    if match is None:
        return None
    return '<Match: %r, groups=%r>' % (match.group(), match.groups())

"""
Parse 'defines' from ninja build file and print 
"""
def Parse_Defines(contents):
    regex = re.compile(r'\bdefines\b\s*=.*\$$\n(^\s.*\n)*', re.M)
    sp = re.compile(r'[\s$]')
    
    matched_ = regex.search(contents)
    if matched_ == None:
        sys.stderr.write('failed to match option define\n')

    splited = sp.split(matched_.group(0))
    splited = filter(lambda a: a != '', splited)
    splited.pop(0)   # remove define word
    splited.pop(0)   # remove = character
    print('%s' % " ".join(str(x) for x in splited))


"""
Parse 'ccflags' from ninja build file and print 
"""
def Parse_CCflags(contents):
    regex = re.compile(r'\bcflags_cc\b\s*=.*\$$\n(^\s.*\n)*', re.M)
    sp = re.compile(r'[\s$]')
    
    matched_ = regex.search(contents)
    if matched_ == None:
        sys.stderr.write('failed to match option define\n')

    splited = sp.split(matched_.group(0))
    splited = filter(lambda a: a != '', splited)
    splited.pop(0)   # remove define word
    splited.pop(0)   # remove = character
    print('%s' % " ".join(str(x) for x in splited))


"""
Parse 'cflags' from ninja build file and print 
"""
def Parse_Cflags(contents):
    regex = re.compile(r'\bcflags\b\s*=.*\$$\n(^\s.*\n)*', re.M)
    sp = re.compile(r'[\s$]')
    
    matched_ = regex.search(contents)
    if matched_ == None:
        sys.stderr.write('failed to match option define\n')

    splited = sp.split(matched_.group(0))
    splited = filter(lambda a: a != '', splited)
    splited.pop(0)   # remove define word
    splited.pop(0)   # remove = character
    print('%s' % " ".join(str(x) for x in splited))

"""
Parse 'libs' from ninja build file and print 
"""
def Parse_Libs(contents):
    # build peerconnection_client: link $
    regex = re.compile(r'build peerconnection_client: link \$$\n(^\s.*\n)*', re.M)
    sp = re.compile(r'[\s+$]')
    libs = re.compile(r'.\.a$')

    matched_ = regex.search(contents)
    if matched_ == None:
        sys.stderr.write('failed to match option libs\n')

    splited = sp.split(matched_.group(0))
    splited = filter(lambda a: libs.search(a), splited)
    sys.stderr.write('webrtc_build_path : ' + webrtc_build_path )
    print('%s' % " ".join(os.path.relpath(webrtc_build_path+'/'+x) for x in splited))

"""
Parse 'include_dirs' from ninja build file and print 
"""
def Parse_Includes(contents):
    regex = re.compile(r'\binclude_dirs\b\s*=.*\$$\n(^\s.*\n)*', re.M)
    sp = re.compile(r'[\s$]')
    
    matched_ = regex.search(contents)
    if matched_ == None:
        sys.stderr.write('failed to match option define\n')

    splited = sp.split(matched_.group(0))
    splited = filter(lambda a: a != '', splited)
    splited.pop(0)   # remove define word
    splited.pop(0)   # remove = character
    print('%s' % " ".join(str(x) for x in splited))


def ParseNinjaFile(ninja_file, option):
    ninja_file_contents = open(ninja_file,'r').read()

    if option == 'defines':
        Parse_Defines(ninja_file_contents)
    elif option == 'libs':
        Parse_Libs(ninja_file_contents)
    elif option == 'ccflags':
        Parse_CCflags(ninja_file_contents)
    elif option == 'cflags':
        Parse_Cflags(ninja_file_contents)
    elif option == 'includes':
        Parse_Includes(ninja_file_contents)
    else:
        sys.stderr.write('undefined options : ' + option + ' specified!\n')
        return 
        

def Main(argv):
    if len(argv) != 4:
        sys.stderr.write('Ninja build file parser for extracting compile and build options\n')
        sys.stderr.write('Usage: ' + argv[0] + ' <webrtc_root> <build_type> <options>\n')
        sys.stderr.write('  webrtc_root : home of WebRTC source tree\n')
        sys.stderr.write('  build_type  : arm/out/{Debug|Release}\n')
        sys.stderr.write('  options     : libs - extract the list of library\n')
        sys.stderr.write('                defines - extract CC defines options\n')
        sys.stderr.write('                includes - extract CC include options \n')
        sys.stderr.write('                ccflags - extract ccflags\n')
        sys.stderr.write('                cflags - extract cflags\n')
        return 1

    global search_path, webrtc_build_path
    search_path = os.path.normpath(argv[1])
    webrtc_build_path = search_path + '/src/' + argv[2]
    target_file = webrtc_build_path + peerconnection_ninja_file

    if not os.path.exists(webrtc_build_path):
        sys.stderr.write('webrtc build path does not exist: %s\n' % webrtc_build_path)
        return 1

    if not os.path.exists(target_file):
        sys.stderr.write('peerconnection ninja does not exist: %s\n' % target_file)
        return 1

    ParseNinjaFile(target_file, argv[3])


if __name__ == '__main__':
  sys.exit(Main(sys.argv))
