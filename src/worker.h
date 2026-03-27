//
// Created by docb on 3/27/26.
//

#ifndef DBRACKMODULES_WORKER_H
#define DBRACKMODULES_WORKER_H
template<typename O,typename T>
struct OscWorker {
  constexpr static size_t NO_SIZES=6;
  constexpr static size_t MAX_CHUNK = 2048;
  size_t sizes[NO_SIZES]={32,64,128,256,512,1024};
  O osc;
  T voct=0.f;
  float freq=261.636f;
  float fmParam=0.f;
  bool run=false;
  bool linear=true;
  std::thread *thread=nullptr;
  dsp::RingBuffer<T,MAX_CHUNK> outBuffer;
  dsp::RingBuffer<T,MAX_CHUNK> inBuffer;
  std::atomic<bool> atomic_fm_connected{false};
  std::atomic<size_t> current_chunk_index{1};

  void setChunkSizeIndex(size_t idx) {
    if(idx<NO_SIZES)
      current_chunk_index.store(idx);
    else
      current_chunk_index.store(1);
    INFO("chunk size %ld idx: %ld",current_chunk_index.load(),idx);
  }
  size_t getChunkSizeIndex() {
    return current_chunk_index;
  }

  void processThread(float sampleTime) {
    const float pow2_30_inv = 1.0f / 1073741824.f;
    const float freq_c4_scaled = dsp::FREQ_C4 * pow2_30_inv;

    T fm_chunk[MAX_CHUNK];
    T out_chunk[MAX_CHUNK];
    int k=0;
    while(run) {
     size_t chunk_size=sizes[current_chunk_index];
      size_t size = outBuffer.size();

      if (size > chunk_size) {
        if(chunk_size>=512) {
          std::this_thread::sleep_for(std::chrono::duration<double>(sampleTime));
        } else {
          std::this_thread::yield();
        }
        continue;
      }

      bool has_fm = atomic_fm_connected.load(std::memory_order_relaxed);
      if (!has_fm) {
        T pitch = freq + voct;
        osc.baseFreq = freq_c4_scaled * dsp::approxExp2_taylor5(pitch + 30.f);
        for (size_t i = 0; i < chunk_size; ++i) {
          out_chunk[i] = osc.process(sampleTime) * 5.f;
          outBuffer.push(out_chunk[i]);
        }
      } else {
        size_t grabbed = inBuffer.size();
        if (grabbed >= chunk_size) {
          inBuffer.shiftBuffer(fm_chunk, chunk_size);
        } else {
          std::fill(fm_chunk, fm_chunk + chunk_size, 0.0f);
        }

        for (size_t i = 0; i < chunk_size; ++i) {
          T pitch = freq + voct;
          T fm_input = fm_chunk[i];

          if(linear) {
            osc.baseFreq = freq_c4_scaled * dsp::approxExp2_taylor5(pitch + 30.f);
            osc.baseFreq += dsp::FREQ_C4 * fm_input * fmParam;
          } else {
            pitch += fm_input * fmParam;
            osc.baseFreq = freq_c4_scaled * dsp::approxExp2_taylor5(pitch + 30.f);
          }
          out_chunk[i] = osc.process(sampleTime) * 5.f;
          outBuffer.push(out_chunk[i]);
        }
        //outBuffer.pushBuffer(out_chunk, chunk_size);
      }
      //outBuffer.pushBuffer(out_chunk, chunk_size);

    }
  }

  void stopThread() {
    run=false;
  }

  void checkThread(float sampleTime) {
    if(!run) {
      run=true;
      thread=new std::thread(&OscWorker::processThread,this,sampleTime);
#ifdef ARCH_LIN
      std::string threadName="OSC_Worker";
      pthread_setname_np(thread->native_handle(),threadName.c_str());
#endif
      thread->detach();
    }
  }

  T getOutput() {
    return outBuffer.empty()?0.f:outBuffer.shift();
  }

  void pushFM(T sample) {
    if(!inBuffer.full()) {
      inBuffer.push(sample);
    }
  }
};

#endif //DBRACKMODULES_WORKER_H
