#ifndef TRANSLATE_H
#define TRANSLATE_H

#include "astring.h"
#include "at_hash.h"

namespace alt {

    string at(string text);
    hash<string,string> translation();
    void setTranslation(hash<string,string> transl);

} // namespace alt

#endif // TRANSLATE_H
