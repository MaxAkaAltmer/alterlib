/*****************************************************************************

This is part of Alterlib - the free code collection under the MIT License
------------------------------------------------------------------------------
Copyright (C) 2006-2025 Maxim L. Grishin  (altmer@arts-union.ru)

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

#include "asound.h"

#define BLOCK_SIZE 1024
#define BLOCK_COUNT 8

using namespace alt;
using namespace audio;

#if defined(ASOUND_ENABLE_MINIAUDIO)

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio/miniaudio.h>
#include <vector>
#include <algorithm> // для std::clamp

#include <alterlib/at_ring.h>

// Static variables
static ma_device device;
static ring<uint32> samples_buffer(BLOCK_SIZE * BLOCK_COUNT);
static volatile bool pause_play_state = false;
static bool SwapChannels = false;
static float sndalVolume = 1.0f;

// Callback для заполнения аудио-буфера
void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    (void)pDevice;
    (void)pInput; // Не используется в режиме воспроизведения

    ma_int16* output = static_cast<ma_int16*>(pOutput);

    for (ma_uint32 i = 0; i < frameCount; ++i)
    {
        if (samples_buffer.Size() < frameCount) {
            // Буфер пуст — пишем тишину
            *output++ = 0;
            *output++ = 0;
            continue;
        }

        uint32 val = samples_buffer.Get();
        if (SwapChannels) {
            val = (val >> 16) | (val << 16); // Меняем каналы
        }

        // Извлечение левого и правого канала из uint32
        ma_int16 left = static_cast<ma_int16>(val & 0xFFFF);
        ma_int16 right = static_cast<ma_int16>((val >> 16) & 0xFFFF);

        // Применение громкости
        left = static_cast<ma_int16>(left * sndalVolume);
        right = static_cast<ma_int16>(right * sndalVolume);

        *output++ = left;
        *output++ = right;
    }
}

// Установка громкости
void alt::audio::set_volume(float val)
{
    val = std::clamp(val, 0.0f, 1.0f); // Ограничение диапазона
    sndalVolume = val;
}

// Переключение каналов
void alt::audio::swap_channels(bool val)
{
    SwapChannels = val;
}

// Использование буфера в процентах
int alt::audio::utilization()
{
    return (100*BLOCK_SIZE*BLOCK_COUNT)/samples_buffer.Size();
}

// Инициализация аудио-устройства
bool alt::audio::init(int _freq)
{
    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format = ma_format_s16; // 16-битный PCM
    deviceConfig.playback.channels = 2;           // Стерео
    deviceConfig.sampleRate = _freq;
    deviceConfig.dataCallback = data_callback;
    deviceConfig.pUserData = NULL;

    if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS) {
        return false;
    }

    if (ma_device_start(&device) != MA_SUCCESS) {
        ma_device_uninit(&device);
        return false;
    }

    return true;
}

// Проверка готовности к записи
bool alt::audio::ready()
{
    return samples_buffer.Allow();
}

// Очистка буфера
bool alt::audio::flush()
{
    return samples_buffer.Allow();
}

// Пауза воспроизведения
void alt::audio::pause(bool pause)
{
    if (pause) {
        ma_device_stop(&device);
    } else {
        ma_device_start(&device);
    }
    pause_play_state = pause;
}

// Освобождение ресурсов
void alt::audio::destroy()
{
    ma_device_uninit(&device);
}

// Добавление данных в буфер
bool alt::audio::push(uint32 val)
{
    return samples_buffer.Push(val);
}

#elif defined(ANDROID_NDK)

#include <assert.h>

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include <alterlib/at_ring.h>

// engine interfaces
static SLObjectItf engineObject = NULL;
static SLEngineItf engineEngine = NULL;
static SLObjectItf outputMixObject = NULL;

// buffer queue player interfaces
static SLObjectItf bqPlayerObject = NULL;
static SLPlayItf bqPlayerPlay = NULL;
static SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue = NULL;
static SLMuteSoloItf bqPlayerMuteSolo = NULL;
static SLVolumeItf bqPlayerVolume = NULL;
#define SL_BUFFER_SIZE 1024
#define SL_BUFFER_SIZE_IN_SAMPLES (SL_BUFFER_SIZE / 4)

// Double buffering.
static uint32 buffer[2][SL_BUFFER_SIZE_IN_SAMPLES] = {0};
static int curBuffer = 0;

//основной звуковой буфер
static ring<uint32> samples_buffer(BLOCK_SIZE*BLOCK_COUNT);
static volatile bool pause_play_state=false;

static bool SwapChannels=false;
static float sndalVolume = 1.0;

void alt::audio::set_volume(float val)
{
    sndalVolume = val;
    if (bqPlayerVolume != NULL)
    {
        if (val == 0.0f) {
            (*bqPlayerVolume)->SetMute(bqPlayerVolume, SL_BOOLEAN_TRUE);
        } else {
            (*bqPlayerVolume)->SetMute(bqPlayerVolume, SL_BOOLEAN_FALSE);
            // Получите реальный диапазон громкости (опционально)
            SLmillibel_t minLevel = SL_MILLIBEL_MIN;
            SLmillibel_t maxLevel = 0;
            SLresult result = (*bqPlayerVolume)->GetVolumeLevelRange(bqPlayerVolume, &minLevel, &maxLevel);
            if (result == SL_RESULT_SUCCESS && minLevel < 0) {
                // Используйте реальный диапазон
                SLmillibel_t level = (SLmillibel_t)(val * (maxLevel - minLevel) + minLevel);
                (*bqPlayerVolume)->SetVolumeLevel(bqPlayerVolume, level);
            } else {
                // Резервный вариант
                SLmillibel_t level = (SLmillibel_t)(val * (-SL_MILLIBEL_MIN) + SL_MILLIBEL_MIN);
                (*bqPlayerVolume)->SetVolumeLevel(bqPlayerVolume, level);
            }
        }
    }
}

void alt::audio::swap_channels(bool val)
{
    SwapChannels=!val;
}

int alt::audio::utilization()
{
    return (100*BLOCK_SIZE*BLOCK_COUNT)/samples_buffer.Size();
}

static void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
    assert(bq == bqPlayerBufferQueue);
    assert(NULL == context);

    uint32 *nextBuffer = buffer[curBuffer];
    int nextSize = sizeof(buffer[0]);

    SLresult result = (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, nextBuffer, nextSize);

    assert(SL_RESULT_SUCCESS == result);

    if(pause_play_state)
    {
        curBuffer ^= 1; // Switch buffer

        // Render to the fresh buffer
        for(int i=0;i<SL_BUFFER_SIZE_IN_SAMPLES;i++)
            buffer[curBuffer][i]=0;

        samples_buffer.Free();
    }

    if(samples_buffer.Size()<SL_BUFFER_SIZE_IN_SAMPLES)return;
    curBuffer ^= 1; // Switch buffer

    // Render to the fresh buffer
    for(int i=0;i<SL_BUFFER_SIZE_IN_SAMPLES;i++)
    {
        uint32 val = samples_buffer.Get();
        if(SwapChannels)
            val = (val>>16)|(val<<16);
        buffer[curBuffer][i]=val;
    }
}

bool alt::audio::init(int _freq)
{
    pause_play_state=false;

    memset(buffer[0],0,sizeof(buffer[0]));
    memset(buffer[1],0,sizeof(buffer[1]));

    SLresult result;
    // create engine
    result = slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
    assert(SL_RESULT_SUCCESS == result);
    result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineEngine);
    assert(SL_RESULT_SUCCESS == result);
    result = (*engineEngine)->CreateOutputMix(engineEngine, &outputMixObject, 0, 0, 0);
    assert(SL_RESULT_SUCCESS == result);
    result = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);

    SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
    SLDataFormat_PCM format_pcm = {
            SL_DATAFORMAT_PCM,
            2,
            _freq*1000,
            SL_PCMSAMPLEFORMAT_FIXED_16,
            SL_PCMSAMPLEFORMAT_FIXED_16,
            SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
            SL_BYTEORDER_LITTLEENDIAN
    };

    SLDataSource audioSrc = {&loc_bufq, &format_pcm};

    // configure audio sink
    SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
    SLDataSink audioSnk = {&loc_outmix, NULL};

    // create audio player
    const SLInterfaceID ids[2] = {SL_IID_BUFFERQUEUE, SL_IID_VOLUME};
    const SLboolean req[2] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
    result = (*engineEngine)->CreateAudioPlayer(engineEngine, &bqPlayerObject, &audioSrc, &audioSnk, 2, ids, req);
    assert(SL_RESULT_SUCCESS == result);

    result = (*bqPlayerObject)->Realize(bqPlayerObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_PLAY, &bqPlayerPlay);
    assert(SL_RESULT_SUCCESS == result);
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_BUFFERQUEUE,
                                             &bqPlayerBufferQueue);
    assert(SL_RESULT_SUCCESS == result);
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_VOLUME, &bqPlayerVolume);
    if (result != SL_RESULT_SUCCESS) {
        bqPlayerVolume = NULL; // Отключите интерфейс, если не поддерживается
    }
    assert(SL_RESULT_SUCCESS == result);
    result = (*bqPlayerBufferQueue)->RegisterCallback(bqPlayerBufferQueue, bqPlayerCallback, NULL);
    assert(SL_RESULT_SUCCESS == result);
    result = (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_PLAYING);
    assert(SL_RESULT_SUCCESS == result);

    // Render and enqueue a first buffer.
    curBuffer ^= 1;

    result = (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, buffer[0], sizeof(buffer[0]));
    if (SL_RESULT_SUCCESS != result)
        return false;

    return true;
}

bool alt::audio::flush()
{
    return samples_buffer.Allow();
}

bool alt::audio::ready()
{
    return samples_buffer.Allow();
}

void alt::audio::pause(bool pause)
{
    if(bqPlayerPlay)
    {
        pause_play_state=pause;
    }
}

void alt::audio::destroy()
{
    if (bqPlayerObject != NULL)
    {
        (*bqPlayerObject)->Destroy(bqPlayerObject);
        bqPlayerObject = NULL;
        bqPlayerPlay = NULL;
        bqPlayerBufferQueue = NULL;
        bqPlayerMuteSolo = NULL;
    }

    if (outputMixObject != NULL)
    {
        (*outputMixObject)->Destroy(outputMixObject);
        outputMixObject = NULL;
    }

    if (engineObject != NULL)
    {
        (*engineObject)->Destroy(engineObject);
        engineObject = NULL;
        engineEngine = NULL;
    }
}

bool alt::audio::push(uint32 val)
{
    return samples_buffer.Push(val);
}

#elif defined(ASOUND_ENABLE_WINMM)

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

void alt::audio::pause(bool pause)
{
    return;
}

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

void alt::audio::pause(bool pause)
{
    return;
}

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
