/*****************************************************************************

This is part of Alterlib - the free code collection under the MIT License
------------------------------------------------------------------------------
Copyright (C) 2006-2026 Maxim L. Grishin  (altmer@arts-union.ru)

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

#ifndef INTMATH_UTILS_HEADER_DEFINITION
#define INTMATH_UTILS_HEADER_DEFINITION

#include "atypes.h"
#include <type_traits>

namespace alt {
namespace imath {

    // реверс всех бит в значении (битовое зеркало)
    // T - беззнаковый целый тип (uint8/uint16/uint32/uint64)
	template <class T>
	constexpr __inline T rev(T x) noexcept
	{
		static_assert(std::is_unsigned_v<T>, "rev() supports only unsigned integer types");
		static_assert(sizeof(T) <= 8, "rev() supports only 8/16/32/64-bit types");

		constexpr T maxv = T(~T(0));
		constexpr T m1 = maxv / 3;     // 0x5555... (младшие биты пар)
		constexpr T m2 = maxv / 5;     // 0x3333... (младшие пары четверок)

		// Меняем соседние биты (по 1) и соседние пары (по 2):
		x = ((x >> 1) & m1) | ((x & m1) << 1);
		x = ((x >> 2) & m2) | ((x & m2) << 2);

		if constexpr (sizeof(T) > 1) {
			constexpr T m4 = maxv / 17; // 0x0F0F... (ровно под размер T, без опечаток!)
			x = ((x >> 4) & m4) | ((x & m4) << 4);
		}
		if constexpr (sizeof(T) > 2) {
			constexpr T m8 = maxv / 257; // 0x00FF00FF...
			x = ((x >> 8) & m8) | ((x & m8) << 8);
		}
		if constexpr (sizeof(T) > 4) {
			constexpr T m16 = maxv / 65537; // 0x0000FFFF0000FFFF
			x = ((x >> 16) & m16) | ((x & m16) << 16);
		}

		// Обмен половин (для uint8 это шаг по 4 бита, для uint64 — по 32 бита)
		constexpr int half_bits = sizeof(T) * 4;
		return (x >> half_bits) | (x << half_bits);
	}

    // деприкейтед: используйте rev<T>()
    [[deprecated("use rev<uint8>() instead")]]
    __inline uint8 rev8(uint8 x)
    {
        return rev(x);
    }

    // деприкейтед: используйте rev<T>()
    [[deprecated("use rev<uint32>() instead")]]
    __inline uint32 rev32(uint32 x)
    {
        return rev(x);
    }

    template <class T>
    __inline T abs(T val)
    {
            if(val<0)return -val;
            return val;
    }

    // расширение знака: бит (8*byte_count-1) копируется во ВСЕ старшие биты T.
    // Старшие байты только заполняются знаком и не отбрасываются; если нужно именно
    // отбрасывание, это арифметический сдвиг влево-вправо - уже другая операция.
    // Проверено MSVC перебором (8e6 пар (val, byte_count), T = uint8/16/32/64 и
    // int8/16/32/64, все byte_count от 1 до sizeof(T)): результат совпадает со
    // прежней реализацией и с независимым (побитовым) эталоном во всех случаях.
    //
    // Арифметика ведётся в беззнаковом аналоге T: для знакового T при
    // byte_count == sizeof(T) выражение вида "0 - (val & m)" было бы 0 - INT_MIN,
    // т.е. переполнением знакового целого (UB), а вычитание из нуля беззнакового
    // аналога определёно и даёт ровно то же расширение.
    //
    // safe = true включает проверку диапазона byte_count: вне [1, sizeof(T)] сдвиг
    // уходит за пределы ширины (это UB), и если количество байт приходит из внешних
    // данных, её лучше включить. По умолчанию safe = false - лишняя ветка не нужна
    // там, где число байт заведомо корректно (например, равно sizeof(T) или берётся
    // из константы формата). Проверка не стоит ничего лишнего и там, и там: MSVC для
    // exign<uint32,false> выдаёт 8 инструкций без единой cmp/jcc, для
    // exign<uint32,true> - 13 инструкций с cmp+ja, а при constexpr byte_count
    // проверка сворачивается полностью (5 инструкций) даже с safe = true.
    template <class T, bool safe = false>
    constexpr __inline T exign(T val, int byte_count) noexcept
    {
        static_assert(std::is_integral_v<T> && !std::is_same_v<T, bool>,
            "exign() supports only integer types");

        using U = std::make_unsigned_t<T>;
        if constexpr (safe)
        {
            if(byte_count < 1 || byte_count > int(sizeof(T)))return val;
        }
        const U m = U(1) << (8 * byte_count - 1);
        const U u = static_cast<U>(val);
        return static_cast<T>(u | (U(0) - (u & m)));
    }

    template <class T>
    __inline void swap(T &v1, T &v2)
    {
        T tmp=v1;
        v1=v2;
        v2=tmp;
    }

    template <class T>
    __inline T max(const T &v1, const T &v2)
    {
        if(v1>v2)return v1;
        return v2;
    }

    template <class T>
    __inline T min(const T &v1, const T &v2)
    {
        if(v1<v2)return v1;
        return v2;
    }

    template <class I, class R>
    __inline I round(R val)
    {
        return val >= R(0.0) ? I(val + R(0.5))
                             : I(val - I(val-1) + R(0.5)) + I(val-1);
    }

    // число бит, необходимых значению: floor(log2(val)) + 1. T - любой целый, кроме
    // bool; знаковый аргумент рассматривается как его беззнаковый эквивалент той же
    // ширины (двоичное дополнение), т.е. bsr(int32(-1)) = 32, а не 64. Прежний bsrT
    // делал bsr64(uint64(val)) и потому расширял знак до 64 бит - это была ошибка: у
    // int8 ответ выходил 64 вместо 8. На ней ошибочно основывался at_fraction.h
    // (max_count = bsrT(T(-1)) - 1 давало 63 для дроби любой ширины вместо числа бит T;
    // стало 7/15/31/63 для int8/16/32/64). Сортировка at_array.h не пострадала: там
    // bsrT(siz - 1) всегда положительный (siz 0 и 1 выходят раньше), сверено для
    // siz = 2..65.
    //
    // Начальная единица и есть особенность нуля: цепочка половинного сдвига даёт
    // k = floor(log2) для ненулевого значения и 0 для нуля, поэтому 1 + k - это и
    // обычное число бит, и 1 для нуля, без отдельной проверки и без ветки на ноль.
    // Вызывающий код этого ждёт там, где аргумент вполне может быть нулём и единица
    // является осмысленным ответом «бит нужен даже на ноль»: 1 << bsr32(...) в
    // дельта-кодере для нулевого байта, bsr32(width - 1) при width = 1.
    // Шаги цепочки отсечены по ширине W: остаются только легальные для данной ширины,
    // а бесполезные шаги ещё и стоят инструкций (замеры ниже).
    //
    // Проверено MSVC (tmp/t_bsr.cpp, /W4 /permissive-, /O2, /O1 и /std:c++17): 3.4e6
    // значений - все ширины T от uint8 до int64, исчерпывающе 0..65535, все границы
    // 2^b, 2^b - 1 и 1.5*2^b, по 4e5 случайных на каждую ширину, знаковые от -1024 до
    // 1024, а также -1 и INT_MIN. Беззнаковые ответы побитово совпадают и с прежними
    // телами bsr32/bsr64, и с независимым (побитовым) эталоном; со старым bsrT
    // сверено 1.6e6 знаковых значений и расхождение в 6.0e5 - все ровно там, где старый
    // давал 64 вместо ширины T (на положительных аргументах расхождений нет). bsr(0) = 1
    // во всех трёх сборках. MSVC /O2 x64, инструкций/операций сравнения и ветвления на
    // тело одного инстанциала (tmp/t_bsr_asm.cpp): uint8 17/4, uint16 21/5, uint32 29/5,
    // uint64 36/10 - против 43/10 у прежнего bsr32; у прежнего bsr64 было 12 инструкций, но
    // завершался он вызовом bsr32, т.е. те же 43 сверху. Знаковый T стоит столько же,
    // сколько беззнаковый той же ширины (приведение - no-op): bsr<int32> - 29/5.
    // bsr<bool> не собирается (C2338, только наше сообщение), вызовы bsr32/bsr64/bsrT
    // дают C4996.
    template <class T>
    constexpr __inline uint32 bsr(T val) noexcept
    {
        // is_unsigned_v<T> для bool тоже true, поэтому "целый и не bool"
        static_assert(std::is_integral_v<T> && !std::is_same_v<T, bool>,
            "bsr() supports only integer types");

        // W - беззнаковый аналог той же ширины: W и есть то, о чём спрашивает bsr
        // (сколько бит в данном рисунке бит), так что расширение ни к чему. Для bool
        // ширина подбирается только затем, чтобы тело не инстанцировалось с
        // "w >>= 4" над bool: иначе поверх нашего static_assert вылезали бы C4804
        using B = std::conditional_t<std::is_same_v<T, bool>, uint8, T>;
        using W = std::make_unsigned_t<B>;
        W w = static_cast<W>(val);

        uint32 k = 1;                           //ноль стоит одного бита
        if constexpr (sizeof(W) > 4){ if(w >> 32){ w >>= 32; k += 32; } }
        if constexpr (sizeof(W) > 2){ if(w >> 16){ w >>= 16; k += 16; } }
        if constexpr (sizeof(W) > 1){ if(w >> 8){ w >>= 8; k += 8; } }
        if(w >> 4){ w >>= 4; k += 4; }
        if(w >> 2){ w >>= 2; k += 2; }
        if(w >> 1)k += 1;
        return k;
    }

    // деприкейтед: используйте bsr<T>() - он делает то же самое для любой ширины T
    [[deprecated("use bsr<T>() instead")]]
    __inline uint32 bsr32(uint32 num)
    {
        return bsr(num);
    }

    [[deprecated("use bsr<T>() instead")]]
    __inline uint32 bsr64(uint64 val)
    {
        return bsr(val);
    }

    // атрибут стоит после template <>, а не перед ним: MSVC иначе не собирает
    // деприкейтед функцию-шаблон (error C2059 на <L_GRAMMAR_template_head_l>)
    template <class T>
    [[deprecated("use bsr<T>() instead")]]
    __inline uint32 bsrT(T val)
    {
        return bsr(val);
    }

    // наибольшая степень двойки, на которую делится val, - т.е. младший стоящий бит её
    // двоичного рисунка. У нуля такого делителя нет, но прежние перегрузки возвращали
    // 1, и это сохранено намеренно: единица - нейтральный ответ там, где результат
    // идёт множителем или величиной сдвига.
    //
    // T - любой целый, кроме bool; знаковый рассматривается как беззнаковый эквивалент
    // той же ширины (как у bsr), поэтому max_div_p2(int32(-128)) = 128, а у INT_MIN
    // результат - это битовый рисунок 2^(N-1), т.е. в знаковом T снова INT_MIN.
    //
    // Прежние перегрузки max_div_p2(uint32) и max_div_p2(uint64) искали этот бит
    // цепочкой из пяти-шести проверок и заменяются этим шаблоном напрямую: вызовы с
    // uint32/uint64 дают тот же T, а вызовы с int или size_t, которые прежней паре
    // перегрузок не нравились (неоднозначное приведение), теперь просто работают.
    // Отдельных деприкейтед-имён здесь нет - имя прежнее, устаревать нечему.
    //
    // Проверено MSVC (tmp/t_maxdiv.cpp, /W4 /permissive-, /O2, /O1 и /std:c++17): 4.1e6
    // значений - все ширины T от uint8 до int64, полный перебор для 8/16 бит, 0..65535 и
    // по 4e5 случайных для 32/64, степени двойки и их нечётные кратные, ноль и знаковый
    // бит. Ответ совпадает с независимым эталоном (цикл сдвигов, а не трюк) во всех
    // случаях и побитово с прежними телами на uint32 и uint64. MSVC /O2 x64: 7
    // инструкций без единого cmp и условного перехода - особенность нуля стоит один
    // setcc (uint8: 6 инструкций), против 36 инструкций и 10 операций сравнения и
    // ветвления у прежней uint32-цепочки и 46/11 у uint64; та же особенность нуля через
    // if вместо (low == 0) стоила бы 8 инструкций с cmp и условным переходом.
    template <class T>
    constexpr __inline T max_div_p2(T val) noexcept
    {
        static_assert(std::is_integral_v<T> && !std::is_same_v<T, bool>,
            "max_div_p2() supports only integer types");

        // W - беззнаковый аналог той же ширины; подбор ширины для bool нужен только
        // затем, чтобы тело не инстанцировалось поверх нашего static_assert
        using B = std::conditional_t<std::is_same_v<T, bool>, uint8, T>;
        using W = std::make_unsigned_t<B>;
        const W w = static_cast<W>(val);

        const W low = w & (W(0) - w);             //младший стоящий бит
        return static_cast<T>(low | (low == 0));  //у нуля низ пуст - ставим прежнюю 1
    }
    
    // подсчет единичных бит (popcount)
    // T - беззнаковый целый тип (uint8/uint16/uint32/uint64)
    template <class T>
    constexpr __inline T hbc(T x) noexcept
    {
        static_assert(std::is_unsigned_v<T>, "hbc() supports only unsigned integer types");
        static_assert(sizeof(T) <= 8, "hbc() supports only 8/16/32/64-bit types");

        // ВНИМАНИЕ: constexpr T m = ~T(0)/3 НЕ РАБОТАЕТ для T уже (uint8/uint16):
        // ~T(0) промоутится к int (-1), а (-1)/3 == 0, и constexpr-значение маски равно 0!
        // T(~T(0)) сначала возвращает промоутенное значение к родному размеру T
        constexpr T maxv = T(~T(0));
        constexpr T m1  = maxv / 3;     // 0x5555...
        constexpr T m2  = maxv / 5;     // 0x3333...
        constexpr T m4  = maxv / 17;    // 0x0F0F...
        constexpr T h01 = maxv / 0xFF;  // 0x0101...

        x -= (x >> 1) & m1;             // вместо двух '&' и '+' — один '&' и '-'
        x = (x & m2) + ((x >> 2) & m2);
        x = (x + (x >> 4)) & m4;        // маска один раз ПОСЛЕ сложения!

        if constexpr (sizeof(T) == 1)
            return x;

        // умножение на 0x0101... суммирует байты в старший байт
        // (сумма не более 8*sizeof(T) <= 64, переносов нет)
        return static_cast<T>(x * h01) >> (sizeof(T) * 8 - 8);
    }

    // деприкейтед: используйте hbc<uint32>()
    [[deprecated("use hbc<uint32>() instead")]]
    __inline uint32 hbc32(uint32 x)
    {
        return hbc(x);
    }

    __inline uint16 bswap16(uint16 x)
    {
        return uint16((x>>8) | (x<<8));
    }
    __inline int16 bswap16(int16 x)
    {
        return int16(uint16((uint16(x)>>8) | (uint16(x)<<8)));
    }
    __inline uint32 bswap32by16(uint32 x)
    {
        return (x>>16) | (x<<16);
    }
    __inline uint64 bswap64by16(uint64 x)
    {
        return (x>>48) | ((x>>16)&0xffff0000) | ((x&0xffff0000)<<16) | (x<<48);
    }
    __inline uint32 bswap32(uint32 x)
    {
        return (x>>24) | ((x>>8)&0x0000FF00L) | ((x&0x0000FF00L)<<8) | (x<<24);
    }
    __inline int32 bswap32(int32 x)
    {
        return int32(bswap32(uint32(x)));
    }
    __inline uint64 bswap64(uint64 x)
    {
        return (x>>56) | ((x>>40)&0x0000FF00) | ((x>>24)&0x00FF0000) | ((x>>8)&0xFF000000) |
                ((x&0xFF000000)<<8) | ((x&0x00FF0000)<<24) | ((x&0x0000FF00)<<40) | (x<<56);
    }
    __inline int64 bswap64(int64 x)
    {
        return int64(bswap64(uint64(x)));
    }
	
	// ==========================================================================
    // Универсальный байтовый swap с addr_xor
    //
    // Базовые bswap16/bswap32/bswap64 объявлены в amath_int.h (он же даёт
    // bswap32by16/bswap64by16 — обмен слов без разворота внутри слов).
    // Механизм — ксоризация адреса, тип T не имеет значения:
    //     out[i] = in[i ^ (addr_xor & (sizeof(T)-1))]
    // где in/out — указатели на объект T, приведённые к unsigned char*.
    // Биты адреса старше sizeof(T) у самого объекта не бывает, поэтому лишние
    // биты addr_xor отбрасываются маской: uint16 с addr_xor=3 даёт обмен байтов.
    //
    // addr_xor = 3 — «схемотехническая аксиома» для 32-битной шины
    //   sizeof(T) == 2, addr_xor == 1 -> a0         -> bswap16
    //   sizeof(T) == 4, addr_xor == 3 -> a1^a0      -> bswap32
    //   sizeof(T) == 8, addr_xor == 7 -> a2^a1^a0   -> bswap64
    //   sizeof(T) == 8, addr_xor == 3 -> a1^a0      -> разворот внутри каждого
    //                                   32-битного слова + обмен слов местами
    //                                   (так 64-битное значение читается с
    //                                   32-битной шины)
    //
    // Требование к типу одно: sizeof(T) — степень двойки (static_assert ниже),
    // иначе перестановка не переводит [0, sizeof(T)) в себя.
    //
    // Операция — инволюция (swap_bytes<swap_bytes(x)> == x), поэтому ctor,
    // присваивание и чтение endian используют один и тот же обмен.
    //
    // Быстрые пути дают ровно тот же результат, что и общий цикл, но /O2
    // помогает не всегда: сворачивание в ror/bswap видно в листинге
    // tmp/asm_probe.asm (3 инструкции), а побайтовый цикл остаётся побайтовым
    // (23 инструкции для swap_bytes<uint64,3>).
    // ==========================================================================

    template <typename T, uint8 addr_xor>
    __inline T swap_bytes(T x)
    {
        static_assert(sizeof(T) >= 1 && (sizeof(T) & (sizeof(T) - 1)) == 0,
            "swap_bytes: sizeof(T) must be a power of two (XOR of an address keeps it in range)");

        // Быстрые пути — только для целых типов: bswap16/32/64 перегружены для
        // uint16/int16, uint32/int32 и uint64/int64, так что с тем же float такой
        // вызов был бы неоднозначным. Несовпадение addr_xor с младшими битами
        // адреса (и нецелые T) отдают общий побайтовый цикл ниже.
        if constexpr (std::is_integral_v<T> && sizeof(T) == 2 && (addr_xor & 1) != 0)
            return reintrpret<T>(bswap16(reintrpret<uint16>(x)));
        else if constexpr (std::is_integral_v<T> && sizeof(T) == 4 && (addr_xor & 3) == 3)
            return reintrpret<T>(bswap32(reintrpret<uint32>(x)));
        else if constexpr (std::is_integral_v<T> && sizeof(T) == 8 && (addr_xor & 7) == 7)
            return reintrpret<T>(bswap64(reintrpret<uint64>(x)));
        else
        {
            static_assert(sizeof(unsigned char) == 1, "unexpected char size");

            const unsigned char *in = reinterpret_cast<const unsigned char*>(&x);

            T result = T();
            unsigned char *res = reinterpret_cast<unsigned char*>(&result);
            for (size_t i = 0; i < sizeof(T); ++i)
                res[i] = in[i ^ (addr_xor & (sizeof(T)-1))];
            return result;
        }
    }

    __inline uint32 rotr32(uint32 val, uint32 shift)
    {
        //на большинстве процессоров можно оптимизировать
        if(!shift)return val;  //бывают тупые компиляторы в которых сдвиг на ноль непредсказуем
        return (val>>shift)|(val<<(32-shift));
    }

    template <class T>
    __inline T rotr(T val, T shift)
    {
        shift&=(sizeof(T)*8-1);
        if(!shift)return val;
        return (val>>shift)|(val<<(sizeof(T)*8-shift));
    }

    template <class T>
    __inline T rotl(T val, T shift)
    {
        shift&=(sizeof(T)*8-1);
        if(!shift)return val;
        return (val<<shift)|(val>>(sizeof(T)*8-shift));
    }

    // целочисленный квадратный корень с округлением до ближайшего целого
    // T - беззнаковый целый тип (uint8/uint16/uint32/uint64)
    template <class T>
    constexpr __inline T sqrt(T val) noexcept
    {
        static_assert(std::is_unsigned_v<T>, "sqrt() supports only unsigned integer types");
        static_assert(sizeof(T) <= 8, "sqrt() supports only 8/16/32/64-bit types");

        if(!val)return T();

        // Разливаем единички от старшего значащего бита вниз до нуля: каждый
        // сдвиг меньше ширины T и удваивает длину залитого участка, поэтому шаг
        // через 1/2/4/8/16/32 закрывает любую ширину 8..64 бит.
        T x = val;
        x |= x >> 1;
        x |= x >> 2;
        x |= x >> 4;                            // 8 бит и шире
        if constexpr (sizeof(T) > 1) x |= x >> 8;   // 16 бит и шире
        if constexpr (sizeof(T) > 2) x |= x >> 16;  // 32 бита и шире
        if constexpr (sizeof(T) > 4) x |= x >> 32;  // 64 бита

        // mask = старшая степень 4, не превышающая val (число итераций цикла
        // ровно такое, каким его считала цепочка сдвигов 16/8/4/2).
        // Залитый участок - это 2^(b+1)-1, где b - номер старшего значащего бита,
        // поэтому x ^ (x >> 2) = (2^(b+1)-1) ^ (2^(b-1)-1) оставляет ровно два
        // его старших бита, позиции b и b-1; из этой пары соседних битов маска
        // чётных позиций выбирает нужную степень четвёрки. Отдельный msb не нужен.
        // Маска выводится из размера T, а не задаётся литералом.
        constexpr T even = T(~T(0)) / 3;    // 0x5555... (биты на чётных позициях)
        T mask = (x ^ (x >> 2)) & even;

        T root = 0;
        do   //цикл вычисления корня
        {
                T tmp = root|mask;
                root>>=1;
                if(tmp<=val)
                {
                        val-=tmp;
                        root|=mask;
                }
                mask>>=2;
        }while(mask);

        if(root<val)root++;   //округление до ближайшего целого

        return root;
    }

    // деприкейтед: используйте sqrt<uint32>()
    [[deprecated("use sqrt<uint32>() instead")]]
    __inline uint32 sqrt32( uint32 val )
    {
        return sqrt(val);
    }

    // ==========================================================================
    // Константы перевода log() из натурального логарифма в другое основание.
    // Всё строго целочисленное и вычисляется на этапе компиляции (constexpr),
    // ни одна из этих операций в рантайм не попадает.
    //
    // ln(base) = k*ln2 + 2*atanh((base - 2^k)/(base + 2^k)), где 2^k - ближайшая
    // степень двойки (то же правило арифметической середины, что в log()), поэтому
    // |x| <= 1/5 и ряд 2*(x + x^3/3 + x^5/5 + ...) убывает не медленнее чем в 25
    // раз за член: 14 членов достаточно для сетки Q56. ln2 = 2*atanh(1/3) - тоже
    // рядом, без литерала. Единственная нетривиальная операция - деление
    // 128-битного произведения на 64-битный делитель: столбиком по битам, без
    // __int128 и без плавающей точки. Проверено MSVC: всё это вычисляется на этапе
    // компиляции при лимитах constexpr по умолчанию (в том числе для основания
    // 2^32-1 - самого длинного из рядов), а не в рантайме.
    // ==========================================================================
    namespace logconst {

        // округлённое (a*b)/den: произведение собирается по 32-битным половинам,
        // частное - делением столбиком по битам (128-битной переменной нет).
        // Требует den < 2^63 (иначе r << 1 переполнится) и того, что частное
        // влезает в uint64; ниже оба условия выполняются во всех вызовах.
        constexpr uint64 mul_round_div(uint64 a, uint64 b, uint64 den) noexcept
        {
            const uint64 a0 = a & 0xFFFFFFFFULL, a1 = a >> 32;
            const uint64 b0 = b & 0xFFFFFFFFULL, b1 = b >> 32;
            const uint64 p00 = a0*b0, p01 = a0*b1, p10 = a1*b0, p11 = a1*b1;
            const uint64 mid = (p00 >> 32) + (p01 & 0xFFFFFFFFULL) + (p10 & 0xFFFFFFFFULL);
            const uint64 lo  = (p00 & 0xFFFFFFFFULL) | (mid << 32);  //младшие 64 бита
            const uint64 hi  = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);

            uint64 r = 0, q = 0;
            for(uint32 i = 128; i-- > 0;)
            {
                r = (r << 1) | ((i >= 64) ? ((hi >> (i - 64)) & 1) : ((lo >> i) & 1));
                q <<= 1;
                if(r >= den){ r -= den; q |= 1; }
            }
            if(r >= den - r)q += 1;       //остаток не меньше половины делителя
            return q;
        }

        // 2*atanh(p/q) в Q56; нужно p/q <= 1/5 - тогда члены убывают >= в 25 раз
        // и цикл гаснет за 14 итераций (pw строго убывает, t обращается в 0)
        constexpr uint64 atanh2_q56(uint64 p, uint64 q) noexcept
        {
            constexpr uint64 q56 = 1ULL << 56;
            const uint64 x  = mul_round_div(p, q56, q);       //x   * 2^56
            const uint64 x2 = mul_round_div(x, x, q56);       //x^2 * 2^56
            uint64 pw = x, sum = 0;
            for(uint32 n = 1;; n += 2)
            {
                const uint64 t = pw / n;
                if(!t)break;
                sum += t;
                pw = mul_round_div(pw, x2, q56);
            }
            return sum << 1;
        }

        // ln(2) в Q56 рядовый (2*atanh(1/3)), а не литерал
        inline constexpr uint64 LN2_Q56 = atanh2_q56(1, 3);

        // ln(base) в Q56, base >= 2
        constexpr uint64 ln_q56(uint32 base) noexcept
        {
            // base = 0/1 запрещены static_assert'ом в log(), но ветка base >= 3 у
            // него НЕ отбрасывается (1 не равно 0 и не равно 2), поэтому выражение
            // ниже инстанцируется и для них: без этого возврата сдвиг (k - 1) при
            // k = 0 ушёл бы за пределы ширины, и компилятор добавил бы к нашему
            // сообщению ещё и ошибку constexpr-вычисления
            if(base < 2)return 0;
            uint32 k = 0;
            for(uint64 t = base; t >> 1; t >>= 1)k++;             //floor(log2(base))
            if((uint64(base) >> (k - 1)) & 1)k++;                 //ближайшая степень 2
            const uint64 b = 1ULL << k;
            const uint64 d = (base > b) ? (base - b) : (b - base);
            const int64 add = int64(atanh2_q56(d, base + b));
            return uint64(int64(LN2_Q56)*int64(k) + ((base >= b) ? add : -add));
        }

        // множители перевода: 1/ln(base) в Q32 (у дробной части ряда) и
        // log_base(2) в Q45 (у целой). base >= 2; Q45 - из (k+sh) <= 64:
        // 64 * log_base(2)*2^45 < 2^51, а старший вариант (Q58) уже не влезал бы.
        template <uint32 base>
        inline constexpr uint64 inv_ln_q32 =
            mul_round_div(0x100000000ULL, 1ULL << 56, ln_q56(base));

        template <uint32 base>
        inline constexpr uint64 log2_q45 =
            mul_round_div(LN2_Q56, 1ULL << 45, ln_q56(base));

    } //namespace logconst

    // логарифм по основанию base в фикс-точке: возвращается log_base(val) * 2^rezfix
    // T - беззнаковый целый тип (uint8/uint16/uint32/uint64)
    // base = 0 - натуральный логарифм (он же по умолчанию), base = 2, base >= 3:
    //   log<uint32>(v, 24)   log<uint32, 2>(v, 24)   log<uint32, 10>(v, 24)
    // Натуральный по-прежнему доступен и под прежним именем ln<T>().
    //
    // val приводится к БЛИЖАЙШЕЙ степени двойки b = 2^k, и дальше
    //   ln(val) = k*ln2 + ln(val/b),   x = (val-b)/(val+b)  ->  val/b = (1+x)/(1-x)
    //   ln(val/b) = 2*(x + x^3/3 + x^5/5 + x^7/7 + x^9/9)
    // У прежней схемы x = (val-1)/(val+1), то есть x -> 1 при больших val и ряд
    // почти не сходился (сотни-тысячи итераций и процент ошибки). Здесь b выбрана
    // ближайшей, поэтому |x| <= 1/5 при любом val: отброшенный хвост 2*x^11/11 не
    // больше 3.9e-9. Число операций фиксированное: 5 умножений, 4 деления на константы
    // (3/5/7/9, компилятор сводит их к умножению) и одно настоящее деление - вместо цикла.
    //
    // Ряд считается в фиксированной сетке Q32, а не в Q(rezfix+1): сдвиги в нём
    // константные, а между сеткой и rezfix остаётся (32 - rezfix) защитных битов вместо
    // одного, так что усечением оказывается только сам результат. Двойка ряда вынесена в
    // множители <<1 (x < 2^30, переполнения от неё нет), иначе poly съедал бы бит сетки
    // (сетка Q31, сдвиг 31 - rezfix) и при rezfix = 31 финального округления не было бы
    // вовсе: -0.9 м.р. смещения и ни одного точного округления на этом rezfix.
    //
    // Перевод в другое основание - у двух слагаемых, каждое со своей constexpr-
    // константой (см. logconst выше), поэтому деления нет ни при каком base. Целая
    // часть (k+sh)*log_base(2) отделена от дробной poly/ln(base): первая доходит до
    // 64*log_base(2), поэтому её константа в Q45, а poly не больше ln(1.25) - ему
    // хватает Q32. Цена перевода: base = 0 - ровно как был ln (ни одного лишнего
    // действия), base = 2 - одно умножение, причём целая часть берётся вообще точно
    // (log_2(2) = 1, это просто (k+sh) << 32), base >= 3 - два умножения и два
    // сдвига вместо деления. Обе константы (namespace logconst) равны ТОЧНО
    // округлённому значению в своей сетке: сверка с точным rational ln(base) для 12
    // оснований даёт нулевое отклонение, относительная ошибка не больше 2.7e-9 у
    // Q32-множителя и 4.7e-13 у Q45. Вклад перевода в ответ при этом от base не
    // зависит: ошибка константы есть половина её собственного шага, т.е.
    // 0.5*ln(base)/2^32, а ответ равен ln(val)/ln(base), значит вклад не больше
    // 0.5*ln(val)/2^32 - это 0.04 м.р. при rezfix = 24 и 5.6 м.р. при rezfix = 31 для
    // uint32 (для uint64 - 0.09 и 11 м.р.).
    //
    // Проверено эмуляцией (7e5 значений uint32 и 2e5 значений uint64, все ширины T,
    // rezfix 0..31; основания 0, 2, 3, 10, 16 и дополнительно 4, 5, 37, 60, 100, 1024,
    // 2^20, 2^32-1). ln<T>() при этом побитово совпадает с прежним ln: 2.2e7 пар
    // (val, rezfix) без единого расхождения. Граница ошибки выводима из самого
    // алгоритма, подгоночных коэффициентов в проверке нет:
    //   |ошибка| <= ((2*x^11/11 + 8 шагов Q32)/ln(base)
    //                + 0.5*ln(max val)*(1/2^32 + 1/(ln2*2^45)))*2^rezfix + 0.5 м.р.
    // ряд и его хвост живут в натуральной сетке, поэтому при переводе делятся на
    // ln(base), а перевод добавляет слагаемое, от base не зависящее. Фактические
    // максимумы для uint32: 0.50 м.р. при rezfix <= 20, 0.53..0.61 при 24, 0.61..0.98
    // при 26, 0.96..1.76 при 28 (диапазон - по всем основаниям; для натурального
    // отдельно: 0.50, 0.58, 0.84, 1.62). Дальше ошибка растёт вдвое за шаг и ограничена
    // хвостом ряда (1.28 при 29, ~11 при 31). Это 24..27 верных бит мантиссы (у uint64
    // 24..30), т.е. ~8 значащих цифр против math.log (у сетки Q(rezfix+1) было 6.3 м.р.
    // и ~23 бита при rezfix 24). Значение ровно равно round(log*2^rezfix) для 100%
    // аргументов при rezfix <= 12, не ниже 99.8% при 20 и для 97.6..99.3% при 24
    // (прежде - 6.6% при 20): ниже 100% начинает сказываться двойное округление (poly
    // усечён в Q32, затем сдвиг к rezfix), доля таких случаев порядка 2^(rezfix-31).
    // Среднее смещение из-за усечений не больше 0.14 м.р. по модулю при rezfix <= 28.
    // Всё перечисленное подтверждено не только эмуляцией: MSVC 2022 (/W4 /permissive-,
    // /std:c++17 и /std:c++20, /O2 и /O1) собрал 2.2e4 вызовов log<T, base> на всех
    // ширинах T, основаниях 0/2/3/10/16/60/1024/2^20/2^32-1 и rezfix 0..40 - ответы
    // совпали с эмуляцией побитово, предупреждений в этом заголовке нет. 408
    // static_assert с теми же значениями компилятор посчитал сам, т.е. logconst
    // укладывается в constexpr-лимиты MSVC по умолчанию (без /constexpr:steps).
    // Вызов ln32()/sqrt32() даёт C4996, а log<uint32, 1> не собирается вовсе.
    // Точные степени двойки дают ровно round(j*log_base(2)*2^rezfix) при любом
    // основании: перебором всех j, rezfix 0..31 и всех T отклонений нет, хотя и k*ln2, и
    // (k+sh)*log2_q45 проходят свою ступень усечения в Q32 раньше финального округления.
    // Точные степени самого основания отличаются от целого j не больше чем на 0.24 от
    // приведённой выше границы. Монотонность по val сохранена: округляется одна
    // монотонная сумма - проверено на 4e5 подряд идущих значений при rezfix 8/16/24 для
    // каждого основания, на границах 2^j, 1.5*2^j, 2^32 и в окрестностях всех степеней
    // основания. Вариант "k*ln2 округлять сразу в сетку rezfix, а poly сдвигать в неё
    // же" даёт те же точные степени двойки, но роняет ln на 1 м.р. у 12 из 4e5 соседних
    // аргументов, т.е. ломает монотонность - он отвергнут.
    //
    // Все промежуточные - uint64, и они заведомо влезают (проверено на всех ширинах T,
    // rezfix 0..31 и на верхнем крае uint64):
    //   b <= 2^32 (k <= 32), значит diff <= b/2 <= 2^31 и diff << 32 <= 2^63;
    //   x <= 0.2*2^32 < 2^30, значит x*x < 2^60, x2 >= x3 >= x5 >= x7 >= x9, а poly
    //   не больше 2*ln(1.25)*2^32 < 2^31;
    //   (k + sh) <= 64, значит (k+sh)*ln2_q58 = ln2*2^64 < 2^64;
    //   у перевода: poly < 2^31, а log_base(2)*2^45 < 2^45 (base >= 3) и
    //   2^32/ln(base) < 2^32, значит poly*константа < 2^63 и (k+sh)*константа < 2^51;
    //   для base = 2 целая часть (k+sh) << 32 <= 2^38, а poly*log2(e)*2^32 не
    //   больше 2*log2(1.25)*2^64 = 0.65*2^64, то есть тоже влезает.
    // 64-битный аргумент заранее сворачивается в 32 значащих бита (val ~= v*2^sh),
    // иначе diff << 32 не помещается; погрешность сворачивания <= 2^-32 от ln,
    // т.е. <= 0.3 младшего разряда даже при rezfix = 31. Для uint8/16/32 эта ветка
    // мертва (там k <= 31), так что путь у алгоритма один для всех ширин.
    //
    // Ограничения:
    //   rezfix <= 31            - иначе сдвиг (32 - rezfix) обращается в 0 и тогда
    //                             (U(1) << (0 - 1)) уже UB, а результат в целое всё
    //                             равно не влезает; функция возвращает максимальное
    //                             значение make_signed_t<T>;
    //   log_base(val) * 2^rezfix < 2^(8*sizeof(T)-1) - иначе результат не влезает
    //                             в make_signed_t<T> (срезается статическим кастом);
    //   base = 0 (натуральный) либо base >= 2 - у основания 1 логарифма нет.
    // Особые случаи как у прежней версии: log(0) = "минус бесконечность"
    // (младший бит взведён), log(1) = 0 - они не зависят от base.
    template <class T, uint32 base = 0>
    constexpr __inline std::make_signed_t<T> log(T val, uint32 rezfix) noexcept
    {
        using S = std::make_signed_t<T>;
        using U = uint64;                 // широкие промежуточные для любой ширины T

        static_assert(std::is_unsigned_v<T>, "log() supports only unsigned integer types");
        static_assert(sizeof(T) <= 8, "log() supports only 8/16/32/64-bit types");
        static_assert(base == 0 || base >= 2, "log(): base 1 has no logarithm");

        if(!val)return static_cast<S>(T(1) << (sizeof(T)*8-1));    //ln(0) = -inf
        if(val==1)return S(0);
        if(rezfix>31)return static_cast<S>((T(1) << (sizeof(T)*8-1)) - 1);    //не влезет

        // floor(log2(val)) = bsr(val) - 1 (val == 0 исключён проверкой выше). bsr
        // считает в uint64, поэтому одной цепочкой покрываются все ширины T и своя
        // цепочка сдвигов здесь больше не нужна; аргумент при этом не портится, ниже
        // используется отдельная переменная v.
        uint32 k = bsr(U(val)) - 1;

        // Оставляем у аргумента 32 значащих бита: val ~= v * 2^sh (с округлением).
        // Округление прибавляется к уже сдвинутому значению, а НЕ к val: у верхнего
        // края диапазона (val близок к 2^64-1) val + (1 << (sh-1)) переполнился бы и
        // аргумент обнулился. Округление изредка поднимает разрядность v на единицу -
        // это учитывает проверка (v >> (kk+1)): тогда floor(log2(v)) на единицу больше.
        uint32 sh = (k > 31) ? (k - 31) : 0;
        U v = U(val);
        if(sh)
        {
            uint32 kk = k - sh;
            v = (U(val) >> sh) + ((U(val) >> (sh - 1)) & 1);
            k = kk + uint32(v >> (kk + 1));
        }

        // b - ближайшая степень двойки: если второй старший бит стоит, 2^(k+1) ближе,
        // чем 2^k, и именно это удерживает |x| <= 1/5.
        if((v >> (k - 1)) & 1)k++;
        U b = U(1) << k;
        bool neg = v < b;
        U diff = neg ? (b - v) : (v - b);

        //Ряд - в фиксированной сетке Q32, поэтому все сдвиги в нём константные, а между
        //сеткой и rezfix остаётся (32 - rezfix) защитных битов. Двойка ряда вынесена в
        //множители <<1, чтобы poly не терял бит сетки: в сетке Q31 сдвиг (31 - rezfix) при
        //rezfix = 31 обнулился бы, и округления не было бы вовсе.
        U x = (diff << 32) / (v + b);                 //x = (val-b)/(val+b) * 2^32
        U x2 = (x * x) >> 32;
        U x3 = (x * x2) >> 32;
        U x5 = (x3 * x2) >> 32;
        U x7 = (x5 * x2) >> 32;
        U x9 = (x7 * x2) >> 32;
        U poly = (x << 1) + ((x3 << 1)/3) + ((x5 << 1)/5)
               + ((x7 << 1)/7) + ((x9 << 1)/9);       //ln(val/b) * 2^32

        //Перевод в нужное основание - по множителю на слагаемое, деления нет. Оба
        //слагаемых в той же сетке Q32: целая часть (k+sh)*log_base(2), дробная
        //poly/ln(base). Здесь усечение: единственное округление к ближнему - внизу.
        U ip, fp;
        if constexpr (base == 0)
        {
            constexpr U ln2_q58 = 0x02C5C85FDF473DE7ULL;  //round(ln2*2^58)
            ip = (U(k + sh) * ln2_q58) >> 26;             //сдвиг 26 = (58 - 32)
            fp = poly;
        }
        else if constexpr (base == 2)
        {
            ip = U(k + sh) << 32;                         //log_2(2) = 1 - точно
            fp = (poly * logconst::inv_ln_q32<2>) >> 32;  //множитель log2(e)
        }
        else
        {
            ip = (U(k + sh) * logconst::log2_q45<base>) >> 13;    //Q45 -> Q32
            fp = (poly * logconst::inv_ln_q32<base>) >> 32;
        }
        U res = neg ? (ip - fp) : (ip + fp);              //log_base(val) * 2^32

        //(32 - rezfix) > 0 гарантировано проверкой rezfix > 31 выше, поэтому округление
        //есть при любом допустимом rezfix - включая 31.
        uint32 out = 32 - rezfix;
        return static_cast<S>((res + (U(1) << (out - 1))) >> out);
    }

    // натуральный логарифм - прежнее имя для log<T, 0>(...), побитово как прежде
    template <class T>
    constexpr __inline std::make_signed_t<T> ln(T val, uint32 rezfix) noexcept
    {
        return log<T, 0>(val, rezfix);
    }

    // деприкейтед: используйте ln<uint32>() либо log<uint32, base>()
    [[deprecated("use ln<uint32>() or log<uint32, base>() instead")]]
    __inline int32 ln32(uint32 val, uint32 rezfix)
    {
        return ln(val, rezfix);
    }

    template <class T>
    __inline T pow(T x, T p)
    {
        if (p < 0)
            return 1/pow(x,-p);

        if (p == 0) return 1;
        if (p == 1) return x;

        T tmp = pow(x, p/2);

        if (p%2 == 0) return tmp * tmp;
        else return x * tmp * tmp;
    }

} //namespace imath
} //namespace alt


#endif
