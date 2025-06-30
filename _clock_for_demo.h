//_clock_for_demo.h
// double clock_s()
/*
 The MIT License (MIT)
 Copyright (c) 2012-2017 HouSisong
 
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
#ifndef HDiffPatch_clock_h
#define HDiffPatch_clock_h

//  #include <time.h>
//  static double clock_s(){ return clock()*1.0/CLOCKS_PER_SEC; }
#ifdef _WIN32
    #include <windows.h>
    static double clock_s(){
        LARGE_INTEGER f;
        if (QueryPerformanceFrequency(&f)){
            LARGE_INTEGER c;
            QueryPerformanceCounter(&c);
            return c.QuadPart/((double)f.QuadPart);
        }
        return GetTickCount64()/1000.0; 
    }
#else
    //Unix-like system
    #include <sys/time.h>
    #include <assert.h>
    static double clock_s(){
        struct timeval t={0,0};
        int ret=gettimeofday(&t,0);
        assert(ret==0);
        if (ret==0)
            return t.tv_sec + t.tv_usec/1000000.0;
        else
            return 0;
    }
#endif

#endif //HDiffPatch_clock_h