/*****************************************************************************

This is part of Alterlib - the free code collection under the MIT License
------------------------------------------------------------------------------
Copyright (C) 2006-2023 Maxim L. Grishin  (altmer@arts-union.ru)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*****************************************************************************/

#include "arch.h"
#include "arch_delta_code.h"
#include "arch_prefix.h"
#include "arch_arith.h"
#include "arch_diction.h"

#define ARCH_HEAD_UNCODED                   0

#define ARCH_HEAD_PREPROCESSING_DELTA       1
#define ARCH_HEAD_PREPROCESSING_EXT         3

#define ARCH_HEAD_DICTION_STD               1
#define ARCH_HEAD_DICTION_EXT               3

#define ARCH_HEAD_STATISTIC_ARITHMETIC      1
#define ARCH_HEAD_STATISTIC_EXT             3


alt::byteArray _arch_data_block_compress_best(const void *data_p, int size)
{
    alt::byteArray result;
    MDWFormat delta_form;
    alt::byteArray delta, delta_dict, dict;

    const uint8 *data=(const uint8*)data_p;

    //будем применять разностный код
    delta_form=_md_FindBestFormat(data,size>>3);
    if(delta_form.block_size)
    {
        //запишем структуру дельта-кода
        delta.append(delta_form.block_size);
        delta.append((delta_form.big_endian?1:0)|(MLDLT_NORM<<1));
        delta.append(delta_form.fields.size());
        for(int i=0;i<delta_form.fields.size();i++)
        {
            delta.append(delta_form.fields[i]);
        }
        //добавим данные
        delta.append((char*)data,size);
        //выполним кодирование
        _md_Encoder(delta()+3+delta_form.fields.size(),size,delta_form,MLDLT_NORM);
    }

    //применим словарный метод
    //if(delta.size())_arch_diction_encode(delta_dict,delta(),delta.size());
    _arch_diction_encode(dict,data,size);

    //применим статистический метод
    ArithEncoder arithc;
    alt::bitSpace<uint32> bs_ori,bs_delta,bs_dict,bs_delta_dict;

    bs_ori.reAllocate(size*12);
    arithc.Start(bs_ori,size);
    arithc.Put(data,size,bs_ori);
    arithc.Stop(bs_ori);

    if(delta.size())
    {
        bs_delta.reAllocate(delta.size()*12);
        arithc.Start(bs_delta,delta.size());
        arithc.Put(delta(),delta.size(),bs_delta);
        arithc.Stop(bs_delta);
    }

    if(delta_dict.size())
    {
        bs_delta_dict.reAllocate(delta_dict.size()*12);
        arithc.Start(bs_delta_dict,delta_dict.size());
        arithc.Put((uint8*)delta_dict(),delta_dict.size(),bs_delta_dict);
        arithc.Stop(bs_delta_dict);
    }

    bs_dict.reAllocate(dict.size()*12);
    arithc.Start(bs_dict,dict.size());
    arithc.Put((uint8*)dict(),dict.size(),bs_dict);
    arithc.Stop(bs_dict);

    //запишем результат - 0 - *, 1 - dws, 2 - ds, 3 - s, 4 - ws, 5 - w, 6 - dw
    int selector=0;
    uint32 selsize=size;

    if(delta_dict.size() && (uint32)delta_dict.size()<selsize){selsize=delta_dict.size();selector=6;}
    if(bs_delta_dict.size() && bs_delta_dict.size()<selsize){selsize=bs_delta_dict.size();selector=1;}
    if((uint32)dict.size()<selsize){selsize=dict.size();selector=5;}
    if(bs_dict.size()<selsize){selsize=bs_dict.size();selector=4;}
    if(bs_ori.size()<selsize){selsize=bs_ori.size();selector=3;}
    if(bs_delta.size() && bs_delta.size()<selsize){selsize=bs_delta.size();selector=2;}

    switch(selector)
    {
    case 1: //dws ..test-ok..
        result.append((char)(ARCH_HEAD_PREPROCESSING_DELTA|(ARCH_HEAD_DICTION_STD<<2)|(ARCH_HEAD_STATISTIC_ARITHMETIC<<4)));
        result.append((char*)bs_delta_dict(),bs_delta_dict.size());
        break;
    case 2: //ds ..test-ok..
        result.append((char)(ARCH_HEAD_PREPROCESSING_DELTA|(ARCH_HEAD_STATISTIC_ARITHMETIC<<4)));
        result.append((char*)bs_delta(),bs_delta.size());
        break;
    case 3: //s ..test-ok..
        result.append((char)((ARCH_HEAD_STATISTIC_ARITHMETIC<<4)));
        result.append((char*)bs_ori(),bs_ori.size());
        break;
    case 4: //ws ..test-ok..
        result.append((char)((ARCH_HEAD_DICTION_STD<<2)|(ARCH_HEAD_STATISTIC_ARITHMETIC<<4)));
        result.append((char*)bs_dict(),bs_dict.size());
        break;
    case 5: //w ..test-ok..
        result.append((char)((ARCH_HEAD_DICTION_STD<<2)));
        result.append(dict);
        break;
    case 6: //dw ..test-ok..
        result.append((char)(ARCH_HEAD_PREPROCESSING_DELTA|(ARCH_HEAD_DICTION_STD<<2)));
        result.append(delta_dict);
        break;
    default: //uncompressed ..test-ok..
        result.append((char)ARCH_HEAD_UNCODED);
        result.append((char*)data,size);
        break;
    }

    return result;
}

