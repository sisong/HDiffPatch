//
//  patch.h
//  HPatch
//
/*
 This is the HPatch copyright.
 
 Copyright (c) 2012-2013 HouSisong All Rights Reserved.
 
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

#ifndef HPatch_patch_h
#define HPatch_patch_h

#ifdef __cplusplus
extern "C"
{
#endif
    
#define hpatch_BOOL   int
#define hpatch_FALSE  0
//#define hpatch_TRUE   (!hpatch_FALSE)
    
hpatch_BOOL patch(unsigned char* newData,unsigned char* newData_end,
            const unsigned char* oldData,const unsigned char* oldData_end,
            const unsigned char* serializedDiff,const unsigned char* serializedDiff_end);
    
#ifdef __cplusplus
}
#endif
    
    
#endif
