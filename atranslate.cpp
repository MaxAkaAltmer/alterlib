#include "atranslate.h"

using namespace alt;

static hash<string,string> at_diction;

string alt::at(string text)
{
    if(at_diction[text].isEmpty()) return text;
    return at_diction[text];
}

hash<string,string> alt::translation()
{
    return at_diction;
}

void alt::setTranslation(hash<string,string> transl)
{
    at_diction.insert(transl);
}
