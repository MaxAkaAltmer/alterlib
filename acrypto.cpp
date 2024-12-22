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

#include "acrypto.h"
#include "crypto/crc32.h"
#include "amath_int.h"

#include "external/cryptopp/pch.h"
#include "external/cryptopp/config.h"

#if CRYPTOPP_MSC_VERSION
# pragma warning(disable: 4127 4189 4459)
#endif

#if CRYPTOPP_GCC_DIAGNOSTIC_AVAILABLE
# pragma GCC diagnostic ignored "-Wunused-value"
# pragma GCC diagnostic ignored "-Wunused-variable"
# pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

#ifndef CRYPTOPP_IMPORTS

#include "external/cryptopp/cryptlib.h"
#include "external/cryptopp/filters.h"
#include "external/cryptopp/algparam.h"
#include "external/cryptopp/fips140.h"
#include "external/cryptopp/argnames.h"
#include "external/cryptopp/fltrimpl.h"
#include "external/cryptopp/osrng.h"
#include "external/cryptopp/secblock.h"
#include "external/cryptopp/smartptr.h"
#include "external/cryptopp/stdcpp.h"
#include "external/cryptopp/misc.h"
#include "external/cryptopp/algebra.cpp"

NAMESPACE_BEGIN(CryptoPP)

CRYPTOPP_COMPILE_ASSERT(sizeof(byte) == 1);
CRYPTOPP_COMPILE_ASSERT(sizeof(word16) == 2);
CRYPTOPP_COMPILE_ASSERT(sizeof(word32) == 4);
CRYPTOPP_COMPILE_ASSERT(sizeof(word64) == 8);
#ifdef CRYPTOPP_NATIVE_DWORD_AVAILABLE
CRYPTOPP_COMPILE_ASSERT(sizeof(dword) == 2*sizeof(word));
#endif

BufferedTransformation & TheBitBucket()
{
    static BitBucket bitBucket;
    return bitBucket;
}

template class AbstractRing<Integer>;
template class AbstractGroup<Integer>;
template class AbstractEuclideanDomain<Integer>;

/// \brief Random Number Generator that does not produce random numbers
/// \details ClassNullRNG can be used for functions that require a RandomNumberGenerator
///   but don't actually use it. The class throws NotImplemented when a generation function is called.
/// \sa NullRNG()
class ClassNullRNG : public RandomNumberGenerator
{
public:
    /// \brief The name of the generator
    /// \returns the string \a NullRNGs
    std::string AlgorithmName() const {return "NullRNG";}

#if defined(CRYPTOPP_DOXYGEN_PROCESSING)
    /// \brief An implementation that throws NotImplemented
    byte GenerateByte () {}
    /// \brief An implementation that throws NotImplemented
    unsigned int GenerateBit () {}
    /// \brief An implementation that throws NotImplemented
    word32 GenerateWord32 (word32 min, word32 max) {}
#endif

    /// \brief An implementation that throws NotImplemented
    void GenerateBlock(byte *output, size_t size)
    {
        CRYPTOPP_UNUSED(output); CRYPTOPP_UNUSED(size);
        throw NotImplemented("NullRNG: NullRNG should only be passed to functions that don't need to generate random bytes");
    }

#if defined(CRYPTOPP_DOXYGEN_PROCESSING)
    /// \brief An implementation that throws NotImplemented
    void GenerateIntoBufferedTransformation (BufferedTransformation &target, const std::string &channel, lword length) {}
    /// \brief An implementation that throws NotImplemented
    void IncorporateEntropy (const byte *input, size_t length) {}
    /// \brief An implementation that returns \p false
    bool CanIncorporateEntropy () const {}
    /// \brief An implementation that does nothing
    void DiscardBytes (size_t n) {}
    /// \brief An implementation that does nothing
    void Shuffle (IT begin, IT end) {}

private:
    Clonable* Clone () const { return NULLPTR; }
#endif
};

RandomNumberGenerator & NullRNG()
{
    static ClassNullRNG s_nullRNG;
    return s_nullRNG;
}

unsigned int HashTransformation::OptimalDataAlignment() const
{
    return GetAlignmentOf<word32>();
}

