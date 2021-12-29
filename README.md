# dbRackModules

Some VCVRack modules
## Sound Generators
![](images/soundgen.png?raw=true)

### Geneticterrain

Geneticterrain is a polyphonic oscillator using wave terrain synthesis.
It is basically a port of my csound web instrument https://docb.io/geneticterrain to VCV-Rack v2.

The 25 provided base terrains can be combined 4 times so each terrain is a "genetic" sequence
with length 4 (sums up to 390625 possible terrains but don't worry many of them are similar).
There are 10 different curves available and each of it has a curve parameter.

On the left side you can select the terrains, in v2 simple right-click on the knob and select.
The polyphonic GATE input is mainly for the display of the curve for each voice and has no
influence on the sound. If WYSIWYG make sure the GATE input reflects the GATE you are using
for e.g. the VCA or ADSR. 

On the right side there are the modulation parameters for the curve.
The modulation inputs are polyphonic, so each voice can be parametrized separately as shown 
in the image above.

NOTE: There is no band-limiting implemented for different reasons, so you have to handle possible
aliasing by yourself. In general: scale down the curves to cut down high frequencies.

NOTE: There are a lot of performance improvements implemented, but it costs still about 4% CPU per Quad Voice
so maximal about 16% when using 16 Voices also depending on how many and which Terrains are combined.

### Geneticsuperterrain

Geneticsuperterrain is the same as Geneticterrain but with curves produced by the superformula.
(https://en.wikipedia.org/wiki/Superformula#Generalization)

Don't use it. 

And if you use it beware of the following issues:

1. The curves are not on all combinations of M1,M2 (aka Y,Z) closed 
and have poles (closed in infinity).
For example if N1>0 and there exists an n,m in Z with 2*M1/M2 = 2m+1/n 
i.e curves with (3,2) (5,2) (7,2) etc and (5,4) (6,4) (7,4) (9,4) etc. 
have a pole which is noticeable when listening. 
If N1 < 0 then the curve is reversed and the poles go 
towards zero in this case 
(see https://web.archive.org/web/20171208231427/http://ftp.lyx.de/Lectures/SuperformulaU.pdf)

2. The speed of the point on some curves can vary dramatically which
results in high frequencies (but also very interesting sounds) and may upset connected VCFs.

If you still want to use it,
I'd recommend to start with simple curves e.g. M1=M2  and avoid sharp,
pointy curves. (use rather the red curve in the image above and not the yellow one).

### AddSynth

AddSynth is a monophonic two-dimensional additive synthesizer.
The first dimension is defined by the ratios of the partials and the corresponding amps which are modulated by the polyphonic amp inputs.
The second dimension is defined by the wave which is applied for each ratio.

The incoming amps can be attenuated by an exponential falling curve towards the higher frequencies.
A dmp value of 0 causes the strongest attenuation.

The ratios and waves are defined  in res/addsynth_default.json in the dbRackModules plugin directory.
You can copy this file to addsynth.json in the same directory and make your own version of it.
If it can be read without errors it will be taken instead otherwise it falls back to addsynth_default.json
(which should never be changed)

### PAD
This module makes pads using Paul Nascas PadSynth algorithm. Simply checkout these beautiful sounds by playing around
with the for parameters after connecting a polyphonic v/oct source. Hint: the partials amps used for the IFFT are
randomly generated (parameter seed) using different methods (parameter Mth).

### Gendy
This is a port of Supercolliders gendy which is based on the work of Peter Hoffmann who has rebuild GENDYN
from Iannis Xenakis.

## Effects
![](images/effects.png?raw=true)

### DCBlock

This is a stereo polyphonic dc blocker. 

Use the default order to dc block and the lower orders for stronger highpass filtering.
This is a reimplementation of the Csound dcblock2 algorithm for Rack.

### PS
An fft-based pitch shifter from the gamma library.

### RSC
Port from Csounds reverbsc opcode. This is an 8 delay line reverb.

### MVerb
Port of a 5x5 waveguide mesh reverb from Jon Christopher Nelson and Michael Gogins
(https://github.com/gogins/csound-extended/tree/master/Opcodes/MVerb)
with the following differences:
There is no equalizer on each delay line (would cause 1000 biquad filter operations). 
The randomization of the mesh frequencies can be done by the frequency modulation inputs. 

The standard presets are provided in the factory presets of the module. 

## Random
![](images/rnd.png?raw=true)

### RndH,RndC

*RndH* is a polyphonic, seedable random and hold with 3 Outputs serving different distributions:
- Min outputs the minimum of k uniform random values (k is adjusted with the Strength-Parameter). In bi-polar mode
  the output is randomly flipped at zero.
- WB ouputs the weibull distribution with sigma = 1/k. In bi-polar mode
  the output is randomly flipped at zero.
- Tri putputs the triangular distribution i.e outputs the average of k uniform random values. 
  With higher k it approximates the gaussian normal distribution.

If Strength = 1 all outputs deliver a uniform distribution. The image below shows the difference
between the distributions for higher strength values.

If the seed input is not connected or 0 the system time is used as seed on every RST trigger. 
Otherwise, the seed is built from the provided value 0<x<=10. In this case for each seed 
you will get exactly the same sequence on every RST Trigger. 

The samplesPerFrame of output channels can be adjusted with the Chn Knob.

When no clock is connected RndH outputs the noise as audio stream.  

*RndC* is similar to RndH but outputs a (very) smooth signal using cubic interpolation. 
Because of this RndC may produce lower/higher voltages as 
0V/10V in unipolar mode and -5V/5V in bipolar mode. I have foregone to scale it as it is done better
outside (and potentially different for each usecase). 
By scaling with 0.8 and adding 1V the normal range should be achieved. But this you may not want for
the min distribution or weibull.


![](images/rand2.png?raw=true)
This image shows from left to right: weibull, min, tri with strength 15 and uniform.


### RndG
The module does the same as Count Modulas Clocked Random Gates but is full polyphonic, seedable and a bit smaller sized.
The probabilities can be provided via the polyphonic prop input (0-10V).

### RndHVS3

RndHvs3 uses a 3d HVS Cube (similar to Csounds hvs3 opcode) filled with seedable random values of different distributions.
The cube values are defined by a 3d lattice of size 7x7x7 where on each node in the lattice
there are put 32 random values.

The values in between the nodes and on the vertices are linearly interpolated.

The x,y,z inputs (0-10V) define the (continuous) point in the cube for which the 32 values are outputted through the two
polyphonic output ports. It will output a smooth signal on every output channel if the input is a smooth signal.

### RTrig
A polyphonic gaussian random trigger around a given frequency and deviation.
The used random values can also be plugged in via the src input.
If Chn > 1 then on each internal trigger the output channel is randomly chosen.

### SuperLFO
The SuperLFO module outputs super formula curves (x and y). 
It can also used as normal polyphonic Oscillator by moving the freq button to maximum (261.63 HZ).

## Sequencing
![](images/seq.png?raw=true)
## HexSeq
**HexSeq** is a **_12x64_** step trigger sequencer in a very compact format.
It accepts hex digits `0123456789ABCDEF` an interprets one hex digit as 4 steps according
to the bit representation of the hex number. The original idea is from Steven Yi in his Csound live coding toolset.

The only thing you must master to remember are the bits of the 16 digits:
```
0     0000
1     0001
2     0010
3     0011
4     0100
5     0101
6     0110
7     0111
8     1000
9     1001
A     1010
B     1011
C     1100
D     1101
E     1110
F     1111
```
The sequence is always looped according to length*4 of the input.
So the input '8' makes a trigger per beat if clocked with 1/16,
'2' makes the typical hihat, '08' makes the typical snare.

As usual due to the one sample delay per cable, it is recommended always to feed the clock delayed by 2-10 samples
so that the rst is triggered for sure before the clock.

### HexSeqP (alpha)
**HexSeqP** is a **_16x16x64_** step trigger sequencer. Similar to as HexSeq but with 16 selectable patterns.



### Frac and Hopa

**Frac** is a monophonic function and hold module with a simple function. 
It devides two integer numbers N and P
and outputs step by step their expansion for the given base (ranging from 2 to 16)
scaled by the Scl Parameter and added to base volt value.
The offset parameter causes the sequence to be started on an RST trigger from the given value.


**Hopa** is a monophonic function and hold module which outputs the sequence of the hopalong attractor for
the given start values x0,y0 and the parameters a,b,c.
```
x=x0, y=y0
xnew=y-sqrt(abs(b*x-c))*sign(x)
ynew=a-x
```

If the CLK input is not connected it outputs the hopalong attractor as audio. In order to hear this
interesting sound it is best to filter out the 10K band a much as possible and push the lower bands.


## Others
![](images/other.png?raw=true)
### FLA and FFL
FLA applies integer arithmetic to a CV signal.
FLL applies integer bit operations to a CV signal.

The CV input is converted to a 24 bit integer and converted back after the operation.
The purpose of these modules was originally to have a method for arbitrary formulas used in the classic
"Experimental music from very short C programs (Viznut)".
However it turns out that also wired sounds can be made with this method.

### STrg
A polyphonic Schmitt Trigger module

### JTScaler
JTScaler tunes a 12edo V/Oct standard input to a just intonation scale.

The scales selectable via the scale knob are defined  in res/default_scales.json in the dbRackModules plugin directory.
You can copy this file to scales.json in the same directory and make your own version of it.
If it can be read without errors it will be taken instead otherwise it falls back to default_scales.json
(which should never be changed)

### JTKeys
Simulates a 31 Key Just Intonation keyboard.

The scales selectable via the scale knob are defined  in res/default_scales_31.json in the dbRackModules plugin directory.
You can copy this file to scales_31.json in the same directory and make your own version of it.
If it can be read without errors it will be taken instead otherwise it falls back to default_scales_31.json
(which should never be changed)

### GenScale
GenScale a polyphonic 16 note scale defined by the toggled buttons.

### Interface

Interface is an utility to define an input/output interface for a group of modules or saved selections.
It does not apply anything to the inputs.

### Plotter
Plots a polyphonic 2D Signal.