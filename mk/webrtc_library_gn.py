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
peerconnection_ninja_file = '/obj/examples/peerconnection_client.ninja'

search_path = ""
webrtc_build_path = ""
webrtc_root_path = ""

def displaymatch(match):
    if match is None:
        return None
    return '<Match: %r, groups=%r>' % (match.group(), match.groups())

"""
generate absolute path from
"""
def generate_AbsolutePath( relpath_of_file ):
    return "".join(os.path.abspath(webrtc_build_path+'/../'+relpath_of_file))

"""
Exclude Fiter
    Remove item which matching exclude_filter in splited_list
"""
def Fiter_ExcludeFilter(splited_list, exclude_patterns):
    """
    """
    filtered_splited_list =  []

    for index,inc in enumerate(splited_list):
        #print("Index : %d, Include: %s" % (index, inc ))
        ### filtering rules
        if any(re.findall(exclude_patterns, inc, re.IGNORECASE)):
            continue
        ### ending rule
        filtered_splited_list.append(inc)

    return filtered_splited_list

"""
Parse 'defines' from ninja build file and print
"""
def Parse_Defines(contents):
    regex = re.compile(r'\bdefines\b\s*=.*\n', re.M)
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
Fiter 'ccflags' from ccflags in ninja build
"""
def Fiter_CCflags(ccflags_lists):
    """
    """
    filtered_ccflags_lists =  []
    """
    base filtering and rule matching patterns
    """
    include_system_str = "-isystem"

    for index,inc in enumerate(ccflags_lists):
        # print("Index : %d, Include: %s" % (index, inc ))
        ### replacing rules
        if inc.find(include_system_str) != -1:
            isystem_dir = inc[len("-isystem"):]
            gen_abspath = generate_AbsolutePath(isystem_dir)
            filtered_ccflags_lists.append("-isystem" + gen_abspath)
            continue
        ### ending rule
        filtered_ccflags_lists.append(inc)

    return filtered_ccflags_lists

"""
Parse 'ccflags' from ninja build file and print
"""
def Parse_CCflags(contents):
    regex = re.compile(r'\bcflags_cc\b\s*=.*\n', re.M)
    sp = re.compile(r'[\s$]')

    matched_ = regex.search(contents)
    if matched_ == None:
        sys.stderr.write('failed to match option define\n')

    splited = sp.split(matched_.group(0))
    splited = filter(lambda a: a != '', splited)
    splited.pop(0)   # remove define word
    splited.pop(0)   # remove = character
    # running include fillter
    splited = Fiter_CCflags(splited)
    print('%s' % " ".join(str(x) for x in splited))


"""
Parse 'cflags' from ninja build file and print
"""
def Parse_Cflags(contents):
    regex = re.compile(r'\bcflags\b\s*=.*\n', re.M)
    sp = re.compile(r'[\s$]')

    matched_ = regex.search(contents)
    if matched_ == None:
        sys.stderr.write('failed to match option define\n')

    splited = sp.split(matched_.group(0))
    splited = filter(lambda a: a != '', splited)
    splited.pop(0)   # remove define word
    splited.pop(0)   # remove = character
    splited = Fiter_CCflags(splited)
    print('%s' % " ".join(str(x) for x in splited))



"""
Fiter 'include_dirs' from includes in ninja build
"""
def Fiter_Includes(include_lists):
    """
    """
    filtered_include_lists =  []
    """
    base filtering and rule matching patterns
    """
    include_dotdot_str = "-I.."
    gen_str = "-Igen"
    testing_str = "testing"
    gtest_str = "gtest"
    """
    excluding list definition
    """
    exclude_pattern=r'gtk|atk|pango|cairo|pixmap|freetype|pixbuf|harfbuzz|pixman|png12'

    for index,inc in enumerate(include_lists):
        #print("Index : %d, Include: %s, Build %s" % (index, inc, webrtc_build_path ))
        ### filtering rules for webrtc includes
        if any(re.findall(exclude_pattern, inc, re.IGNORECASE)):
            continue
        if inc.find(testing_str) != -1:
            continue
        if inc.find(gtest_str) != -1:
            continue
        ### replacing rules
        if inc.find(gen_str) != -1:
            gen_dir = inc[len("-I"):]   # include 'gen' string in dirname
            gen_abspath = generate_AbsolutePath(gen_dir)
            filtered_include_lists.append("-I" + gen_abspath)
            continue
        if inc.find(include_dotdot_str) != -1:
            include_dir = inc[len(include_dotdot_str):]
            include_abspath = generate_AbsolutePath(include_dir)
            filtered_include_lists.append("-I" + include_abspath)
            continue
        ### ending rule
        filtered_include_lists.append(inc)

    return filtered_include_lists

"""
Parse 'include_dirs' from ninja build file and print
"""
def Parse_Includes(contents):
    regex = re.compile(r'\binclude_dirs\b\s*=.*\n', re.M)
    sp = re.compile(r'[\s\$]')

    matched_ = regex.search(contents)
    if matched_ == None:
        sys.stderr.write('failed to match option define\n')

    splited = sp.split(matched_.group(0))
    splited = filter(lambda a: a != '', splited)
    splited.pop(0)   # remove define word
    splited.pop(0)   # remove = character
    # running include fillter
    splited = Fiter_Includes(splited)
    print('%s' % " ".join(str(x) for x in splited))

"""
Parse 'libs' from ninja build file and print
"""
def Parse_Libs(contents):
    # build peerconnection_client: link $
    regex = re.compile(r'build ./peerconnection_client: link .*\n', re.M)
    sp = re.compile(r'[\s\$]')
    libs = re.compile(r'.\.[ao]$')

    matched_ = regex.search(contents)
    if matched_ == None:
        sys.stderr.write('failed to match option libs\n')

    splited = sp.split(matched_.group(0))
    splited = filter(lambda a: libs.search(a), splited)

    splited.pop(0)   # remove peerconnection_client object
    splited.pop(0)   # remove peerconnection_client object
    splited.pop(0)   # remove peerconnection_client object
    splited.pop(0)   # remove peerconnection_client object
    splited.pop(0)   # remove peerconnection_client object

    print('%s' % " ".join(os.path.abspath(webrtc_build_path+'/'+x) for x in splited))


"""
Parse 'ldflags' from ninja build file and print
"""
def Parse_Ldflags(contents):
    regex = re.compile(r'\bldflags\b\s*=.*\n', re.M)
    sp = re.compile(r'[\s$]')

    ## exclude_patern
    exclude_patterns=r'-B'

    matched_ = regex.search(contents)
    if matched_ == None:
        sys.stderr.write('failed to match option define\n')

    splited = sp.split(matched_.group(0))
    splited = filter(lambda a: a != '', splited)
    splited.pop(0)   # remove define word
    splited.pop(0)   # remove = character
    # running include fillter
    splited = Fiter_ExcludeFilter(splited, exclude_patterns)
    print('%s' % " ".join(str(x) for x in splited))


"""
Parse 'syslibs' from ninja build file and print for webrtc peerconnection libs
"""
def Parse_Syslibs(contents):
    regex = re.compile(r'\blibs\b\s*=.*\n', re.M)
    sp = re.compile(r'[\s$]')

    # syslib exclude patterns
    exclude_patterns= r'X11|Xcomposite|Xext|Xrender|gmodule|gtk|gio|gdk|\
            gobjects|gthread|glib|gobject|fontconfig|atk|pango|cairo|freetype|'

    matched_ = regex.search(contents)
    if matched_ == None:
        sys.stderr.write('failed to match option define\n')

    splited = sp.split(matched_.group(0))
    splited = filter(lambda a: a != '', splited)
    splited.pop(0)   # remove define word
    splited.pop(0)   # remove = character
    # running include fillter
    splited = Fiter_ExcludeFilter(splited, exclude_patterns)
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
    elif option == 'ldflags':
        Parse_Ldflags(ninja_file_contents)
    elif option == 'syslibs':
        Parse_Syslibs(ninja_file_contents)
    else:
        sys.stderr.write('undefined options : ' + option + ' specified!\n')
        return

def Main(argv):
    if len(argv) != 4:
        sys.stderr.write('Ninja build file parser for extracting compile and build options\n')
        sys.stderr.write('Usage: ' + argv[0] + ' <webrtc_root> <build_name> <options>\n')
        sys.stderr.write('  webrtc_root : home of WebRTC source tree\n')
        sys.stderr.write('  build_name  : $(webrtc_root)/src/build_name\n')
        sys.stderr.write('  options     : libs - extract the list of webrtc library\n')
        sys.stderr.write('                defines - extract defines options\n')
        sys.stderr.write('                includes - extract include opttions \n')
        sys.stderr.write('                ccflags - extract ccflags for C++ options\n')
        sys.stderr.write('                cflags - extract cflags for C options\n')
        sys.stderr.write('                ldflags - extract ldflags for linker options\n')
        sys.stderr.write('                syslibs - extract system library list for linker options\n')
        return 1

    global search_path, webrtc_build_path, webrtc_root_path, peerconnection_ninja_path
    search_path = os.path.normpath(argv[1])
    webrtc_build_path = search_path + '/src/' + argv[2]
    target_file = webrtc_build_path + peerconnection_ninja_file
    webrtc_root_path = search_path + '/src/'
    peerconnection_ninja_path = os.path.dirname(os.path.abspath(target_file))

    if not os.path.exists(webrtc_build_path):
        sys.stderr.write('webrtc build path does not exist: %s\n' % webrtc_build_path)
        return 1

    if not os.path.exists(target_file):
        sys.stderr.write('peerconnection ninja does not exist: %s\n' % target_file)
        return 1

    ParseNinjaFile(target_file, argv[3])


if __name__ == '__main__':
  sys.exit(Main(sys.argv))
