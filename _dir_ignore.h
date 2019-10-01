//  _dir_ignore.h
//  hdiffz
//  Created by housisong on 2019-09-30.
//
/*
 The MIT License (MIT)
 Copyright (c) 2012-2019 HouSisong
 
 Permission is hereby granted, free of charge, to any person
 obtaining a copy of this software and associated documentation
 files (the "Software"), to deal in the Software without
 restriction, including without limitation the rights to use,
 copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the
 Software is furnished to do so, subject to the following
 conditions:
 
 The above copyright notice and this permission notice shall be
 included in all copies of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef _dir_ignore_h
#define _dir_ignore_h
#include "dirDiffPatch/dir_patch/dir_patch.h"

#if (_IS_NEED_DIR_DIFF_PATCH)
static void formatIgnorePathName(std::string& path_utf8){
    for (size_t i=0;i<path_utf8.size();++i) {
        char c=path_utf8[i];
        if ((c=='\\')||(c=='/'))
            path_utf8[i]=kPatch_dirSeparator;
#ifdef _WIN32
        else if (isascii(c))
            path_utf8[i]=(char)tolower(c);
#endif
    }
}

#ifdef _WIN32
static const char kIgnoreMagicChar = '?';
#else
static const char kIgnoreMagicChar = ':';
#endif
static void _formatIgnorePathSet(std::string& path_utf8){
    formatIgnorePathName(path_utf8);
    size_t insert=0;
    size_t i=0;
    while (i<path_utf8.size()) {
        char c=path_utf8[i];
        if (c=='*'){
            if ((i+1<path_utf8.size())&&(path_utf8[i+1]==':')){ // *: as *
                path_utf8[insert++]=c; i+=2; //skip *:
            }else{
                path_utf8[insert++]=kIgnoreMagicChar; ++i; //skip *
            }
        }else{
            path_utf8[insert++]=c; ++i;  //skip c
        }
    }
    path_utf8.resize(insert);
}

static hpatch_BOOL _getIgnorePathSetList(std::vector<std::string>& out_pathList,const char* plist){
    std::string cur;
    while (true) {
        char c=*plist;
        if ((c=='#')&&(plist[1]==':')){ // #: as #
            cur.push_back(c); plist+=2; //skip #:
        }else if ((c=='*')&&(plist[1]==':')){ // *: as *:
            cur.push_back(c); cur.push_back(':'); plist+=2; //skip *:
        }else if ((c=='\0')||((c=='#')&&(plist[1]!=':'))){
            if (cur.empty()) return hpatch_FALSE;// can't empty
            if (std::string::npos!=cur.find("**")) return hpatch_FALSE;// can't **
            _formatIgnorePathSet(cur);
            out_pathList.push_back(cur);
            if (c=='\0') return hpatch_TRUE;
            cur.clear();  ++plist; //skip #
        }else if (c==kIgnoreMagicChar){
            return hpatch_FALSE; //error path char
        }else{
            cur.push_back(c); ++plist; //skip c
        }
    }
}
#endif

#endif // dir_ignore_h