bool HashTransformation::TruncatedVerify(const byte *digest, size_t digestLength)
{
    // Allocate at least 1 for calculated to avoid triggering diagnostics
    ThrowIfInvalidTruncatedSize(digestLength);
    SecByteBlock calculated(digestLength ? digestLength : 1);
    TruncatedFinal(calculated, digestLength);
    return VerifyBufsEqual(calculated, digest, digestLength);
}

void HashTransformation::ThrowIfInvalidTruncatedSize(size_t size) const
{
    if (size > DigestSize())
        throw InvalidArgument("HashTransformation: can't truncate a " + IntToString(DigestSize()) + " byte digest to " + IntToString(size) + " bytes");
}

unsigned int BufferedTransformation::GetMaxWaitObjectCount() const
{
    const BufferedTransformation *t = AttachedTransformation();
    return t ? t->GetMaxWaitObjectCount() : 0;
}

void BufferedTransformation::GetWaitObjects(WaitObjectContainer &container, CallStack const& callStack)
{
    BufferedTransformation *t = AttachedTransformation();
    if (t)
        t->GetWaitObjects(container, callStack);  // reduce clutter by not adding to stack here
}

void BufferedTransformation::Initialize(const NameValuePairs &parameters, int propagation)
{
    CRYPTOPP_UNUSED(propagation);
    CRYPTOPP_ASSERT(!AttachedTransformation());
    IsolatedInitialize(parameters);
}

bool BufferedTransformation::Flush(bool hardFlush, int propagation, bool blocking)
{
    CRYPTOPP_UNUSED(propagation);
    CRYPTOPP_ASSERT(!AttachedTransformation());
    return IsolatedFlush(hardFlush, blocking);
}

bool BufferedTransformation::MessageSeriesEnd(int propagation, bool blocking)
{
    CRYPTOPP_UNUSED(propagation);
    CRYPTOPP_ASSERT(!AttachedTransformation());
    return IsolatedMessageSeriesEnd(blocking);
}

byte * BufferedTransformation::ChannelCreatePutSpace(const std::string &channel, size_t &size)
{
    byte* space = NULLPTR;
    if (channel.empty())
        space = CreatePutSpace(size);
    else
        throw NoChannelSupport(AlgorithmName());
    return space;
}

size_t BufferedTransformation::ChannelPut2(const std::string &channel, const byte *inString, size_t length, int messageEnd, bool blocking)
{
    size_t size = 0;
    if (channel.empty())
        size = Put2(inString, length, messageEnd, blocking);
    else
        throw NoChannelSupport(AlgorithmName());
    return size;
}

size_t BufferedTransformation::ChannelPutModifiable2(const std::string &channel, byte *inString, size_t length, int messageEnd, bool blocking)
{
    size_t size = 0;
    if (channel.empty())
        size = PutModifiable2(inString, length, messageEnd, blocking);
    else
        size = ChannelPut2(channel, inString, length, messageEnd, blocking);
    return size;
}

bool BufferedTransformation::ChannelFlush(const std::string &channel, bool hardFlush, int propagation, bool blocking)
{
    bool result = 0;
    if (channel.empty())
        result = Flush(hardFlush, propagation, blocking);
    else
        throw NoChannelSupport(AlgorithmName());
    return result;
}

bool BufferedTransformation::ChannelMessageSeriesEnd(const std::string &channel, int propagation, bool blocking)
{
    bool result = false;
    if (channel.empty())
        result = MessageSeriesEnd(propagation, blocking);
    else
        throw NoChannelSupport(AlgorithmName());
    return result;
}

lword BufferedTransformation::MaxRetrievable() const
{
    lword size = 0;
    if (AttachedTransformation())
        size = AttachedTransformation()->MaxRetrievable();
    else
        size = CopyTo(TheBitBucket());
    return size;
}

bool BufferedTransformation::AnyRetrievable() const
{
    bool result = false;
    if (AttachedTransformation())
        result = AttachedTransformation()->AnyRetrievable();
    else
    {
        byte b;
        result = Peek(b) != 0;
    }
    return result;
}

