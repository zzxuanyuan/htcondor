#ifndef CONDOR_VECTOR_H
#define CONODR_VECTOR_H

#include <vector>

namespace condor {

template <class T, class Alloc = alloc>
class vector : public std::vector<T,alloc> {

  public:

    vector() : std::vector<T,alloc>(), m_filler(T()) {}
    vector(size_type n, const T& value) :
        std::vector<T,alloc>(n,value), m_filler(value) {}
    vector(int n, const T& value) :
        std::vector<T,alloc>(n,value),m_filler(value) {}
    vector(long n, const T& value) :
        std::vector<T,alloc>(n,value),m_filler(value) {}
    explicit vector(size_type n) : std::vector<T,alloc>(n),m_filler(T()) {}

    vector(const vector<T, Alloc>& x) : std::vector<T,alloc>(x) {}

#ifdef __STL_MEMBER_TEMPLATES
    template <class InputIterator>
    vector(InputIterator first, InputIterator last) : 
        std::vector<T,alloc>(first,last) {}
#else /* __STL_MEMBER_TEMPLATES */
    vector(const_iterator first, const_iterator last) :
        std::vector<T,alloc>(first,last) {}

#endif /* __STL_MEMBER_TEMPLATES */
    
    const_reference operator[] (size_type n) const {
        if (n >= size()) return m_filler;
        else return std::vector<T,alloc>::operator[](n);
    }

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
