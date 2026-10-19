#pragma once
#include <string>

// Musical key encoded as 0..23: pitch class (C=0 .. B=11) + 12 for minor. -1 = unknown.
namespace keys {

inline bool valid(int k) { return k >= 0 && k < 24; }
inline int pitchClass(int k) { return k % 12; }
inline bool isMinor(int k) { return k >= 12; }

inline int transpose(int k, int semitones)
{
    if (!valid(k))
        return -1;
    const int pc = ((pitchClass(k) + semitones) % 12 + 12) % 12;
    return pc + (isMinor(k) ? 12 : 0);
}

inline std::string standardName(int k)
{
    static const char *names[12] = {"C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
    if (!valid(k))
        return "";
    return std::string(names[pitchClass(k)]) + (isMinor(k) ? "m" : "");
}

// Camelot wheel: a fifth up = +1. Am = 8A, C = 8B.
inline std::string camelot(int k)
{
    if (!valid(k))
        return "";
    const int pc = pitchClass(k);
    int n = (pc * 7 + (isMinor(k) ? 5 : 8)) % 12;
    if (n == 0)
        n = 12;
    return std::to_string(n) + (isMinor(k) ? "A" : "B");
}

} // namespace keys
