/*
 * sais.hxx for sais-lite
 * Copyright (c) 2008-2010 Yuta Mori All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _SAIS_HXX
#define _SAIS_HXX 1
#ifdef __cplusplus

#include <cassert>
#include <iterator>
#include <limits>

#ifdef __INTEL_COMPILER
#pragma warning(disable : 383 981 1418)
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4365)
#endif

namespace saisxx_private {

/* find the start or end of each bucket */
template<typename string_type, typename bucket_type, typename index_type>
void
getCounts(const string_type T, bucket_type C, index_type n, index_type k) {
  index_type i;
  for(i = 0; i < k; ++i) { C[i] = 0; }
  for(i = 0; i < n; ++i) { ++C[T[i]]; }
}
template<typename bucketC_type, typename bucketB_type, typename index_type>
void
getBuckets(const bucketC_type C, bucketB_type B, index_type k, bool end) {
  index_type i, sum = 0;
  if(end != false) { for(i = 0; i < k; ++i) { sum += C[i]; B[i] = sum; } }
  else { for(i = 0; i < k; ++i) { sum += C[i]; B[i] = sum - C[i]; } }
}

template<typename string_type, typename sarray_type,
         typename bucketC_type, typename bucketB_type, typename index_type>
void
LMSsort1(string_type T, sarray_type SA,
         bucketC_type C, bucketB_type B,
         index_type n, index_type k, bool recount) {
typedef typename std::iterator_traits<string_type>::value_type char_type;
  sarray_type b;
  index_type i, j;
  char_type c0, c1;

  /* compute SAl */
  if(recount != false) { getCounts(T, C, n, k); }
  getBuckets(C, B, k, false); /* find starts of buckets */
  j = n - 1;
  b = SA + B[c1 = T[j]];
  --j;
  *b++ = (T[j] < c1) ? ~j : j;
  for(i = 0; i < n; ++i) {
    if(0 < (j = SA[i])) {
      assert(T[j] >= T[j + 1]);
      if((c0 = T[j]) != c1) { B[c1] = (index_type)(b - SA); b = SA + B[c1 = c0]; }
      assert(i < (b - SA));
      --j;
      *b++ = (T[j] < c1) ? ~j : j;
      SA[i] = 0;
    } else if(j < 0) {
      SA[i] = ~j;
    }
  }
  /* compute SAs */
  if(recount != false) { getCounts(T, C, n, k); }
  getBuckets(C, B, k, true); /* find ends of buckets */
  for(i = n - 1, b = SA + B[c1 = 0]; 0 <= i; --i) {
    if(0 < (j = SA[i])) {
      assert(T[j] <= T[j + 1]);
      if((c0 = T[j]) != c1) { B[c1] = (index_type)(b - SA); b = SA + B[c1 = c0]; }
      assert((b - SA) <= i);
      --j;
      *--b = (T[j] > c1) ? ~(j + 1) : j;
      SA[i] = 0;
    }
  }
}
template<typename string_type, typename sarray_type, typename index_type>
index_type
LMSpostproc1(string_type T, sarray_type SA, index_type n, index_type m) {
typedef typename std::iterator_traits<string_type>::value_type char_type;
  index_type i, j, p, q, plen, qlen, name;
  char_type c0, c1;
  bool diff;

  /* compact all the sorted substrings into the first m items of SA
      2*m must be not larger than n (proveable) */
  assert(0 < n);
  for(i = 0; (p = SA[i]) < 0; ++i) { SA[i] = ~p; assert((i + 1) < n); }
  if(i < m) {
    for(j = i, ++i;; ++i) {
      assert(i < n);
      if((p = SA[i]) < 0) {
        SA[j++] = ~p; SA[i] = 0;
        if(j == m) { break; }
      }
    }
  }

  /* store the length of all substrings */
  i = n - 1; j = n - 1; c0 = T[n - 1];
  do { c1 = c0; } while((0 <= --i) && ((c0 = T[i]) >= c1));
  for(; 0 <= i;) {
    do { c1 = c0; } while((0 <= --i) && ((c0 = T[i]) <= c1));
    if(0 <= i) {
      SA[m + ((i + 1) >> 1)] = j - i; j = i + 1;
      do { c1 = c0; } while((0 <= --i) && ((c0 = T[i]) >= c1));
    }
  }

  /* find the lexicographic names of all substrings */
  for(i = 0, name = 0, q = n, qlen = 0; i < m; ++i) {
    p = SA[i], plen = SA[m + (p >> 1)], diff = true;
    if((plen == qlen) && ((q + plen) < n)) {
      for(j = 0; (j < plen) && (T[p + j] == T[q + j]); ++j) { }
      if(j == plen) { diff = false; }
    }
    if(diff != false) { ++name, q = p, qlen = plen; }
    SA[m + (p >> 1)] = name;
  }

  return name;
}
template<typename string_type, typename sarray_type,
         typename bucketC_type, typename bucketB_type, typename bucketD_type,
         typename index_type>
