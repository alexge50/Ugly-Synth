#include <iostream>
#include <string>
#include <atomic>
#include <cmath>

#include <portaudio.h>
#include <portmidi.h>


const int N_POLYPHONY_NOTES = 128;


struct VoiceData
{
    float frequency;
    std::atomic<PaTime> time;
    int note;

    VoiceData *next;
};


struct SynthData
{
    VoiceData voiceData[N_POLYPHONY_NOTES];
    std::atomic<VoiceData *> voicelist;
    int freestack[N_POLYPHONY_NOTES], stackTop;
    std::atomic<int> nPressedKeys;

    SynthData()
    {
        for(int i = 0; i < N_POLYPHONY_NOTES; i++)
            freestack[i] = N_POLYPHONY_NOTES - i - 1;
        stackTop = N_POLYPHONY_NOTES - 1;
        voicelist = nullptr;
    }
};


const int N_SAMPLES = 44100;
const float SAMPLE_FREQUENCY = 1.f / N_SAMPLES;

float waveGenerator(float frequency, PaTime time)
{
    auto a = static_cast<float>(floor(2 * time * frequency + 1.f / 2));
    float b = static_cast<int>(a + 0.001f) % 2 == 0 ? 1.f : -1.f;

    return static_cast<float>(4 * frequency * (time - 2 * (1. / frequency) * a) * b - 1.);
}

int synthCallback(
        [[maybe_unused]]const void *inputBuffer,
        void *outputBuffer,
        unsigned long framesPerBuffer,
        const PaStreamCallbackTimeInfo *timeInfo,
        PaStreamCallbackFlags statusFlags,
        void *userData
        )
{
    auto *out = static_cast<float*>(outputBuffer);

    auto *synthData = static_cast<SynthData*>(userData);

    for(int i = 0; i < framesPerBuffer; i++)
    {
        float a = 0;

        VoiceData *voice = synthData->voicelist;
        while(voice != nullptr)
        {
            a += (1.f / synthData->nPressedKeys) * waveGenerator(voice->frequency, voice->time);
            voice->time = voice->time + SAMPLE_FREQUENCY;
            voice = voice->next;
        }


        *out++ = a;
        *out++ = a;
    }

    return 0;
}

float getFrequencyFromKey(int key)
{
    static float middleOctave[12] = {261.6256, 277.1826, 293.6648, 311.1270, 329.6276, 349.2282, 369.9944, 391.9954, 415.3047, 440.0000, 466.1638, 493.8833};

    int octave = key / 12;
    int note = key % 12;
    float p = octave < 5 ? 1.f / (1 << (5 - octave)) : 1 << (octave - 5);

    return p * middleOctave[note];
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
int main()
{
    PaError error = Pa_Initialize();
    if(error != paNoError)
        printf("PortAudio error: %s\n", Pa_GetErrorText(error));

    PaStream *stream;
    SynthData synthData{};

    error = Pa_OpenDefaultStream(
            &stream,
            0,
            2,
            paFloat32,
            N_SAMPLES,
            256,
            synthCallback,
            &synthData);

    if(error != paNoError)
        std::cout << "PortAudio error: " << Pa_GetErrorText(error) << std::endl;

    error = Pa_StartStream(stream);
    if(error != paNoError)
        std::cout << "PortAudio error: " << Pa_GetErrorText(error) << std::endl;

    std::string input{};

    {

        int id = 0;
        const PmDeviceInfo *deviceInfo;
        while((deviceInfo = Pm_GetDeviceInfo(id)) != nullptr)
        {
            std::cout << "name: " << deviceInfo->name << "\n";
            std::cout << "interf: " << deviceInfo->interf << "\n";
            std::cout << "id:" << id << "\n";
            std::cout << "input: " << deviceInfo->input << "\n";
            std::cout << "output: " << deviceInfo->output << "\n";
            std::cout << "\n";

            id++;
        }

        PortMidiStream *stream;
        PmError error = Pm_OpenInput(
                &stream,
                3,
                NULL,
                100,
                NULL,
                NULL);

        Pm_SetFilter(stream, PM_FILT_ACTIVE | PM_FILT_CLOCK);
        Pm_SetChannelMask(stream, Pm_Channel(0));

        if(error != pmNoError)
            std::cout << "error: " << Pm_GetErrorText(error);

        int notes[128];
        while(true)
        {
            PmEvent events[100];
            int e = Pm_Read(stream, events, 100);

            for(int i = 0; i < e; i++)
            {
                auto event = events[i];
                int status = static_cast<unsigned int>(event.message & 0xff);
                int data1 = Pm_MessageData1(event.message);

                if ((status & 0xF0) == 0x90 ||
                    (status & 0xF0) == 0x80)
                {
                    std::cout << "note on / note off" << std::endl;
                    std::cout << data1 << std::endl;

                    if((status & 0xF0) == 0x90)
                    {
                        int p = synthData.freestack[synthData.stackTop];
                        synthData.stackTop --;

                        synthData.voiceData[p].time = 0;
                        synthData.voiceData[p].frequency = getFrequencyFromKey(data1);
                        synthData.voiceData[p].next = synthData.voicelist;
                        synthData.voiceData[p].note = data1;

                        synthData.nPressedKeys ++;
                        synthData.voicelist = &synthData.voiceData[p];

                        notes[data1] = p;
                    }
                    else
                    {
                        VoiceData *list = synthData.voicelist;
                        VoiceData **p = &list;
                        while(*p != nullptr && (*p)->note != data1)
                            p = &((*p)->next);
                        *p = (*p)->next;

                        synthData.nPressedKeys --;
                        synthData.voicelist = list;
                        synthData.freestack[++synthData.stackTop] = notes[data1];
                    }
                }

            }
        }
    }

    Pm_Close(stream);

    error = Pa_StopStream( stream );
    if(error != paNoError)
        std::cout << "PortAudio error: " << Pa_GetErrorText(error) << std::endl;

    error = Pa_Terminate();
    if(error != paNoError)
        std::cout << "PortAudio error: " << Pa_GetErrorText(error) << std::endl;

    return 0;
}
#pragma clang diagnostic pop