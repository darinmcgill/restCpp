#pragma once
#include "external.hpp"

namespace restCpp {
using namespace std;

inline
char toHex(int val) {
    if (val < 0 || val > 15) 
        throw runtime_error(string("toHex(val):") + to_string(val));
    switch (val) {
        case 10: return 'A';
        case 11: return 'B';
        case 12: return 'C';
        case 13: return 'D';
        case 14: return 'E';
        case 15: return 'F';
        default:
            return char('0' + val);
    }
}


inline
string repr(const char* ptr,ssize_t len=-1) {
    if (len == -1) len = strlen(ptr);
    char buffer[(len+2) * 6];
    size_t n = 0;
    buffer[n++] = '"';
    for (size_t i=0;i<len;i++) {
        char c = ptr[i];
        if (c >= 0x20 && c <= 0x7E && !(c == 0x22 || c == 0x5C)){
            buffer[n++] = c;
        } else {
            buffer[n++] = 0x5C; // backslash
            switch (c) {
                case '\n': buffer[n++] = 'n'; break;
                case '\t': buffer[n++] = 't'; break;
                case '\r': buffer[n++] = 'r'; break;
                case 0x22: buffer[n++] = 0x22; break;
                case 0x5C: buffer[n++] = 0x5C; break;
                default:
                    buffer[n++] = 'u';
                    buffer[n++] = '0';
                    buffer[n++] = '0';
                    buffer[n++] = toHex(c / 16);
                    buffer[n++] = toHex(c % 16);
            }
        }
    }
    buffer[n++] = '"';
    return string(buffer,n);
}

inline 
string repr(string s) {
    return repr(s.c_str(),s.size());
}

inline
string after(string left, const char* rite) {
    // "foobarbaz" % "bar" => "baz"
    string right = rite;
    auto loc = left.find(right);
    if (loc == string::npos) 
        throw runtime_error("failed:after("+repr(left) + "," + repr(rite)+")");
    return left.substr(loc + right.size());
}

inline
string before(string left, const char* rite) {
    // "foobarbaz" / "bar" => "foo"
    string right = rite;
    auto loc = left.find(right);
    if (loc == string::npos) return left;
    return left.substr(0,loc);
}

inline
bool contains(string left, string right) {
    //  "foobarbaz" & "bar" => true
    auto loc = left.find(right);
    if (loc == string::npos) return false;
    return true;
}


inline
bool isWhitespace(char c) {
    switch (c) {
        case ' ':
        case '\t':
        case '\r':
        case '\n':
            return true;
        default:
            return false;
    }
}


inline
string strip(string thing) {
    size_t i = 0;
    for (;i<thing.size();i++) {
        if (!isWhitespace(thing[i])) break;
    }
    if (i==thing.size()) return string("");
    size_t j = thing.size() - 1;
    for (;;j--) {
        if (!isWhitespace(thing[j])) break;
    }
    return thing.substr(i,j-i+1);
}


inline
vector<string> split(string thing, string how) {
    vector<string> out;
    size_t blen = how.size();
    size_t loc = 0;
    while (1) {
        size_t x = thing.find(how,loc);
        if (x == string::npos) {
            out.push_back( thing.substr(loc) );
            return out;
        }
        out.push_back( thing.substr(loc,x - loc) );
        loc = x + blen;
    }
}




}
