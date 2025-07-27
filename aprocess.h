#ifndef APROCESS_H
#define APROCESS_H

#include "atypes.h"
#include "astring.h"

namespace alt {

    /** @brief Возвращает идентификатор текущего процесса. */
    long long processId();

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

    private:
        uint8* buffer = nullptr;      ///< Буфер разделяемой памяти.
        uintz buffer_size = 0;        ///< Размер буфера.
        void *internal = nullptr;     ///< Внутренняя реализация.
    };

}

#endif // APROCESS_H
