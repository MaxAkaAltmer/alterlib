#ifndef APROCESS_H
#define APROCESS_H

#include "atypes.h"
#include "astring.h"

#include <atomic>
#include <thread>
#include <chrono>

namespace alt {

    /** @brief Возвращает идентификатор текущего процесса. */
    long long processId();

#if (defined(__cplusplus) && __cplusplus >= 202002L) || (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)

    class processMutex
    {
        std::atomic<uint32_t> state{0};

    public:
        void lock()
        {
            uint32_t expected = 0;
            while (!state.compare_exchange_weak(expected, 1, std::memory_order_acquire))
            {
                state.wait(1, std::memory_order_relaxed);
                expected = 0;
            }
        }

        bool try_lock()
        {
            uint32_t expected = 0;
            return state.compare_exchange_strong(expected, 1, std::memory_order_acquire);
        }

        void unlock()
        {
            state.store(0, std::memory_order_release);
            state.notify_one();
        }
    };


    class processCondition
    {
        std::atomic<uint32_t> generation{0};

    public:
        template <typename Lock>
        void wait(Lock& lock)
        {
            uint32_t current_gen = generation.load(std::memory_order_acquire);
            lock.unlock();

            generation.wait(current_gen, std::memory_order_acquire);

            lock.lock();
        }

        void notify_one()
        {
            generation.fetch_add(1, std::memory_order_release);
            generation.notify_one();
        }

        void notify_all()
        {
            generation.fetch_add(1, std::memory_order_release);
            generation.notify_all();
        }
    };

#else

    class processMutex
    {
        std::atomic_flag lock_flag = ATOMIC_FLAG_INIT;

    public:

        void lock()
        {
            while (lock_flag.test_and_set(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }
        }

        bool try_lock()
        {
            return !lock_flag.test_and_set(std::memory_order_acquire);
        }

        void unlock()
        {
            // Освобождаем флаг
            lock_flag.clear(std::memory_order_release);
        }
    };

    class processCondition
    {
        std::atomic<uint32_t> generation{0};

    public:

        template <typename Lock>
        void wait(Lock& lock)
        {
            uint32_t current_gen = generation.load(std::memory_order_acquire);
            lock.unlock();

            while (generation.load(std::memory_order_acquire) == current_gen)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            lock.lock();
        }

        void notify_one()
        {
            generation.fetch_add(1, std::memory_order_release);
        }

        void notify_all()
        {
            generation.fetch_add(1, std::memory_order_release);
        }
    };

#endif


    /**
     * @brief Класс для работы с разделяемой памятью между процессами.
     */
    class processSharedMemory
    {
    public:
        /**
         * @param name Имя разделяемой памяти.
         * @param size Размер в байтах.
         */
        processSharedMemory(const string& name, uintz size);
        processSharedMemory(const processSharedMemory&) = delete;
        ~processSharedMemory();
        processSharedMemory& operator = (const processSharedMemory&) = delete;

        /** @brief Доступ к байту по индексу. */
        uint8& operator[](uintz ind) { return buffer[ind]; }
        /** @brief Указатель на буфер. */
        uint8* operator()() { return buffer; }
        /** @brief Размер буфера. */
        uintz size() const { return buffer_size; }

        static void cleanup(const string& name);

    private:
        uint8* buffer = nullptr;      ///< Буфер разделяемой памяти.
        uintz buffer_size = 0;        ///< Размер буфера.
        void *internal = nullptr;     ///< Внутренняя реализация.
    };

}

#endif // APROCESS_H