size_t BufferedTransformation::Get(byte &outByte)
{
    size_t size = 0;
    if (AttachedTransformation())
        size = AttachedTransformation()->Get(outByte);
    else
        size = Get(&outByte, 1);
    return size;
}

size_t BufferedTransformation::Get(byte *outString, size_t getMax)
{
    size_t size = 0;
    if (AttachedTransformation())
        size = AttachedTransformation()->Get(outString, getMax);
    else
    {
        ArraySink arraySink(outString, getMax);
        size = (size_t)TransferTo(arraySink, getMax);
    }
    return size;
}

size_t BufferedTransformation::Peek(byte &outByte) const
{
    size_t size = 0;
    if (AttachedTransformation())
        size = AttachedTransformation()->Peek(outByte);
    else
        size = Peek(&outByte, 1);
    return size;
}

size_t BufferedTransformation::Peek(byte *outString, size_t peekMax) const
{
    size_t size = 0;
    if (AttachedTransformation())
        size = AttachedTransformation()->Peek(outString, peekMax);
    else
    {
        ArraySink arraySink(outString, peekMax);
        size = (size_t)CopyTo(arraySink, peekMax);
    }
    return size;
}

lword BufferedTransformation::Skip(lword skipMax)
{
    lword size = 0;
    if (AttachedTransformation())
        size = AttachedTransformation()->Skip(skipMax);
    else
        size = TransferTo(TheBitBucket(), skipMax);
    return size;
}

lword BufferedTransformation::TotalBytesRetrievable() const
{
    lword size = 0;
    if (AttachedTransformation())
        size = AttachedTransformation()->TotalBytesRetrievable();
    else
        size = MaxRetrievable();
    return size;
}

unsigned int BufferedTransformation::NumberOfMessages() const
{
    unsigned int size = 0;
    if (AttachedTransformation())
        size = AttachedTransformation()->NumberOfMessages();
    else
        size = CopyMessagesTo(TheBitBucket());
    return size;
}

bool BufferedTransformation::AnyMessages() const
{
    bool result = false;
    if (AttachedTransformation())
        result = AttachedTransformation()->AnyMessages();
    else
        result = NumberOfMessages() != 0;
    return result;
}

bool BufferedTransformation::GetNextMessage()
{
    bool result = false;
    if (AttachedTransformation())
        result = AttachedTransformation()->GetNextMessage();
    else
    {
        CRYPTOPP_ASSERT(!AnyMessages());
    }
    return result;
}

unsigned int BufferedTransformation::SkipMessages(unsigned int count)
{
    unsigned int size = 0;
    if (AttachedTransformation())
        size = AttachedTransformation()->SkipMessages(count);
    else
        size = TransferMessagesTo(TheBitBucket(), count);
    return size;
}

size_t BufferedTransformation::TransferMessagesTo2(BufferedTransformation &target, unsigned int &messageCount, const std::string &channel, bool blocking)
{
    if (AttachedTransformation())
        return AttachedTransformation()->TransferMessagesTo2(target, messageCount, channel, blocking);
    else
    {
        unsigned int maxMessages = messageCount;
        for (messageCount=0; messageCount < maxMessages && AnyMessages(); messageCount++)
        {
            size_t blockedBytes;
            lword transferredBytes;

            while (AnyRetrievable())
            {
                // MaxRetrievable() instead of LWORD_MAX due to GH #962. If
                // the target calls CreatePutSpace(), then the allocation
                // size will be LWORD_MAX. That happens when target is a
                // ByteQueue. Maybe ByteQueue should check the size, and if
                // it is LWORD_MAX or -1, then use a default like 4096.
                transferredBytes = MaxRetrievable();
                blockedBytes = TransferTo2(target, transferredBytes, channel, blocking);
                if (blockedBytes > 0)
                    return blockedBytes;
            }

            if (target.ChannelMessageEnd(channel, GetAutoSignalPropagation(), blocking))
                return 1;

            bool result = GetNextMessage();
            CRYPTOPP_UNUSED(result); CRYPTOPP_ASSERT(result);
        }
        return 0;
    }
}

unsigned int BufferedTransformation::CopyMessagesTo(BufferedTransformation &target, unsigned int count, const std::string &channel) const
{
    unsigned int size = 0;
    if (AttachedTransformation())
        size = AttachedTransformation()->CopyMessagesTo(target, count, channel);
    return size;
}

