/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#ifndef CONDOR_VECTOR_H
#define CONODR_VECTOR_H

#include <vector>

namespace condor {

/** A subclass of std::vector.  Accessing the vector out of bounds with
    operator[] automatically stretches the vector rather than seg faulting.
    This class remembers a filler value, which is returned when an
    uninitialized element is read.  See the \URL[STL
    Documentation]{http://www.sgi.com/Technology/STL/Vector.html} for complete
    information on std::vector meathod semantics.  */
template <class T, class Alloc = alloc>
class vector : public std::vector<T,alloc> {
    
  public:
    
    /// Filler value is the default constructor for Alloc
    vector() : std::vector<T,alloc>(), m_filler(T()) {}
    
    /** @param value The filler value
     */
    vector(size_type n, const T& value) :
        std::vector<T,alloc>(n,value), m_filler(value) {}

    /** @param value The filler value
     */
    vector(int n, const T& value) :
        std::vector<T,alloc>(n,value),m_filler(value) {}

    /** @param value The filler value
     */
    vector(long n, const T& value) :
        std::vector<T,alloc>(n,value),m_filler(value) {}

    /// Filler value is the default constructor for Alloc
    explicit vector(size_type n) : std::vector<T,alloc>(n),m_filler(T()) {}

    ///
    vector(const vector<T, Alloc>& x) : std::vector<T,alloc>(x) {}

#ifdef __STL_MEMBER_TEMPLATES
    ///
    template <class InputIterator>
    ///
    vector(InputIterator first, InputIterator last) : 
        std::vector<T,alloc>(first,last) {}
#else /* __STL_MEMBER_TEMPLATES */
    vector(const_iterator first, const_iterator last) :
        std::vector<T,alloc>(first,last) {}

#endif /* __STL_MEMBER_TEMPLATES */
    
    /** Accessing the vector out of bounds returns the filler value.  The
        filler value was provided by the constructor, or is the default
        allocator if none was provided.
    */
    const_reference operator[] (size_type n) const {
        if (n >= size()) return m_filler;
        else return std::vector<T,alloc>::operator[](n);
    }

    /** Stretches the vector's capacity if necessary before assigning the
        element
    */
    inline reference operator[](size_type n) {
        check_n_stretch(n);
        return std::vector<T,alloc>::operator[](n);
    }

  private:

    void check_n_stretch (size_type n) {
        if (n >= capacity()) {
            // Find next power of 2 larger than n
            size_type N = 1;
            for (size_type s = n; s != 0 ; s >>= 1, N <<= 1);
            reserve (N);
        }
        if (n >= size()) resize(n+1, m_filler);
    }
    
    const T m_filler;
};

} // namespace condor

#endif
