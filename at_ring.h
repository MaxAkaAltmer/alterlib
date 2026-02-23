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

#ifndef AT_RING_H
#define AT_RING_H

#include <atomic>
#include <cstring> //todo: memcopy performance test
#include <type_traits> 

//Памятка:

//Release — запрещает всплытие операций (снизу вверх) за барьер.
//          Это Store-Store барьер (на x86 вообще ничего не стоит, на ARM дешево). 

//Acquire — запрещает проваливание операций (сверху вниз) за барьер.
//          Это Load-Load барьер (на x86 тоже ничего не стоит, на ARM дешево).

namespace alt {
	
#ifdef __cpp_lib_hardware_interference_size
    using std::hardware_destructive_interference_size;
#else
    constexpr std::size_t hardware_destructive_interference_size = 64;
#endif

	template <class T>
	class snapshotBuffer
	{		
		static_assert(std::is_trivially_copyable<T>::value, "T must be trivially copyable for memcpy snapshot");
		
		struct State {
			alignas(hardware_destructive_interference_size) volatile unsigned int counter = 0;			
			T buffers[2];
		};

		alignas(hardware_destructive_interference_size) State shared_state;

	public:
		
		void write(const T& val)
		{
			unsigned int cnt = shared_state.counter;			
			int idx = cnt & 1;

			shared_state.buffers[idx] = val;
			std::atomic_thread_fence(std::memory_order_release);
			shared_state.counter = cnt + 1;
		}

		bool read(T& outVal, unsigned int &catch_up_counter)
		{
			unsigned int current_cnt = shared_state.counter;
			std::atomic_thread_fence(std::memory_order_acquire);

			State snapshot;
			snapshot.counter = current_cnt;
			std::memcpy(snapshot.buffers, shared_state.buffers, sizeof(T)*2);	
			std::atomic_thread_fence(std::memory_order_acquire);
			
			unsigned int seq_end = shared_state.counter;

			if (seq_end - snapshot.counter > 1 || snapshot.counter == catch_up_counter) {
				return false;
			}

			int safe_idx = (snapshot.counter & 1) ^ 1;

			outVal = snapshot.buffers[safe_idx];
			catch_up_counter = snapshot.counter;
			return true;
		}
	};
	
	template <class T>
	class dualBuffer
	{
	public:

		dualBuffer() = default;
		
		void writeDone()
		{
			std::atomic_thread_fence(std::memory_order_release);
			if (wr_index == rd_index)
			{
				wr_index = wr_index ^ 1;
			}
		}
		bool canRead() const
		{
			return rd_index != wr_index;
		}
		void readDone()
		{
			std::atomic_thread_fence(std::memory_order_release);
			rd_index = wr_index;
		}

		T& readBuffer()
		{
			std::atomic_thread_fence(std::memory_order_acquire);
			return buffers[rd_index];
		}
		T& writeBuffer() { return buffers[wr_index]; }

	private:

		alignas(hardware_destructive_interference_size) volatile int wr_index = 0;
		alignas(hardware_destructive_interference_size) volatile int rd_index = 0;
		T buffers[2];
	};

	//todo: full refactoring
    template <class T>
    class ring
    {
     private:
            T *buff;
            volatile int len;
			alignas(hardware_destructive_interference_size) volatile int up;
			alignas(hardware_destructive_interference_size) volatile int down;
     public:
            ring(){buff=0;len=1;up=0;down=0;}
            ring(int size){buff=new T[size+1];len=size+1;up=0;down=0;}
            ~ring(){if(buff)delete []buff;}

            void Resize(int size)
            {
                if(buff)delete []buff;
                buff=new T[size+1];len=size+1;up=0;down=0;
            }

            int Read(T *data, int size)
            {
                int cnt=Size();
                if(cnt>size)cnt=size;
                for(int i=0;i<cnt;i++)
                {
                    data[i]=Get();
                }
                return cnt;
            }

            bool WriteBlock(const T *block, int size)
            {
                if(Allow()<size)return false;
                for(int i=0;i<size;i++)
                {
                    buff[up]=block[i];
                    std::atomic_thread_fence(std::memory_order_release);
                    up=(up+1)%len;
                }
                return true;
            }

            int Limit(){return len-1;}

            void forcedPush_Unsafe(const T &val) //не для многопоточного применения!
            {
                if(!Allow())Get();
                buff[up]=val;
                up=(up+1)%len;
            }

            bool Push(const T &val)
            {
                if(!Allow())return false;
                buff[up]=val;

                std::atomic_thread_fence(std::memory_order_release);
                up=(up+1)%len;
                return true;
            }

            int Size() const
            {
                volatile int tup=up,tdown=down;
                if(tup>=tdown)return tup-tdown;
                return len-tdown+tup;
            }

            //размер линейного блока для вычитывания
            int blockSizeToRead()
            {
                volatile int tup=up,tdown=down;
                if(tup>=tdown)return tup-tdown;
                return len-tdown;
            }

            //размер линейного блока для записи
            int blockSizeToWrite()
            {
                volatile int tup=up,tdown=down;
                if(tup>=tdown)return len-tup-(tdown?0:1);
                return tdown-tup-1;
            }

            //указатель на начало данных
            T* startPoint()
            {
                std::atomic_thread_fence(std::memory_order_acquire);
                return &buff[down];
            }

            //указатель на место записи
            T* afterPoint()
            {
                std::atomic_thread_fence(std::memory_order_acquire);
                return &buff[up];
            }

            T Get()
            {
             T rv;
                if(!Size())return rv;

                std::atomic_thread_fence(std::memory_order_acquire);
                rv=buff[down];

                std::atomic_thread_fence(std::memory_order_release);
                down=(down+1)%len;
                return rv;
            }

            int Allow()
            {
                if(!buff)return 0;
                volatile int tup=up,tdown=down;
                if(tup<tdown)return tdown-tup-1;
                return len-tup+tdown-1;
            }
            T& operator[](int ind)
            {
                if(ind<0)
                {
                    ind = Size() + ind;
                }
                std::atomic_thread_fence(std::memory_order_acquire);
                return buff[(down+ind)%len];
            }
            void Free(int size=-1)
            {
                std::atomic_thread_fence(std::memory_order_release);
                if(size<0)down=up;
                else down=(down+size)%len;
            }
            void Added(int size)
            {
                std::atomic_thread_fence(std::memory_order_release);
                up=(up+size)%len;
            }
    };

} // namespace alt

#endif // AT_RING_H
