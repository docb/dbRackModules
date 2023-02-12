
// MULTIPLE STATIC WAVESHAPING CLASSES
// THESE CLASSES INCORPORATE THE ANTIDERIVATE ANTIALIASING METHOD DESCRIBED IN:
// "ANTIDERIVATIVE ANTIALIASING FOR MEMORYLESS NONLINEARITIES" BY S. BILBAO ET AL. IEEE SPL, 2017
//
// THIS CODE IS PROVIDED "AS-IS", WITH NO GUARANTEE OF ANY KIND.
//
// CODED BY F. ESQUEDA - DECEMBER 2017
//
// ADAPTED FOR VCV RACK JANUARY 2018
//
// TODO:
// 		ADD MORE STANDARD FILTERS
// docB: Adapted to simd for implementing polyphony

#ifndef WAVESHAPING_H
#define WAVESHAPING_H
template<typename T>
class HardClipper {

// THIS CLASS IMPLEMENTS AN ANTIALIASED HARD CLIPPING FUNCTION.
// THIS CLASS USES THE FIRST-ORDER ANTIDERIVATIVE METHOD.

private:
	T xn1 = 0.0;
	T Fn = 0.0;
	T Fn1 = 0.0;

	const float thresh = 10.0e-2;
	const float oneTwelfth = 1.0/12.0;

public:

	HardClipper() {}
	~HardClipper() {}

	T process(T input) {
		return antialiasedHardClipN1(input);
	}

	T hardClipN0(T x) {
		// Hard clipping function
		return 0.5f*(simd::sgn(x+1.0f)*(x+1.0f) - simd::sgn(x-1.0f)*(x-1.0f));
	}

  T hardClipN1(T x) {
		// First antiderivative of hardClipN0
		return 0.25f*(simd::sgn(x+1.0f)*(x+1.0f)*(x+1.0f) - simd::sgn(x-1.0f)*(x-1.0f)*(x-1.0f) - 2.0f);
	}

	T hardClipN2(T x) {
		// second antiderivative of hardClipN0
		return oneTwelfth*(simd::sgn(x+1.0f)*(x+1.0f)*(x+1.0f)*(x+1.0f) - simd::sgn(x-1.0f)*(x-1.0f)*(x-1.0f)*(x-1.0f) - 6.0f*x);
	}

	T antialiasedHardClipN1(T x) {
		// Hard clipping with 1st-order antialiasing
		Fn = hardClipN1(x);
		T tmp=simd::ifelse(simd::fabs(x - xn1) < thresh,hardClipN0(0.5f * (x + xn1)),(Fn - Fn1)/(x - xn1));
		xn1 = x;
		Fn1 = Fn;
		return tmp;
	}

};

template<typename T>
class Wavefolder {

// THIS CLASS IMPLEMENTS A FOLDING FUNCTION, SOMETIMES KNOWN AS A MATHEMATICAL FOLDER DUE TO ITS
// SHARP EDGES. THE SOUND PRODUCED IS SIMILAR TO THAT OF THE BUCHLA 259'S TIMBRE SECTION.

private:
	// Antialiasing state variables
	T xn1 = 0.0;
	T xn2 = 0.0;
	T Fn = 0.0;
	T Fn1 = 0.0;
	T Gn = 0.0;
	T Gn1 = 0.0;

	// Ill-conditioning threshold
	const float thresh = 10.0e-2;

	const float oneSixth = 1.0/6.0;

	HardClipper<T> hardClipper;

public:

	Wavefolder() {}
	~Wavefolder() {}

	T process(T input) {
		return antialiasedFoldN2(input);
	}

	T foldFunctionN0(T x) {
		// Folding function
		return (2.0f*hardClipper.hardClipN0(x) - x);
	}

	T foldFunctionN1(T x) {
		// First antiderivative of the folding function
		return (2.0f*hardClipper.hardClipN1(x) - 0.5f*x*x);
	}

	T foldFunctionN2(T x) {
		// Second antiderivative of the folding function
		return (2.0f*hardClipper.hardClipN2(x) - oneSixth*(x*x*x));
	}

	T antialiasedFoldN1(T x) {
		// Folding with 1st-order antialiasing (not recommended)
		Fn = foldFunctionN1(x);
		T tmp = simd::ifelse(simd::fabs(x - xn1) < thresh,foldFunctionN0(0.5f * (x + xn1)),(Fn - Fn1)/(x - xn1));
		xn1 = x;
		Fn1 = Fn;
		return tmp;
	}

	T antialiasedFoldN2(T x) {

		// Folding with 2nd-order antialiasing
		Fn = foldFunctionN2(x);
		Gn = simd::ifelse(simd::fabs(x - xn1) < thresh,foldFunctionN1(0.5f * (x + xn1)),(Fn - Fn1) / (x - xn1));
    T delta = 0.5f * (x - 2.0f*xn1 + xn2);
    T tmp5=foldFunctionN2(0.5f * (x + xn2));
    T tmp4=foldFunctionN1(0.5f * (x + xn2));
    T tmp3=(2.0f/delta)*(tmp4 + (Fn1 - tmp5)/delta);
    T tmp2=foldFunctionN0(0.25f * (x + 2.0f*xn1 + xn2));
    T tmp1=simd::ifelse(simd::fabs(delta) < thresh,tmp2,tmp3);
		T tmp =simd::ifelse(simd::fabs(x - xn2) < thresh,tmp1,2.0f * (Gn - Gn1)/(x - xn2));

		 // Update state variables
		Fn1 = Fn;
		Gn1 = Gn;
		xn2 = xn1;
		xn1 = x;

		return tmp;
	}

};

template<typename T>
class SoftClipper {

// THIS CLASS IMPLEMENTS A PIECEWISE SOFT SATURATOR WITH
// FIRST-ORDER ANTIDERIVATIVE ANTIALIASING.

private:

	// Antialiasing variables
	T xn1 = 0.0;
	T Fn = 0.0;
	T Fn1 = 0.0;
	const float thresh = 10.0e-2;

public:
	SoftClipper() {}
	~SoftClipper() {}

	T process(T input) {
		return antialiasedSoftClipN1(input);
	}

	T softClipN0(T x) {
		return simd::ifelse(simd::fabs(x)<1,simd::sin(0.5f*M_PI*x),simd::sgn(x));
	}

	T softClipN1(T x) {
		return simd::ifelse(simd::fabs(x)<1,1.0f - (2.0f/M_PI)*simd::cos(x*0.5f*M_PI),simd::sgn(x)*x);
	}

	T antialiasedSoftClipN1(T x) {
		Fn = softClipN1(x);
		T tmp = simd::ifelse(simd::fabs(x - xn1) < thresh,softClipN0(0.5f * (x + xn1)),(Fn - Fn1)/(x - xn1));
		// Update states
		xn1 = x;
		Fn1 = Fn;
		return tmp;
	}
};

#endif

// EOF