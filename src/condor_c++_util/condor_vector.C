#include "condor_vector.h"

//---------------------------------------------------------------------------
#if defined (TEST)

#include <iostream>

typedef std::vector<char,alloc>::size_type size_type;

int main (void) {
    char val;

    cout << "Enter default char value: ";
    cin >> val;

    condor::vector<char> vect(5, val);
    bool quit = false;

    while (!quit) {
        cout << "Vector: (";
        copy(vect.begin(), vect.end(),
             std::ostream_iterator<char>(std::cout, ","));
        cout << "end)" << endl;
        cout << "Size: " << vect.size() << endl;
        cout << "Capacity: " << vect.capacity() << endl;

        cout << "Choice:  (I)nsert (A)ssign (R)ead (C)lear (Q)uit ";
        char c;
        size_type pos;
        cin >> c;
        cout << endl;
        switch (c) {
          case 'I':
          case 'i':
            cin >> pos >> val;
            cout << "vect.insert(vect.begin()+" << pos << ")" << endl;
            vect.insert(vect.begin()+pos, val);
            break;
          case 'A':
          case 'a':
            cin >> pos >> val;
            cout << "vect[" << pos << "] = " << val << endl;
            vect[pos] = val;
            break;
          case 'R':
          case 'r':
            cin >> pos;
			val = vect[pos];
            cout << "vect[" << pos << "] == '" << val << "'" << endl;
            break;
          case 'C':
          case 'c':
            cout << "vect.clear()" << endl;
            vect.clear();
            break;
          case 'Q':
          case 'q':
            quit = true;
            break;
        }
    }
    return 0;
}

#endif
