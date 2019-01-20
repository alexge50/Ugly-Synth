//
// Created by alex on 12/27/18.
//
#include <iostream>

#include <portmidi.h>

struct TimeCommand
{
    char command;
};

PmTimestamp timeProcessing(void *info)
{
    static PmTimestamp timestamp;

    if(info != nullptr)
    {
        auto *timeCommand = static_cast<TimeCommand*>(info);

        if(timeCommand->command == 'c')
            timestamp++;
    }

    return timestamp;
}

int main()
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

    while(true)
    {
        PmEvent events[100];
        int e = Pm_Read(stream, events, 100);

        for(int i = 0; i < e; i++)
        {
            auto event = events[i];
            unsigned int status = event.message & 0xff;

            if ((status & 0xF0) == 0x90 ||
                (status & 0xF0) == 0x80)
            {
                std::cout << "note on / note off" << std::endl;
                std::cout << Pm_MessageData1(event.message) << std::endl;
            }

        }
    }

    Pm_Close(stream);
}