void
LMSsort2(string_type T, sarray_type SA,
         bucketC_type C, bucketB_type B, bucketD_type D,
         index_type n, index_type k) {
typedef typename std::iterator_traits<string_type>::value_type char_type;
  sarray_type b;
  index_type i, j, t, d;
  char_type c0, c1;

  /* compute SAl */
  getBuckets(C, B, k, false); /* find starts of buckets */
  j = n - 1;
  b = SA + B[c1 = T[j]];
  --j;
  t = (T[j] < c1);
  j += n;
  *b++ = (t & 1) ? ~j : j;
  for(i = 0, d = 0; i < n; ++i) {
    if(0 < (j = SA[i])) {
      if(n <= j) { d += 1; j -= n; }
      assert(T[j] >= T[j + 1]);
      if((c0 = T[j]) != c1) { B[c1] = (index_type)(b - SA); b = SA + B[c1 = c0]; }
      assert(i < (b - SA));
      --j;
      t = c0; t = (t << 1) | (T[j] < c1);
      if(D[t] != d) { j += n; D[t] = d; }
      *b++ = (t & 1) ? ~j : j;
      SA[i] = 0;
    } else if(j < 0) {
      SA[i] = ~j;
    }
  }
  for(i = n - 1; 0 <= i; --i) {
    if(0 < SA[i]) {
      if(SA[i] < n) {
        SA[i] += n;
        for(j = i - 1; SA[j] < n; --j) { }
        SA[j] -= n;
        i = j;
      }
    }
  }

  /* compute SAs */
  getBuckets(C, B, k, true); /* find ends of buckets */
  for(i = n - 1, d += 1, b = SA + B[c1 = 0]; 0 <= i; --i) {
    if(0 < (j = SA[i])) {
      if(n <= j) { d += 1; j -= n; }
      assert(T[j] <= T[j + 1]);
      if((c0 = T[j]) != c1) { B[c1] = (index_type)(b - SA); b = SA + B[c1 = c0]; }
      assert((b - SA) <= i);
      --j;
      t = c0; t = (t << 1) | (T[j] > c1);
      if(D[t] != d) { j += n; D[t] = d; }
      *--b = (t & 1) ? ~(j + 1) : j;
      SA[i] = 0;
    }
  }
}
template<typename sarray_type, typename index_type>
index_type
LMSpostproc2(sarray_type SA, index_type n, index_type m) {
  index_type i, j, d, name;

  /* compact all the sorted LMS substrings into the first m items of SA */
  assert(0 < n);
  for(i = 0, name = 0; (j = SA[i]) < 0; ++i) {
    j = ~j;
    if(n <= j) { name += 1; }
    SA[i] = j;
    assert((i + 1) < n);
  }
  if(i < m) {
    for(d = i, ++i;; ++i) {
      assert(i < n);
      if((j = SA[i]) < 0) {
        j = ~j;
        if(n <= j) { name += 1; }
        SA[d++] = j; SA[i] = 0;
        if(d == m) { break; }
      }
    }
  }
  if(name < m) {
    /* store the lexicographic names */
    for(i = m - 1, d = name + 1; 0 <= i; --i) {
      if(n <= (j = SA[i])) { j -= n; --d; }
      SA[m + (j >> 1)] = d;
    }
  } else {
    /* unset flags */
    for(i = 0; i < m; ++i) {
      if(n <= (j = SA[i])) { j -= n; SA[i] = j; }
    }
  }

  return name;
}

