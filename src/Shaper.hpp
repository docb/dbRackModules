
#ifndef DBRACKMODULES_SHAPER_HPP
#define DBRACKMODULES_SHAPER_HPP
#include "SIMDUtilities.hpp"
/*
ValleyRackFree Copyright (C) 2020, Valley Audio Soft, Dale Johnson
Licenced under the terms of the GNU General Public License v3 or later

Modifications: moved out WARPLE shaper
 */
class Shaper {
public:
  enum Modes {
    BEND_MODE,
    TILT_MODE,
    LEAN_MODE,
    TWIST_MODE,
    WRAP_MODE,
    MIRROR_MODE,
    REFLECT_MODE,
    PULSE_MODE,
    STEP4_MODE,
    STEP8_MODE,
    STEP16_MODE,
    VARSTEP_MODE,
    SINEWRAP_MODE,
    HARMONICS_MODE,
    BUZZ_X2_MODE,
    BUZZ_X4_MODE,
    BUZZ_X8_MODE,
    WRINKLE_X2_MODE,
    WRINKLE_X4_MODE,
    WRINKLE_X8_MODE,
    SINE_DOWN_X2_MODE,
    SINE_DOWN_X4_MODE,
    SINE_DOWN_X8_MODE,
    SINE_UP_X2_MODE,
    SINE_UP_X4_MODE,
    SINE_UP_X8_MODE,
    NUM_MODES
  };

  Shaper();
  ~Shaper();
  __m128 process(const __m128& a, const __m128& f);
  void setShapeMode(int mode);
private:
  int _shapeMode;

  // Common vars
  __m128 __aScale, __x, __y, __z, __f, __xx, __ff, __k, __mask, __midMask, __highMask;
  __m128 __m, __b, __c, __denom;
  __m128 __output;
  __m128i __aInt, __xInt, __yInt;
  __m128 __aIntF, __xIntF, __yIntF;

  // Numbers
  __m128 __third, __twoThird, __half, __minusHalf, __fourth, __eighth, __sixteenth, __hundredth;
  __m128 __minus, __zeros, __ones, __twos, __threes, __fours, __eights, __nines, __sixteens;
  uint32_t _z[4];
  uint32_t _w[4];
  float* random;
  __m128 __noise;

  // Shaping functions
  void bend(const __m128& a, const __m128& f);
  void tilt(const __m128& a, const __m128& f);
  void lean(const __m128& a, const __m128& f);
  void lean2(const __m128& a, const __m128& f);
  void twist(const __m128& a, const __m128& f);
  void wrap(const __m128& a, const __m128& f);
  void mirror(const __m128& a, const __m128& f);
  void reflect(const __m128& a, const __m128& f);
  void pulse(const __m128& a, const __m128& f);
  void step4(const __m128& a, const __m128& f);
  void step8(const __m128& a, const __m128& f);
  void step16(const __m128& a, const __m128& f);
  void varStep(const __m128& a, const __m128& f);
  void sineWrap(const __m128& a, const __m128& f);
  void harmonics(const __m128& a, const __m128& f);
  void buzzX2(const __m128& a, const __m128& f);
  void buzzX4(const __m128& a, const __m128& f);
  void buzzX8(const __m128& a, const __m128& f);
  void wrinkleX2(const __m128& a, const __m128& f);
  void wrinkleX4(const __m128& a, const __m128& f);
  void wrinkleX8(const __m128& a, const __m128& f);
  void sineDownX2(const __m128& a, const __m128& f);
  void sineDownX4(const __m128& a, const __m128& f);
  void sineDownX8(const __m128& a, const __m128& f);
  void sineUpX2(const __m128& a, const __m128& f);
  void sineUpX4(const __m128& a, const __m128& f);
  void sineUpX8(const __m128& a, const __m128& f);
};


#endif //DBCAEMODULES_SHAPER_HPP
