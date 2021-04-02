///
/// Copyright Y.Suzuki 2021
/// wave.suzuki.z@gmail.com
///
#pragma once

#include <Arduino.h>

namespace Worker
{
    using Func = void (*)(int);
    ///
    ///
    class Task
    {
        static constexpr const uint16_t stackSize = 4096;
        static constexpr TickType_t delayTime = 100 / portTICK_PERIOD_MS;
        static void job(void *arg)
        {
            Task *self = static_cast<Task *>(arg);
            self->update();
        }

        struct Event
        {
            Func func;
            int arg;
        };
        QueueHandle_t queue;

        void update()
        {
            while (true)
            {
                Event ev;
                auto stat = xQueueReceive(queue, &ev, portTICK_RATE_MS * 1000);
                if (stat == pdPASS)
                {
                    ev.func(ev.arg);
                }
            }
        }

    public:
        void start(int core = 1)
        {
            queue = xQueueCreate(4, sizeof(Event));
            xTaskCreatePinnedToCore(job, "Worker", stackSize, this, 1, nullptr, core);
        }
        bool signal(Func f, int a)
        {
            Event ev{f, a};
            auto stat = xQueueSend(queue, &ev, portTICK_RATE_MS * 1000);
            return stat == pdPASS;
        }
    };
}
