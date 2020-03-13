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
#include <vector>
#include <string>
#include <algorithm>
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
static const char _private_kIgnoreMagicChar = '?';  //a char not used in path name on WIN32
#else
static const char _private_kIgnoreMagicChar = ':';  //a char not used in path name
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
                path_utf8[insert++]=_private_kIgnoreMagicChar; ++i; //skip *
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
        }else if (c==_private_kIgnoreMagicChar){
            return hpatch_FALSE; //error path char
        }else{
            cur.push_back(c); ++plist; //skip c
        }
    }
}

static bool _matchIgnore(const char* beginS,const char* endS,
                         const std::vector<const char*>& matchs,size_t mi,
                         const char* ignoreBegin,const char* ignoreEnd){
    //O(n*n) !
    const char* match   =matchs[mi];
    const char* matchEnd=matchs[mi+1];
    const char* curS=beginS;
    while (curS<endS){
        const char* found=std::search(curS,endS,match,matchEnd);
        if (found==endS) return false;
        bool isMatched=true;
        //check front
        if (beginS<found){
            if (mi>0){ //[front match]*[cur match]
                for (const char* it=beginS;it<found; ++it) {
                    if ((*it)==kPatch_dirSeparator) { isMatched=false; break; }
                }
            }else{ // ?[first match]
                if ((match==ignoreBegin)&&(match[0]!=kPatch_dirSeparator)&&(found[-1]!=kPatch_dirSeparator))
                    isMatched=false;
            }
        }
        const char* foundEnd=found+(matchEnd-match);
        //check back
        if (isMatched && (mi+2>=matchs.size()) && (foundEnd<endS)){ //[last match]
            if ((matchEnd==ignoreEnd)&&(matchEnd[-1]!=kPatch_dirSeparator)&&(foundEnd[0]!=kPatch_dirSeparator))
                isMatched=false;
        }
        if (isMatched && (mi+2<matchs.size())
            && (!_matchIgnore(foundEnd,endS,matchs,mi+2,ignoreBegin,ignoreEnd)))
            isMatched=false;
        if (isMatched) return true;
        curS=found+1;//continue
    }
    return false;
}

static bool isMatchIgnore(const std::string& subPath,const std::string& ignore){
    assert(!ignore.empty());
    std::vector<const char*> matchs;
    const char* beginI=ignore.c_str();
    const char* endI=beginI+ignore.size();
    const char* curI=beginI;
    while (curI<endI) {
        const char* clip=std::find(curI,endI,_private_kIgnoreMagicChar);
        if (curI<clip){
            matchs.push_back(curI);
            matchs.push_back(clip);
        }
        curI=clip+1;
    }
    if (matchs.empty()) return true; // WARNING : match any path
    const char* beginS=subPath.c_str();
    const char* endS=beginS+subPath.size();
    return _matchIgnore(beginS,endS,matchs,0,beginI,endI);
}

static inline bool isMatchIgnoreList(const std::string& subPath,const std::vector<std::string>& ignoreList){
    for (size_t i=0; i<ignoreList.size(); ++i) {
        if (isMatchIgnore(subPath,ignoreList[i])) return true;
    }
    return false;
}

struct CDirPathIgnore{
    CDirPathIgnore(const std::vector<std::string>& ignorePathListBase,
                   const std::vector<std::string>& ignorePathList,bool isPrintIgnore)
    :_ignorePathListBase(ignorePathListBase),_ignorePathList(ignorePathList),
    _isPrintIgnore(isPrintIgnore),_ignoreCount(0){ }
    CDirPathIgnore(const std::vector<std::string>& ignorePathList,bool isPrintIgnore)
    :_ignorePathListBase(ignorePathList),_ignorePathList(ignorePathList),
    _isPrintIgnore(isPrintIgnore),_ignoreCount(0){ }
    bool isNeedIgnore(const std::string& path,size_t rootPathNameLen){
        std::string subPath(path.begin()+rootPathNameLen,path.end());
        formatIgnorePathName(subPath);
        bool result=   isMatchIgnoreList(subPath,_ignorePathListBase);
        if ((!result)&&(&_ignorePathListBase!=&_ignorePathList))
            result=isMatchIgnoreList(subPath,_ignorePathList);
        if (result) ++_ignoreCount;
        if (result&&_isPrintIgnore){ //printf
            printf("  ignore file : \"");
            hpatch_printPath_utf8(path.c_str());  printf("\"\n");
        }
        return result;
    }
    inline size_t ignoreCount()const{ return _ignoreCount; }
private:
    const std::vector<std::string>& _ignorePathListBase;
    const std::vector<std::string>& _ignorePathList;
    const bool                      _isPrintIgnore;
    size_t                          _ignoreCount;
};
#endif

#endif // dir_ignore_h