void BufferedTransformation::SkipAll()
{
    if (AttachedTransformation())
        AttachedTransformation()->SkipAll();
    else
    {
        while (SkipMessages()) {}
        while (Skip()) {}
    }
}

size_t BufferedTransformation::TransferAllTo2(BufferedTransformation &target, const std::string &channel, bool blocking)
{
    if (AttachedTransformation())
        return AttachedTransformation()->TransferAllTo2(target, channel, blocking);
    else
    {
        CRYPTOPP_ASSERT(!NumberOfMessageSeries());

        unsigned int messageCount;
        do
        {
            messageCount = UINT_MAX;
            size_t blockedBytes = TransferMessagesTo2(target, messageCount, channel, blocking);
            if (blockedBytes)
                return blockedBytes;
        }
        while (messageCount != 0);

        lword byteCount;
        do
        {
            byteCount = ULONG_MAX;
            size_t blockedBytes = TransferTo2(target, byteCount, channel, blocking);
            if (blockedBytes)
                return blockedBytes;
        }
        while (byteCount != 0);

        return 0;
    }
}

void BufferedTransformation::CopyAllTo(BufferedTransformation &target, const std::string &channel) const
{
    if (AttachedTransformation())
        AttachedTransformation()->CopyAllTo(target, channel);
    else
    {
        CRYPTOPP_ASSERT(!NumberOfMessageSeries());
        while (CopyMessagesTo(target, UINT_MAX, channel)) {}
    }
}

void BufferedTransformation::SetRetrievalChannel(const std::string &channel)
{
    if (AttachedTransformation())
        AttachedTransformation()->SetRetrievalChannel(channel);
}

size_t BufferedTransformation::ChannelPutWord16(const std::string &channel, word16 value, ByteOrder order, bool blocking)
{
    PutWord(false, order, m_buf, value);
    return ChannelPut(channel, m_buf, 2, blocking);
}

size_t BufferedTransformation::ChannelPutWord32(const std::string &channel, word32 value, ByteOrder order, bool blocking)
{
    PutWord(false, order, m_buf, value);
    return ChannelPut(channel, m_buf, 4, blocking);
}

size_t BufferedTransformation::ChannelPutWord64(const std::string &channel, word64 value, ByteOrder order, bool blocking)
{
    PutWord(false, order, m_buf, value);
    return ChannelPut(channel, m_buf, 8, blocking);
}

size_t BufferedTransformation::PutWord16(word16 value, ByteOrder order, bool blocking)
{
    return ChannelPutWord16(DEFAULT_CHANNEL, value, order, blocking);
}

size_t BufferedTransformation::PutWord32(word32 value, ByteOrder order, bool blocking)
{
    return ChannelPutWord32(DEFAULT_CHANNEL, value, order, blocking);
}

size_t BufferedTransformation::PutWord64(word64 value, ByteOrder order, bool blocking)
{
    return ChannelPutWord64(DEFAULT_CHANNEL, value, order, blocking);
}

size_t BufferedTransformation::PeekWord16(word16 &value, ByteOrder order) const
{
    byte buf[2] = {0, 0};
    size_t len = Peek(buf, 2);

    if (order == BIG_ENDIAN_ORDER)
        value = word16((buf[0] << 8) | buf[1]);
    else
        value = word16((buf[1] << 8) | buf[0]);

    return len;
}

size_t BufferedTransformation::PeekWord32(word32 &value, ByteOrder order) const
{
    byte buf[4] = {0, 0, 0, 0};
    size_t len = Peek(buf, 4);

    if (order == BIG_ENDIAN_ORDER)
        value = word32((buf[0] << 24) | (buf[1] << 16) |
                       (buf[2] << 8)  | (buf[3] << 0));
    else
        value = word32((buf[3] << 24) | (buf[2] << 16) |
                       (buf[1] << 8)  | (buf[0] << 0));

    return len;
}

