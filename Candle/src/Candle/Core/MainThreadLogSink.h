#pragma once

#include "Candle/Core/LogSink.h"
#include "Candle/Core/Memory/Collections/MPSCRingBuffer.h"

#include <functional>
#include <vector>
#include <span>

namespace Candle {

    using LogListener = std::function<void(std::span<const LogRecord>)>;
    using LogListenerHandle = uint32_t;

    // Threading: Write runs on the drain thread; everything else is main thread only.
    class MainThreadLogSink : public LogSink
    {
    public:
        explicit MainThreadLogSink(size_t capacity);

        void Write(const LogRecord* records, size_t count) override;    // TryPushN into m_Pending
        void Flush() override {}

        void Dispatch();        // Once per frame: PopBatch until empty, hand each batch to every listener

        LogListenerHandle RegisterListener(LogListener listener);
        void UnregisterListener(LogListenerHandle handle);

    private:
        MPSCRingBuffer<LogRecord> m_Pending;
        std::vector<std::pair<LogListenerHandle, LogListener>> m_Listeners;
        LogListenerHandle m_NextHandle = 1;
    };

}