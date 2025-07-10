#include "asound.h"

#define BLOCK_SIZE 1024
#define BLOCK_COUNT 8

using namespace alt;
using namespace audio;

#if defined(ASOUND_ENABLE_WINMM)

#include <stdint.h>

#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

static HWAVEOUT hWaveOut = nullptr;
static WAVEHDR waveHeaders[BLOCK_COUNT];
static int16_t waveBuffers[BLOCK_COUNT][BLOCK_SIZE * 2]; // 2 канала
static int writeBlock = 0;
static int writePos = 0;
static int readBlock = 0;
static int freq = 44100;
static float sndalVolume = 1.0f;
static bool swapChannels = false;

// --- Сервисные функции ---

int alt::audio::utilization()
{
    int used = 0;
    for (int i = 0; i < BLOCK_COUNT; ++i)
        if (waveHeaders[i].dwFlags & WHDR_INQUEUE) used++;
    return (used * 100) / BLOCK_COUNT;
}

// --- Основные функции ---

bool alt::audio::init(int _freq)
{
    destroy();

    freq = _freq;
    writeBlock = writePos = readBlock = 0;

    WAVEFORMATEX wfx = {0};
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 2;
    wfx.nSamplesPerSec = freq;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = wfx.nChannels * wfx.wBitsPerSample / 8;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

    if (hWaveOut) destroy();

    if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
        return false;

    for (int i = 0; i < BLOCK_COUNT; ++i) {
        waveHeaders[i].lpData = (LPSTR)waveBuffers[i];
        waveHeaders[i].dwBufferLength = BLOCK_SIZE * 2 * sizeof(int16_t);
        waveHeaders[i].dwFlags = 0;
        waveHeaders[i].dwLoops = 0;
        waveOutPrepareHeader(hWaveOut, &waveHeaders[i], sizeof(WAVEHDR));
    }
    return true;
}

bool alt::audio::push(uint32_t val)
{
    if (writePos >= BLOCK_SIZE)
    {
        // Блок заполнен, отправляем
        if (waveHeaders[writeBlock].dwFlags & WHDR_INQUEUE)
            return false; // Нет свободных буферов

        waveHeaders[writeBlock].dwBufferLength = BLOCK_SIZE * 2 * sizeof(int16_t);
        waveOutWrite(hWaveOut, &waveHeaders[writeBlock], sizeof(WAVEHDR));
        writeBlock = (writeBlock + 1) % BLOCK_COUNT;
        writePos = 0;
    }

    if (swapChannels) val=(val>>16)|(val<<16);

    // Извлекаем левый и правый каналы (unsigned 16 bit)
    uint16_t l = (val & 0xFFFF);
    uint16_t r = (val >> 16);

    waveBuffers[writeBlock][writePos * 2 + 0] = l;
    waveBuffers[writeBlock][writePos * 2 + 1] = r;
    writePos++;
    return true;
}

bool alt::audio::ready()
{
    return !(waveHeaders[writeBlock].dwFlags & WHDR_INQUEUE);
}

bool alt::audio::flush()
{
    if (writePos == 0) return true;
    if (waveHeaders[writeBlock].dwFlags & WHDR_INQUEUE)
        return false;

    waveHeaders[writeBlock].dwBufferLength = writePos * 2 * sizeof(int16_t);
    waveOutWrite(hWaveOut, &waveHeaders[writeBlock], sizeof(WAVEHDR));
    writeBlock = (writeBlock + 1) % BLOCK_COUNT;
    writePos = 0;
    return true;
}

void alt::audio::destroy()
{
    if (!hWaveOut) return;
    waveOutReset(hWaveOut);
    for (int i = 0; i < BLOCK_COUNT; ++i)
        waveOutUnprepareHeader(hWaveOut, &waveHeaders[i], sizeof(WAVEHDR));
    waveOutClose(hWaveOut);
    hWaveOut = nullptr;
}

void alt::audio::set_volume(float val)
{
    if (val < 0.0f) val = 0.0f;
    if (val > 1.0f) val = 1.0f;
    sndalVolume = val;

    if (hWaveOut)
    {
        int32 vol = int32(val * 0xFFFF);
        if(vol > 0xFFFF)
            vol = 0xFFFF;
        if(vol<0)
            vol = 0;

        uint32 both = ((uint32)vol & 0xFFFF) | (((uint32)vol & 0xFFFF) << 16); // одинаково для L и R
        waveOutSetVolume(hWaveOut, both);
    }
}

void alt::audio::swap_channels(bool val)
{
    swapChannels = val;
}

#else

#ifdef __APPLE__
    #include <OpenAL/al.h>
    #include <OpenAL/alc.h>
#else
    #include <AL/al.h>
    #include <AL/alc.h>
#endif

static ALCdevice *pDevice = nullptr;
static ALCcontext *pContext;

static int FREQ_VAL_FREE=44100;

static ALuint	sndalBuffs[BLOCK_COUNT], sndalSource;
static uint32 sndalQueue[BLOCK_SIZE], sndalCount;

static bool SwapChannels=false;
static float sndalVolume = 1.0;

void alt::audio::set_volume(float val)
{
    sndalVolume = val;
	alSourcef (sndalSource, AL_GAIN,    val);
}