alt::byteArray _arch_data_block_compress_rle(const void *data_p, int size)
{
    alt::byteArray result;

    const uint8 *data=(const uint8*)data_p;

    _arch_diction_encode(result,data,size,4);
    result.prepend((char)((ARCH_HEAD_DICTION_STD<<2)));
    return result;
}

alt::byteArray _arch_data_block_compress_fast(const void *data_p, int size)
{    
    alt::byteArray result;
    ArithEncoder arithc;
    alt::bitSpace<uint32> bs_result;

    const uint8 *data=(const uint8*)data_p;

    _arch_diction_encode(result,data,size,2048);
    if(result.size()>size) //словарный метод не эффективен?
    {
        //пробуем статистический
        bs_result.reAllocate(size*12);
        arithc.Start(bs_result,size);
        arithc.Put(data,size,bs_result);
        arithc.Stop(bs_result);

        if(size<(int)bs_result.size()) //тоже плохо?
        {
            result.clear();
            result.append((char)ARCH_HEAD_UNCODED);
            result.append((char*)data,size);
            return result;
        }

        //вернем результат статистического сжатия
        result.resize(0);
        result.append((char)((ARCH_HEAD_STATISTIC_ARITHMETIC<<4)));
        result.append((char*)bs_result(),bs_result.size());
        return result;
    }

    //добьем статистическим сжатием
    bs_result.reAllocate(result.size()*12);
    arithc.Start(bs_result,result.size());
    arithc.Put((uint8*)result(),result.size(),bs_result);
    arithc.Stop(bs_result);

    if(result.size()<(int)bs_result.size())
    { //статистика ухудшила результат?
        result.prepend((char)((ARCH_HEAD_DICTION_STD<<2)));
        return result;
    }

    //вернем результат двуслойного кодирования
    result.resize(0);
    result.append((char)((ARCH_HEAD_DICTION_STD<<2)|(ARCH_HEAD_STATISTIC_ARITHMETIC<<4)));
    result.append((char*)bs_result(),bs_result.size());

    return result;
}

//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////

alt::byteArray _arch_data_block_decompress_layer3(const uint8 *data, int size, int format)
{
    alt::byteArray result;

    if(format!=ARCH_HEAD_STATISTIC_ARITHMETIC)return alt::byteArray();

    ArithDecoder decoder;
    alt::bitSpace<uint32> bs((void*)data,size);

    uint32 resize=decoder.Start(bs);
    if(!resize)return alt::byteArray();
    result.resize(resize);

    resize=decoder.Get(result(),resize,bs);

    if(!decoder.IsEnd() || resize!=(uint32)result.size())
        return alt::byteArray();

    decoder.Stop();

    return result;
}

void _arch_data_block_decompress_layer1(alt::byteArray &data, int format)
{
    if(format!=ARCH_HEAD_PREPROCESSING_DELTA || data.size()<4)
    {
        data.clear();
        return;
    }

    MDWFormat delta_form;
    delta_form.block_size=data[0];
    delta_form.big_endian=data[1]&1;

    int head_size=data[2]+3;
    if(data.size()<=head_size || MLDLT_FIX<(data[1]>>1)){data.clear();return;}

    int bsiz=0;
    for(int i=0;i<data[2];i++)
    {
        delta_form.fields.append(data[3+i]);
        bsiz+=data[3+i];
        if(!data[3+i])bsiz++;
    }

    if(bsiz!=delta_form.block_size){data.clear();return;}

    _md_Decoder(data()+head_size, data.size()-head_size, delta_form, data[1]>>1);
    data.remove(0,head_size);
}

