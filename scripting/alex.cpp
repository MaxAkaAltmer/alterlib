#include "alex.h"

using namespace alt;

ALexBase::ALexBase()
{
    buildDefLexTable();
}

ALexBase::~ALexBase()
{
    clear();
}

int ALexBase::countLinePos(byteArray data, int offset, int *sympos)
{
    int counter=1;
    if(sympos)*sympos=0;
    for(int i=0;i<offset;i++)
    {
        if(data[i]=='\r')
        {
            if(i+1<offset && data[i+1]=='\n')i++;
            counter++;
            if(sympos)*sympos=0;
        }
        else if(data[i]=='\n')
        {
            if(i+1<offset && data[i+1]=='\r')i++;
            counter++;
            if(sympos)*sympos=0;
        }
        else
        {
            if(sympos)*sympos+=1;
        }
    }
    return counter;
}

void ALexBase::buildDefLexTable()
{
    for(int i=0;i<256;i++)lexTable[i]=LEX_UNDEF;

    lexTable[uint8(' ')]=LEX_SEP;
    lexTable[uint8('\t')]=LEX_SEP;
    lexTable[uint8('\r')]=LEX_SEP;
    lexTable[uint8('\n')]=LEX_SEP;
    lexTable[uint8('\v')]=LEX_SEP;
    lexTable[uint8('\f')]=LEX_SEP;

    const char ops[]="@$`#{},|[]+-%;:?<>=!&~^*\\()/.";
    for(uintx i=0;i<sizeof(ops)/sizeof(ops[0]);i++)lexTable[(uint8)ops[i]]=LEX_OPER;

    for(int v='0';v<='9';v++)lexTable[v]=LEX_CONST;

    lexTable[uint8('_')]=LEX_ID;
    for(int v='A';v<='Z';v++)lexTable[v]=LEX_ID;
    for(int v='a';v<='z';v++)lexTable[v]=LEX_ID;

    lexTable[uint8('\"')]=LEX_STR;

    lexTable[uint8('\'')]=LEX_CHAR;
}

void ALexBase::clear()
{
    for(int i=0;i<fileStack.size();i++)
        delete fileStack[i];
    fileStack.clear();

    cash.clear();
    errStack.clear();

    for(int i=0;i<lexDatum.size();i++)
        delete lexDatum.values()[i];
    lexDatum.clear();
}

void ALexBase::parceCurrent()
{
    int ind=fileStack.size()-1;
    int findex=cash.indexOf(fileStack[ind]->fileName());

    while(!fileStack[ind]->atEnd())
    {
        ALexElement el;
        el.file_index=findex;
        el.offset=fileStack[ind]->position();
        el.line=fileStack[ind]->currLine();
        el.columne=fileStack[ind]->currColumn();
        char csym=fileStack[ind]->popSym();
        el.type=lexTable[(uint8)csym];

        switch(el.type)
        {
        case LEX_UNDEF: //вычленяем разделители
            while(!fileStack[ind]->atEnd()
                  && lexTable[(uint8)fileStack[ind]->getSym(0)]==LEX_UNDEF)
            {
                fileStack[ind]->popSym();
            }
            el.size=fileStack[ind]->position()-el.offset;
            break;
        case LEX_SEP: //вычленяем разделители
            while(!fileStack[ind]->atEnd()
                  && lexTable[(uint8)fileStack[ind]->getSym(0)]==LEX_SEP)
            {
                fileStack[ind]->popSym();
            }
            el.size=fileStack[ind]->position()-el.offset;
            break;
            continue;
        case LEX_OPER:
            el.size=fileStack[ind]->position()-el.offset;
            break;
        case LEX_STR:
            while(!fileStack[ind]->atEnd())
            {
                char c=fileStack[ind]->getSym(0);
                if(c=='\n' || c=='\"')
                {
                    break;
                }
                fileStack[ind]->popSym();
            }
            if(!fileStack[ind]->atEnd() &&
                    fileStack[ind]->getSym(0)=='\"')
            {
                fileStack[ind]->popSym();
                el.size=fileStack[ind]->position()-el.offset;
            }
            else
            {
                el.size=fileStack[ind]->position()-el.offset;
                //генерим ошибку
                makeError(ERR_INCOMPLETE_STRING);
            }
            break;
        case LEX_CHAR:
            while(!fileStack[ind]->atEnd())
            {
                char c=fileStack[ind]->getSym(0);
                if(c=='\n' || c=='\'')
                {
                    break;
                }
                fileStack[ind]->popSym();
            }
            if(!fileStack[ind]->atEnd() &&
                    fileStack[ind]->getSym(0)=='\'')
            {
                fileStack[ind]->popSym();
                el.size=fileStack[ind]->position()-el.offset;
            }
            else
            {
                el.size=fileStack[ind]->position()-el.offset;
                //генерим ошибку
                makeError(ERR_INCOMPLETE_CHAR);
            }
            break;
        case LEX_ID:
            scipID(ind);
            el.size=fileStack[ind]->position()-el.offset;
            break;
        case LEX_CONST:
            if(!scipConst(ind, csym))
            {
                el.type=LEX_OPER;
            }
            el.size=fileStack[ind]->position()-el.offset;
            break;
        };
        fileStack[ind]->currLexStack->append(el);
    }
}

void ALexBase::scipID(int ind)
{
    while(!fileStack[ind]->atEnd())
    {
        char c=fileStack[ind]->getSym(0);
        if(lexTable[(uint8)c]!=LEX_ID && !(c>='0' && c<='9'))break;
        fileStack[ind]->popSym();
    }
}

void ALexBase::scipLine(int ind)
{
    while(!fileStack[ind]->atEnd())
    {
        char c=fileStack[ind]->getSym(0);
        if(c=='\n')break;
        fileStack[ind]->popSym();
    }
}

bool ALexBase::scipConst(int ind, char fsym)
{
    while(!fileStack[ind]->atEnd())
    {
        char c=fileStack[ind]->getSym(0);
        if(lexTable[(uint8)c]!=LEX_CONST && lexTable[(uint8)c]!=LEX_ID)break;
        fileStack[ind]->popSym();
    }
    return true;
}

retCode ALexBase::parce(const string &fname)
{
    file hand(fname);

    //пытаемся загрузить и обработать файл
    if(hand.open(file::OReadOnly))
    {
        int64 size=hand.size();

        //сформируем точку разбора
        byteArray temp=hand.read(size);
        fileStack.append(new ALexFileNode(fname,temp,0));

        //подготовимся к рекурсии
        cash[fname]=temp;
        fileStack.last()->currLexStack = new array<ALexElement>;
        lexDatum[fname]=fileStack.last()->currLexStack;
        parceCurrent();

        //почистим за собой
        delete fileStack.last();
        fileStack.pop();
        hand.close();
        return -errStack.size();
    }

    //формируем ошибку открытия файла
    makeError(ERR_FILE_OPEN);
    return -errStack.size();
}

ALexErrorInfo ALexBase::errDescriptor(int index)
{
    if(errStack.size()<index)
    {
        ALexErrorInfo rv;
        rv.realCode=0;
        return rv;
    }
    return errStack[index];
}

void ALexBase::makeError(int code)
{
    int ind=fileStack.size()-1;
    if(ind<0)return;
    ALexErrorInfo err=fileStack[ind]->fixError(code);
    errStack.append(err);
}