/* compute SA and BWT */
template<typename string_type, typename sarray_type,
         typename bucketC_type, typename bucketB_type, typename index_type>
void
induceSA(string_type T, sarray_type SA, bucketC_type C, bucketB_type B,
         index_type n, index_type k, bool recount) {
typedef typename std::iterator_traits<string_type>::value_type char_type;
  sarray_type b;
  index_type i, j;
  char_type c0, c1;
  /* compute SAl */
  if(recount != false) { getCounts(T, C, n, k); }
  getBuckets(C, B, k, false); /* find starts of buckets */
  b = SA + B[c1 = T[j = n - 1]];
  *b++ = ((0 < j) && (T[j - 1] < c1)) ? ~j : j;
  for(i = 0; i < n; ++i) {
    j = SA[i], SA[i] = ~j;
    if(0 < j) {
      if((c0 = T[--j]) != c1) { B[c1] = (index_type)(b - SA); b = SA + B[c1 = c0]; }
      *b++ = ((0 < j) && (T[j - 1] < c1)) ? ~j : j;
    }
  }
  /* compute SAs */
  if(recount != false) { getCounts(T, C, n, k); }
  getBuckets(C, B, k, true); /* find ends of buckets */
  for(i = n - 1, b = SA + B[c1 = 0]; 0 <= i; --i) {
    if(0 < (j = SA[i])) {
      if((c0 = T[--j]) != c1) { B[c1] = (index_type)(b - SA); b = SA + B[c1 = c0]; }
      *--b = ((j == 0) || (T[j - 1] > c1)) ? ~j : j;
    } else {
      SA[i] = ~j;
    }
  }
}
template<typename string_type, typename sarray_type,
         typename bucketC_type, typename bucketB_type, typename index_type>
index_type
computeBWT(string_type T, sarray_type SA, bucketC_type C, bucketB_type B,
           index_type n, index_type k, bool recount) {
typedef typename std::iterator_traits<string_type>::value_type char_type;
  sarray_type b;
  index_type i, j, pidx = -1;
  char_type c0, c1;
  /* compute SAl */
  if(recount != false) { getCounts(T, C, n, k); }
  getBuckets(C, B, k, false); /* find starts of buckets */
  b = SA + B[c1 = T[j = n - 1]];
  *b++ = ((0 < j) && (T[j - 1] < c1)) ? ~j : j;
  for(i = 0; i < n; ++i) {
    if(0 < (j = SA[i])) {
      SA[i] = ~((index_type)(c0 = T[--j]));
      if(c0 != c1) { B[c1] = (index_type)(b - SA); b = SA + B[c1 = c0]; }
      *b++ = ((0 < j) && (T[j - 1] < c1)) ? ~j : j;
    } else if(j != 0) {
      SA[i] = ~j;
    }
  }
  /* compute SAs */
  if(recount != false) { getCounts(T, C, n, k); }
  getBuckets(C, B, k, true); /* find ends of buckets */
  for(i = n - 1, b = SA + B[c1 = 0]; 0 <= i; --i) {
    if(0 < (j = SA[i])) {
      SA[i] = (c0 = T[--j]);
      if(c0 != c1) { B[c1] = (index_type)(b - SA); b = SA + B[c1 = c0]; }
      *--b = ((0 < j) && (T[j - 1] > c1)) ? ~((index_type)T[j - 1]) : j;
    } else if(j != 0) {
      SA[i] = ~j;
    } else {
      pidx = i;
    }
  }
  return pidx;
}

