/*****************************************************************************

Copyright (C) 2006 Гришин Максим Леонидович (altmer@arts-union.ru)
          (C) 2006 Maxim L. Grishin  (altmer@arts-union.ru)

        Программное обеспечение (ПО) предоставляется "как есть" - без явной и
неявной гарантии. Авторы не несут никакой ответственности за любые убытки,
связанные с использованием данного ПО, вы используете его на свой страх и риск.
        Данное ПО не предназначено для использования в странах с патентной
системой разрешающей патентование алгоритмов или программ.
        Авторы разрешают свободное использование данного ПО всем желающим,
в том числе в коммерческих целях.
        На распространение измененных версий ПО накладываются следующие ограничения:
        1. Запрещается утверждать, что это вы написали оригинальный продукт;
        2. Измененные версии не должны выдаваться за оригинальный продукт;
        3. Вам запрещается менять или удалять данное уведомление из пакетов с исходными текстами.

*****************************************************************************/

#ifndef ALEXBASE_H
#define ALEXBASE_H

#include "../astring.h"
#include "../avariant.h"
#include "../afile.h"

namespace alt
{

struct ALexElement
{
    int type;
    int file_index;
    int size;
    int offset;
    int line,columne;
    array<variant> extra;
    array<ALexElement> macroLexStack;
};

struct ALexErrorInfo
{
    int realCode;
    string fname;
    int lineCounter;
    int symCounter;
    int realOffset;
};

class ALexFileNode
{
public:

    //кострукторы, деструкторы и операторы
    ALexFileNode(const string &fname, const byteArray &src, int off=0)
    {
        data=src;
        offset=off;
        path=fname;
        currPos=0;
        lineCounter=0;
        symCounter=0;
    }
    ALexFileNode()
    {
        offset=0;
        currPos=0;
        lineCounter=0;
        symCounter=0;
    }
    ALexFileNode(const ALexFileNode &val)
    {
        *this=val;
    }
    virtual ~ALexFileNode(){}

    ALexFileNode& operator=(const ALexFileNode &val)
    {
        offset=val.offset;
        currPos=val.currPos;
        lineCounter=val.lineCounter;
        symCounter=val.symCounter;
        path=val.path;
        data=val.data;
        return *this;
    }

    //работа с ошибками
    ALexErrorInfo fixError(int code) const
    {
        ALexErrorInfo rv;
        rv.fname=path;
        rv.lineCounter=lineCounter;
        rv.realCode=code;
        rv.realOffset=position();
        rv.symCounter=symCounter;
        return rv;
    }

    int currLine(){return lineCounter;}
    int currColumn(){return symCounter;}

    //функционал извлечения символов и статистики
    int position() const
    {
        return currPos+offset;
    }

    bool atEnd()
    {
        scipNotNewLine();
        if(data.size()<=currPos) return true;
        return false;
    }

    string getElement(int pos, int size)
    {
        string rv;
        rv.reserve(size);
        for(int i=0;i<size;i++)
        {
            int posit=pos-offset+i;
            posit=scipNotNewLine_helper(posit);
            i=posit-(pos-offset);
            if(i>=size)break;

            if(!data[pos-offset+i])
            {
                rv.append(' ');
                continue;
            }
            rv.append(data[pos-offset+i]);
        }
        return rv;
    }

    char getSym(int ind)
    {
        scipNotNewLine();

        //пропуск отмененных переносов строк
        int posit=currPos;
        while(ind)
        {
            posit++;ind--;
            if(data.size()<=posit)return 0;

            //пропускаем отмененный перевод строки
            posit=scipNotNewLine_helper(posit);
        }

        if(data.size()<=posit)return 0;
        if(data[posit]=='\r')return '\n';
        return data[posit];
    }

    void popSym(int cnt)
    {
        while(cnt--){popSym();}
    }

    char popSym()
    {
        scipNotNewLine();
        if(data.size()<=currPos)return 0;
        char rv=data[currPos];
        if(rv=='\r' || rv=='\n')
        {
            if(data.size()>currPos+1 && (data[currPos+1]=='\r' || data[currPos+1]=='\n'))
            {
                if(rv!=data[currPos+1])currPos++;
            }
            lineCounter++;
            symCounter=0;
            currPos++;
            return '\n';
        }
        currPos++;
        symCounter++;
        return rv;
    }

    string fileName() const
    {
        return path;
    }

    //указатель на текущий лексический стек
    array<ALexElement> *currLexStack;
    array<array<ALexElement>*> ifBlockStack;

protected:

    virtual int scipNotNewLine_helper(int posit)
    {
        return posit;
    }

    void scipNotNewLine()
    {
        //пропускаем отмененный перевод строки
        currPos=scipNotNewLine_helper(currPos);
    }

    string path;
    byteArray data;
    int currPos;

    //счетчики положения
    int lineCounter;
    int symCounter;

    //для механизмов подмены
    int offset;

};

class ALexBase
{
public:
    ALexBase();
    virtual ~ALexBase();

    void clear();

    //код ошибки говорит о количестве ошибок
    virtual retCode parce(const string &fname);

    enum LexType
    {
        LEX_UNDEF,
        LEX_SEP,
        LEX_COMM,
        LEX_OPER,
        LEX_STR,
        LEX_CHAR,
        LEX_ID,
        LEX_CONST,
        LEX_USER_BASE
    };

    enum ErrorCodes
    {
        ERR_NO,
        ERR_FILE_OPEN,
        ERR_INCOMPLETE_STRING,
        ERR_INCOMPLETE_CHAR,
        ERR_UNDEFINED_LEX,
        ERR_USER_BASE
    };

    //вернет описание ошибки
    ALexErrorInfo errDescriptor(int index);

protected:

    static int countLinePos(byteArray data, int offset, int *sympos=NULL);

    //лексические таблицы
    int lexTable[256];
    void buildDefLexTable();
    virtual void parceCurrent();

    void makeError(int code);

    //кэш данных из задействованных файлов
    hash< string, byteArray > cash;
    hash< string, array<ALexElement>* > lexDatum;
    array<ALexFileNode*> fileStack;

    //здесь храним найденные ошибки и предупреждения
    array<ALexErrorInfo> errStack;
    array<ALexErrorInfo> warningStack;

    virtual void scipID(int ind);
    virtual void scipLine(int ind);
    virtual bool scipConst(int ind, char fsym);

};

} //alt namespace

#endif // ALEXBASE_H