void alt::audio::swap_channels(bool val)
{
	SwapChannels=!val;
}

bool alt::audio::init(int freq)
{
 int i;

    destroy();
 
    FREQ_VAL_FREE=freq;

    // Позиция слушателя.
    ALfloat ListenerPos[] = { 0.0, 0.0, 0.0 };

    // Скорость слушателя.
    ALfloat ListenerVel[] = { 0.0, 0.0, 0.0 };

    // Ориентация слушателя. (Первые 3 элемента – направление «на», последние 3 – «вверх»)
    ALfloat ListenerOri[] = { 0.0, 0.0, -1.0,  0.0, 1.0, 0.0 };

    ALfloat SourcePos[] = { 0, 0, 1 };

    if(pDevice)destroy();

    // Открываем заданное по умолчанию устройство
    pDevice = alcOpenDevice(nullptr);
    // Проверка на ошибки
    if (!pDevice)
    {        
        return false;
    }
    // Создаем контекст рендеринга
    pContext = alcCreateContext(pDevice, nullptr);
    if ( alcGetError(pDevice) != ALC_NO_ERROR) return false;
  
    // Делаем контекст текущим
    alcMakeContextCurrent(pContext);

    // Устанавливаем параметры слушателя
    // Позиция
    alListenerfv(AL_POSITION,    ListenerPos);
    // Скорость
    alListenerfv(AL_VELOCITY,    ListenerVel);
    // Ориентация
    alListenerfv(AL_ORIENTATION, ListenerOri);

	sndalCount=0;
	alGenSources ( 1, &sndalSource );

	alSourcef (sndalSource, AL_PITCH,    1.0f);	//скорость
    alSourcef (sndalSource, AL_GAIN,    sndalVolume); //громкость
    alSourcefv(sndalSource, AL_POSITION,  SourcePos);
    alSourcefv(sndalSource, AL_VELOCITY,  ListenerVel);
    alSourcei (sndalSource, AL_LOOPING,  false);

	alGenBuffers(BLOCK_COUNT, sndalBuffs);
	
	sndalQueue[0]=0;

	for(i=0;i<BLOCK_COUNT;i++)
	{
        alBufferData(sndalBuffs[i], AL_FORMAT_STEREO16, sndalQueue, 4, FREQ_VAL_FREE);
	}

	alSourceQueueBuffers(sndalSource, BLOCK_COUNT, sndalBuffs);
	
	alSourcePlay(sndalSource);

    return true;

}
static int Processed = 0;
bool alt::audio::push(uint32 val)
{
 	if(sndalCount<(BLOCK_SIZE-1))
	{
        if(SwapChannels)
            val=(val>>16)|(val<<16);
        sndalQueue[sndalCount++]=val;
        return true;
	}
	else
	{
        ALuint      BufID;

		Processed=0;
		alGetSourcei(sndalSource, AL_BUFFERS_PROCESSED, &Processed);
		if(!Processed)
		{
            return false;
		}

        if(SwapChannels)val=(val>>16)|(val<<16);
        sndalQueue[sndalCount]=val;
                
        sndalCount=0;
		alSourceUnqueueBuffers(sndalSource, 1, &BufID);
		alBufferData(BufID, AL_FORMAT_STEREO16, sndalQueue, BLOCK_SIZE<<2, FREQ_VAL_FREE);
        alSourceQueueBuffers(sndalSource, 1, &BufID);

		alGetSourcei(sndalSource, AL_SOURCE_STATE, (int*)&BufID);
		if(BufID!=AL_PLAYING)
		{
			alSourcePlay(sndalSource);
		}
	}
    return true;
}

bool alt::audio::ready()
{
    Processed=0;
    alGetSourcei(sndalSource, AL_BUFFERS_PROCESSED, &Processed);
    return Processed;
}

bool alt::audio::flush()
{
    if(!sndalCount)return true;

    if(ready())
    {
        ALuint      BufID;

        alSourceUnqueueBuffers(sndalSource, 1, &BufID);
        alBufferData(BufID, AL_FORMAT_STEREO16, sndalQueue, sndalCount<<2, FREQ_VAL_FREE);
        alSourceQueueBuffers(sndalSource, 1, &BufID);

        sndalCount=0;

        alGetSourcei(sndalSource, AL_SOURCE_STATE, (int*)&BufID);
        if(BufID!=AL_PLAYING)
        {
            alSourcePlay(sndalSource);
        }
        return true;
    }
    return false;
}

void alt::audio::destroy()
{
    if(!pDevice)
        return;
    alSourceStop ( sndalSource );
    // Очищаем все буффера
    alDeleteBuffers(BLOCK_COUNT, sndalBuffs);
    alDeleteSources(1, &sndalSource);
    // Выключаем текущий контекст
    alcMakeContextCurrent(NULL);
    // Уничтожаем контекст
    alcDestroyContext(pContext);
    // Закрываем звуковое устройство
    alcCloseDevice(pDevice);
    pDevice=nullptr;
}

int alt::audio::utilization()
{       
    return (100*(BLOCK_COUNT-Processed))/BLOCK_COUNT;
}

#endif