template<typename string_type, typename sarray_type,
         typename bucketC_type, typename bucketB_type,
         typename index_type>
std::pair<index_type, index_type>
stage1sort(string_type T, sarray_type SA,
           bucketC_type C, bucketB_type B,
           index_type n, index_type k, unsigned flags) {
typedef typename std::iterator_traits<string_type>::value_type char_type;
  sarray_type b;
  index_type i, j, name, m;
  char_type c0, c1;
  getCounts(T, C, n, k); getBuckets(C, B, k, true); /* find ends of buckets */
  for(i = 0; i < n; ++i) { SA[i] = 0; }
  b = SA + n - 1; i = n - 1; j = n; m = 0; c0 = T[n - 1];
  do { c1 = c0; } while((0 <= --i) && ((c0 = T[i]) >= c1));
  for(; 0 <= i;) {
    do { c1 = c0; } while((0 <= --i) && ((c0 = T[i]) <= c1));
    if(0 <= i) {
      *b = j; b = SA + --B[c1]; j = i; ++m; assert(B[c1] != (n - 1));
      do { c1 = c0; } while((0 <= --i) && ((c0 = T[i]) >= c1));
    }
  }
  SA[n - 1] = 0;

  if(1 < m) {
    if(flags & (16 | 32)) {
      assert((j + 1) < n);
      ++B[T[j + 1]];
      if(flags & 16) {
        index_type *D;
        try { D = new index_type[k * 2]; } catch(...) { D = 0; }
        if(D == 0) { return std::make_pair(-2, -2); }
        for(i = 0, j = 0; i < k; ++i) {
          j += C[i];
          if(B[i] != j) { assert(SA[B[i]] != 0); SA[B[i]] += n; }
          D[i] = D[i + k] = 0;
        }
        LMSsort2(T, SA, C, B, D, n, k);
        delete[] D;
      } else {
        bucketB_type D = B - k * 2;
        for(i = 0, j = 0; i < k; ++i) {
          j += C[i];
          if(B[i] != j) { assert(SA[B[i]] != 0); SA[B[i]] += n; }
          D[i] = D[i + k] = 0;
        }
        LMSsort2(T, SA, C, B, D, n, k);
      }
      name = LMSpostproc2(SA, n, m);
    } else {
      LMSsort1(T, SA, C, B, n, k, (flags & (4 | 64)) != 0);
      name = LMSpostproc1(T, SA, n, m);
    }
  } else if(m == 1) {
    *b = j + 1;
    name = 1;
  } else {
    name = 0;
  }
  return std::make_pair(m, name);
}
template<typename string_type, typename sarray_type,
         typename bucketC_type, typename bucketB_type, typename index_type>
index_type
stage3sort(string_type T, sarray_type SA, bucketC_type C, bucketB_type B,
           index_type n, index_type m, index_type k,
           unsigned flags, bool isbwt) {
typedef typename std::iterator_traits<string_type>::value_type char_type;
  index_type i, j, p, q, pidx = 0;
  char_type c0, c1;
  if((flags & 8) != 0) { getCounts(T, C, n, k); }
  /* put all left-most S characters into their buckets */
  if(1 < m) {
    getBuckets(C, B, k, 1); /* find ends of buckets */
    i = m - 1, j = n, p = SA[m - 1], c1 = T[p];
    do {
      q = B[c0 = c1];
      while(q < j) { SA[--j] = 0; }
      do {
        SA[--j] = p;
        if(--i < 0) { break; }
        p = SA[i];
      } while((c1 = T[p]) == c0);
    } while(0 <= i);
    while(0 < j) { SA[--j] = 0; }
  }
  if(isbwt == false) { induceSA(T, SA, C, B, n, k, (flags & (4 | 64)) != 0); }
  else { pidx = computeBWT(T, SA, C, B, n, k, (flags & (4 | 64)) != 0); }
  return pidx;
}

/* find the suffix array SA of T[0..n-1] in {0..k}^n
   use a working space (excluding s and SA) of at most 2n+O(1) for a constant alphabet */