alt::byteArray _arch_data_block_decompress(const void *data_p, int size, int32 predict_size)
{
    const uint8 *data=(const uint8*)data_p;

    alt::byteArray result;
    uint8 head=data[0];
    data++;size--;

    if(predict_size>0)predict_size+=64;

    if(!head) //данные не сжаты?
    {
        result.append((char*)data,size);
        return result;
    }

    //расширений пока не предусмотрено
    if((head>>6)&3)return alt::byteArray();

    //снимаем статистический слой кодирования
    switch((head>>4)&3)
    {
    case ARCH_HEAD_STATISTIC_ARITHMETIC:
        result=_arch_data_block_decompress_layer3(data,size,ARCH_HEAD_STATISTIC_ARITHMETIC);
        if(!result.size())return alt::byteArray();
        break;
    case ARCH_HEAD_UNCODED:
        result.append((char*)data,size);
        break;
    default:
        return alt::byteArray();
    };

    //снимаем словарный слой кодирования
    switch((head>>2)&3)
    {
    case ARCH_HEAD_DICTION_STD:
    {
        alt::byteArray temp;
        _arch_diction_decode(temp,result(),result.size(),predict_size);
        result=temp;
        if(!result.size())return alt::byteArray();
    }
        break;
    case ARCH_HEAD_UNCODED:
        break;
    default:
        return alt::byteArray();
    };

    //снимаем слой препроцессинга
    switch((head)&3)
    {
    case ARCH_HEAD_PREPROCESSING_DELTA:
        _arch_data_block_decompress_layer1(result,ARCH_HEAD_PREPROCESSING_DELTA);
        break;
    case ARCH_HEAD_UNCODED:
        break;
    default:
        return alt::byteArray();
    };

    return result;
}

alt::byteArray _arch_data_block_compress_diff(const void *data_p, int size,
                                          const void *old_data_p)
{
    alt::array<int> meta_type;
    alt::array<int> meta_size;

    const uint8 *data=(const uint8*)data_p;
    const uint8 *old_data=(const uint8*)old_data_p;

    int off=0,j,i;
    for(i=0;i<size;)
    {
        //проверяем на эквивалентность
        if(data[i]==old_data[i])
        {
            //вычисляем объем теста
            int max=size-i;
            //тестируем
            for(j=1;j<max;j++)
            {
                if(data[i+j]!=old_data[i+j])break;
            }
            //стоит заморачиваться?
            if(j>3)
            {
                if(off!=i)//есть неэквивалентные?
                {
                    meta_type.append(1);
                    meta_size.append(i-off);
                }
                meta_type.append(0);
                meta_size.append(j);
                off=i+j;
            }
            i+=j;
        }
        else //идем дальше
        {
            i++;
        }
    }
    if(off!=i) //есть неэквивалентные?
    {
        meta_type.append(1);
        meta_size.append(size-off);
    }

    //создаем патч
    alt::byteArray patch;
    patch.reserve(size);
    off=0;
    for(i=0;i<meta_type.size();i++)
    {
        if(meta_type[i]) //new?
        {
            uint32 sv=_pref_sign2unsign(-meta_size[i]);
            _pref_p1qX_encoder(sv,7,patch);
            for(int j=0;j<meta_size[i];j++)
                patch.append((char)(data[off+j]^old_data[off+j]));
        }
        else //old data
        {
            uint32 sv=_pref_sign2unsign(meta_size[i]);
            _pref_p1qX_encoder(sv,7,patch);
        }
        off+=meta_size[i];
    }

    //сжимаем патч
    return _arch_data_block_compress_rle((uint8*)patch(),patch.size());
}

bool _arch_data_block_decompress_diff(uint8 *patch, int patch_size,
                                            uint8 *data, int size)
{
    //рспакуем патч
    alt::byteArray tmp=_arch_data_block_decompress(patch,patch_size);
    if(tmp.isEmpty())return false;

    //патчим предыдущую последовательность
    int off=0,offset=0;
    while(off<tmp.size())
    {
        uint32 vs;
        int len=_pref_p1q7_decoder(&vs,&tmp()[off],tmp.size()-off);
        if(len<0)return false;

        int sz=_pref_unsign2sign(vs);
        off+=len;
        if(sz<0)
        {
            sz=-sz;
            if(sz+off>tmp.size())return false;
            if(offset+sz>size)return false;
            for(int j=0;j<sz;j++)
                data[offset+j]^=tmp()[off+j];
            off+=sz;
        }
        else
        {
            if(offset+sz>size)return false;
        }
        offset+=sz;
    }

    return true;
}