size_t BufferedTransformation::PeekWord64(word64 &value, ByteOrder order) const
{
    byte buf[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    size_t len = Peek(buf, 8);

    if (order == BIG_ENDIAN_ORDER)
        value = ((word64)buf[0] << 56) | ((word64)buf[1] << 48) | ((word64)buf[2] << 40) |
                ((word64)buf[3] << 32) | ((word64)buf[4] << 24) | ((word64)buf[5] << 16) |
                ((word64)buf[6] << 8)  |  (word64)buf[7];
    else
        value = ((word64)buf[7] << 56) | ((word64)buf[6] << 48) | ((word64)buf[5] << 40) |
                ((word64)buf[4] << 32) | ((word64)buf[3] << 24) | ((word64)buf[2] << 16) |
                ((word64)buf[1] << 8)  |  (word64)buf[0];

    return len;
}

size_t BufferedTransformation::GetWord16(word16 &value, ByteOrder order)
{
    return (size_t)Skip(PeekWord16(value, order));
}

size_t BufferedTransformation::GetWord32(word32 &value, ByteOrder order)
{
    return (size_t)Skip(PeekWord32(value, order));
}

size_t BufferedTransformation::GetWord64(word64 &value, ByteOrder order)
{
    return (size_t)Skip(PeekWord64(value, order));
}

void BufferedTransformation::Attach(BufferedTransformation *newAttachment)
{
    if (AttachedTransformation() && AttachedTransformation()->Attachable())
        AttachedTransformation()->Attach(newAttachment);
    else
        Detach(newAttachment);
}

unsigned int RandomNumberGenerator::GenerateBit()
{
    return GenerateByte() & 1;
}

byte RandomNumberGenerator::GenerateByte()
{
    byte b;
    GenerateBlock(&b, 1);
    return b;
}

word32 RandomNumberGenerator::GenerateWord32(word32 min, word32 max)
{
    const word32 range = max-min;
    const unsigned int maxBits = BitPrecision(range);

    word32 value;

    do
    {
        GenerateBlock((byte *)&value, sizeof(value));
        value = Crop(value, maxBits);
    } while (value > range);

    return value+min;
}

// Stack recursion below... GenerateIntoBufferedTransformation calls GenerateBlock,
// and GenerateBlock calls GenerateIntoBufferedTransformation. Ad infinitum. Also
// see http://github.com/weidai11/cryptopp/issues/38.
//
// According to Wei, RandomNumberGenerator is an interface, and it should not
// be instantiable. Its now spilt milk, and we are going to CRYPTOPP_ASSERT it in Debug
// builds to alert the programmer and throw in Release builds. Developers have
// a reference implementation in case its needed. If a programmer
// unintentionally lands here, then they should ensure use of a
// RandomNumberGenerator pointer or reference so polymorphism can provide the
// proper runtime dispatching.

void RandomNumberGenerator::GenerateBlock(byte *output, size_t size)
{
    CRYPTOPP_UNUSED(output), CRYPTOPP_UNUSED(size);

    ArraySink s(output, size);
    GenerateIntoBufferedTransformation(s, DEFAULT_CHANNEL, size);
}

void RandomNumberGenerator::DiscardBytes(size_t n)
{
    GenerateIntoBufferedTransformation(TheBitBucket(), DEFAULT_CHANNEL, n);
}

void RandomNumberGenerator::GenerateIntoBufferedTransformation(BufferedTransformation &target, const std::string &channel, lword length)
{
    FixedSizeSecBlock<byte, 256> buffer;
    while (length)
    {
        size_t len = UnsignedMin(buffer.size(), length);
        GenerateBlock(buffer, len);
        (void)target.ChannelPut(channel, buffer, len);
        length -= len;
    }
}

class NullNameValuePairs : public NameValuePairs
{
public:
    NullNameValuePairs() {}    //  Clang complains a default ctor must be available
    bool GetVoidValue(const char *name, const std::type_info &valueType, void *pValue) const
        {CRYPTOPP_UNUSED(name); CRYPTOPP_UNUSED(valueType); CRYPTOPP_UNUSED(pValue); return false;}
};

#if HAVE_GCC_INIT_PRIORITY
  const std::string DEFAULT_CHANNEL __attribute__ ((init_priority (CRYPTOPP_INIT_PRIORITY + 25))) = "";
  const std::string AAD_CHANNEL __attribute__ ((init_priority (CRYPTOPP_INIT_PRIORITY + 26))) = "AAD";
  const NullNameValuePairs s_nullNameValuePairs __attribute__ ((init_priority (CRYPTOPP_INIT_PRIORITY + 27)));
  const NameValuePairs& g_nullNameValuePairs = s_nullNameValuePairs;
#elif HAVE_MSC_INIT_PRIORITY
  #pragma warning(disable: 4073)
  #pragma init_seg(lib)
  const std::string DEFAULT_CHANNEL = "";
  const std::string AAD_CHANNEL = "AAD";
  const NullNameValuePairs s_nullNameValuePairs;
  const NameValuePairs& g_nullNameValuePairs = s_nullNameValuePairs;
  #pragma warning(default: 4073)
#elif HAVE_XLC_INIT_PRIORITY
  #pragma priority(260)
  const std::string DEFAULT_CHANNEL = "";
  const std::string AAD_CHANNEL = "AAD";
  const NullNameValuePairs s_nullNameValuePairs;
  const NameValuePairs& g_nullNameValuePairs = s_nullNameValuePairs;
#else
  const std::string DEFAULT_CHANNEL = "";
  const std::string AAD_CHANNEL = "AAD";
  const simple_ptr<NullNameValuePairs> s_pNullNameValuePairs(new NullNameValuePairs);
  const NameValuePairs &g_nullNameValuePairs = *s_pNullNameValuePairs.m_p;
#endif

NAMESPACE_END  // CryptoPP

#endif

#include "external/cryptopp/md5.h"

using namespace alt;

cryptoHash::cryptoHash(HashFunction method)
    : hashfun(method)
{
    switch(hashfun)
    {
    case MD5:
        context = new CryptoPP::Weak1::MD5;
        break;
    case SHA1:
        context = new CryptoPP::SHA1;
        break;
    default: //CRC32
        context = new uint32;
        break;
    }
    reset();
}

cryptoHash::~cryptoHash()
{
    switch(hashfun)
    {
    case MD5:
        delete (CryptoPP::Weak1::MD5*)context;
        break;
    case SHA1:
        delete (CryptoPP::SHA1*)context;
        break;
    default: //CRC32
        delete (uint32*)context;
        break;
    }
}

void cryptoHash::reset()
{
    hashdata.clear();
    switch(hashfun)
    {
    case MD5:
        ((CryptoPP::Weak1::MD5*)context)->Restart();
        break;
    case SHA1:
        ((CryptoPP::SHA1*)context)->Restart();
        break;
    default: //CRC32
        *(uint32*)context=0xffffffff;
        break;
    }
}

void cryptoHash::append(const alt::byteArray &data)
{
    switch(hashfun)
    {
    case MD5:
        ((CryptoPP::Weak1::MD5*)context)->Update(data(),data.size());
        break;
    case SHA1:
        ((CryptoPP::SHA1*)context)->Update(data(),data.size());
        break;
    default: //CRC32
        *(uint32*)context=__crc32(data(),data.size(),*(uint32*)context);
        break;
    }
}

void cryptoHash::append(void *buff, int size)
{
    switch(hashfun)
    {
    case MD5:
        ((CryptoPP::Weak1::MD5*)context)->Update((uint8*)buff,size);
        break;
    case SHA1:
        ((CryptoPP::SHA1*)context)->Update((uint8*)buff,size);
        break;
    default: //CRC32
        *(uint32*)context=__crc32(buff,size,*(uint32*)context);
        break;
    }
}

alt::byteArray cryptoHash::result()
{
    if(hashdata.isEmpty())
    {
        uint8 buff[32];
        switch(hashfun)
        {
        case MD5:
            ((CryptoPP::Weak1::MD5*)context)->Final(buff);
            hashdata.append(buff,16);
            break;
        case SHA1:
            ((CryptoPP::SHA1*)context)->Final(buff);
            hashdata.append(buff,20);
            break;
        default: //CRC32
            hashdata.appendT(*(uint32*)context);
            break;
        }
    }
    return hashdata;
}

uint32 cryptoHash::makeCRC32(void *buff, int size)
{
    return __crc32(buff,size);
}