template<typename string_type, typename sarray_type, typename index_type>
index_type
suffixsort(string_type T, sarray_type SA,
           index_type fs, index_type n, index_type k,
           bool isbwt) {
typedef typename std::iterator_traits<string_type>::value_type char_type;
  sarray_type RA, C, B;
  index_type *Cp, *Bp;
  index_type i, j, m, name, pidx, newfs;
  unsigned flags = 0;
  char_type c0, c1;

  /* stage 1: reduce the problem by at least 1/2
     sort all the S-substrings */
  C = B = SA; /* for warnings */
  Cp = 0, Bp = 0;
  if(k <= 256) {
    try { Cp = new index_type[k]; } catch(...) { Cp = 0; }
    if(Cp == 0) { return -2; }
    if(k <= fs) {
      B = SA + (n + fs - k);
      flags = 1;
    } else {
      try { Bp = new index_type[k]; } catch(...) { Bp = 0; }
      if(Bp == 0) { return -2; }
      flags = 3;
    }
  } else if(k <= fs) {
    C = SA + (n + fs - k);
    if(k <= (fs - k)) {
      B = C - k;
      flags = 0;
    } else if(k <= 1024) {
      try { Bp = new index_type[k]; } catch(...) { Bp = 0; }
      if(Bp == 0) { return -2; }
      flags = 2;
    } else {
      B = C;
      flags = 64 | 8;
    }
  } else {
    try { Cp = new index_type[k]; } catch(...) { Cp = 0; }
    if(Cp == 0) { return -2; }
    Bp = Cp;
    flags = 4 | 8;
  }
  if((n <= ((std::numeric_limits<index_type>::max)() / 2)) && (2 <= (n / k))) {
    if(flags & 1) { flags |= ((k * 2) <= (fs - k)) ? 32 : 16; }
    else if((flags == 0) && ((k * 2) <= (fs - k * 2))) { flags |= 32; }
  }
  {
    std::pair<index_type, index_type> r;
    if(Cp != 0) {
      if(Bp != 0) { r = stage1sort(T, SA, Cp, Bp, n, k, flags); }
      else { r = stage1sort(T, SA, Cp, B, n, k, flags); }
    } else {
      if(Bp != 0) { r = stage1sort(T, SA, C, Bp, n, k, flags); }
      else { r = stage1sort(T, SA, C, B, n, k, flags); }
    }
    m = r.first, name = r.second;
  }
  if(m < 0) {
    if(flags & (1 | 4)) { delete[] Cp; }
    if(flags & 2) { delete[] Bp; }
    return -2;
  }

  /* stage 2: solve the reduced problem
     recurse if names are not yet unique */
  if(name < m) {
    if(flags & 4) { delete[] Cp; }
    if(flags & 2) { delete[] Bp; }
    newfs = (n + fs) - (m * 2);
    if((flags & (1 | 4 | 64)) == 0) {
      if((k + name) <= newfs) { newfs -= k; }
      else { flags |= 8; }
    }
    assert((n >> 1) <= (newfs + m));
    RA = SA + m + newfs;
    for(i = m + (n >> 1) - 1, j = m - 1; m <= i; --i) {
      if(SA[i] != 0) { RA[j--] = SA[i] - 1; }
    }
    if(suffixsort(RA, SA, newfs, m, name, false) != 0) { if(flags & 1) { delete[] Cp; } return -2; }
    i = n - 1; j = m - 1; c0 = T[n - 1];
    do { c1 = c0; } while((0 <= --i) && ((c0 = T[i]) >= c1));
    for(; 0 <= i;) {
      do { c1 = c0; } while((0 <= --i) && ((c0 = T[i]) <= c1));
      if(0 <= i) {
        RA[j--] = i + 1;
        do { c1 = c0; } while((0 <= --i) && ((c0 = T[i]) >= c1));
      }
    }
    for(i = 0; i < m; ++i) { SA[i] = RA[SA[i]]; }
    if(flags & 4) {
      try { Cp = new index_type[k]; } catch(...) { Cp = 0; }
      if(Cp == 0) { return -2; }
      Bp = Cp;
    }
    if(flags & 2) {
      try { Bp = new index_type[k]; } catch(...) { Bp = 0; }
      if(Bp == 0) { if(flags & 1) { delete[] Cp; } return -2; }
    }
  }

  /* stage 3: induce the result for the original problem */
  if(Cp != 0) {
    if(Bp != 0) { pidx = stage3sort(T, SA, Cp, Bp, n, m, k, flags, isbwt); }
    else { pidx = stage3sort(T, SA, Cp, B, n, m, k, flags, isbwt); }
  } else {
    if(Bp != 0) { pidx = stage3sort(T, SA, C, Bp, n, m, k, flags, isbwt); }
    else { pidx = stage3sort(T, SA, C, B, n, m, k, flags, isbwt); }
  }
  if(flags & (1 | 4)) { delete[] Cp; }
  if(flags & 2) { delete[] Bp; }

  return pidx;
}

} /* namespace saisxx_private */


/**
 * @brief Constructs the suffix array of a given string in linear time.
 * @param T[0..n-1] The input string. (random access iterator)
 * @param SA[0..n-1] The output array of suffixes. (random access iterator)
 * @param n The length of the given string.
 * @param k The alphabet size.
 * @return 0 if no error occurred, -1 or -2 otherwise.
 */
template<typename string_type, typename sarray_type, typename index_type>
index_type
saisxx(string_type T, sarray_type SA, index_type n, index_type k = 256) {
typedef typename std::iterator_traits<sarray_type>::value_type savalue_type;
  assert((std::numeric_limits<index_type>::min)() < 0);
  assert((std::numeric_limits<savalue_type>::min)() < 0);
  assert((std::numeric_limits<savalue_type>::max)() == (std::numeric_limits<index_type>::max)());
  assert((std::numeric_limits<savalue_type>::min)() == (std::numeric_limits<index_type>::min)());
  if((n < 0) || (k <= 0)) { return -1; }
  if(n <= 1) { if(n == 1) { SA[0] = 0; } return 0; }
  return saisxx_private::suffixsort(T, SA, (index_type)0, n, k, false);
}

/**
 * @brief Constructs the burrows-wheeler transformed string of a given string in linear time.
 * @param T[0..n-1] The input string. (random access iterator)
 * @param U[0..n-1] The output string. (random access iterator)
 * @param A[0..n-1] The temporary array. (random access iterator)
 * @param n The length of the given string.
 * @param k The alphabet size.
 * @return The primary index if no error occurred, -1 or -2 otherwise.
 */
template<typename string_type, typename sarray_type, typename index_type>
index_type
saisxx_bwt(string_type T, string_type U, sarray_type A, index_type n, index_type k = 256) {
typedef typename std::iterator_traits<sarray_type>::value_type savalue_type;
typedef typename std::iterator_traits<string_type>::value_type char_type;
  index_type i, pidx;
  assert((std::numeric_limits<index_type>::min)() < 0);
  assert((std::numeric_limits<savalue_type>::min)() < 0);
  assert((std::numeric_limits<savalue_type>::max)() == (std::numeric_limits<index_type>::max)());
  assert((std::numeric_limits<savalue_type>::min)() == (std::numeric_limits<index_type>::min)());
  if((n < 0) || (k <= 0)) { return -1; }
  if(n <= 1) { if(n == 1) { U[0] = T[0]; } return n; }
  pidx = saisxx_private::suffixsort(T, A, 0, n, k, true);
  if(0 <= pidx) {
    U[0] = T[n - 1];
    for(i = 0; i < pidx; ++i) { U[i + 1] = (char_type)A[i]; }
    for(i += 1; i < n; ++i) { U[i] = (char_type)A[i]; }
    pidx += 1;
  }
  return pidx;
}


#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif /* __cplusplus */
#endif /* _SAIS_HXX */
