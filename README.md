# dbRackModules

Some VCVRack modules

New modules in 2.1.0: [PAD2](#pad2),[µPad2](#%C2%B5pad2),[PhO](#pho), [PhS](#phs), [PHSR](#phsr), [SPL](#spl), [YAC](#yac),[Faders](#faders),[PLC](#plc)


![](images/allmodules.png?raw=true)

<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->
**Table of Contents**

- [Modular Phase Distortion](#modular-phase-distortion)
  - [PhO](#pho)
  - [PhS](#phs)
  - [PHSR](#phsr)
  - [Other modules which can be phase driven](#other-modules-which-can-be-phase-driven)
- [Sound Generators](#sound-generators)
  - [SPL](#spl)
  - [Geneticterrain](#geneticterrain)
  - [Geneticsuperterrain](#geneticsuperterrain)
  - [SuperLFO](#superlfo)
  - [AddSynth](#addsynth)
  - [PAD](#pad)
  - [PAD2](#pad2)
  - [µPad2](#%C2%B5pad2)
  - [Gendy](#gendy)
- [Effects](#effects)
  - [DCBlock](#dcblock)
  - [PS](#ps)
  - [RSC](#rsc)
  - [MVerb](#mverb)
  - [YAC](#yac)
- [Random](#random)
  - [RndH,RndC](#rndhrndc)
  - [RndG](#rndg)
  - [RndHVS3](#rndhvs3)
  - [RTrig](#rtrig)
- [Sequencing](#sequencing)
  - [HexSeq](#hexseq)
  - [HexSeqP](#hexseqp)
  - [Frac and Hopa](#frac-and-hopa)
- [Others](#others)
  - [Faders](#faders)
  - [PLC](#plc)
  - [FLA and FFL](#fla-and-ffl)
  - [STrg](#strg)
  - [JTScaler](#jtscaler)
  - [JTKeys](#jtkeys)
  - [GenScale](#genscale)
  - [Interface](#interface)
  - [Plotter](#plotter)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->



## Modular Phase Distortion
The following modules are built for a modular phase distortion synthesis system which consists of:
- A start module which delivers a phase. This can be anything outputting an audio wave, but
  the most useful start phase is a clean saw wave e.g. the output of the VCV LFO Saw. Later also a sine wave or triangle wave can be considered.
- A chain of modules which modify the phase. This also can be anything which has an input and an output.
- A phase driven oscillator. This is the key point of the system. Such an oscillator is capable of taking an external phase
  to produce its output. 

Let's start with the last point:
### PhO
![](images/PhO.png?raw=true)

A 16 harmonics additive phase driven oscillator. The harmonics are passed using the polyphonic input "Pts".
When fed with a clean saw wave it outputs the additive wave.
With the dmp parameter the harmonics can be attenuated with an exponential falling curve towards the higher harmonics.

Now lets take a look what happens if the phase is modified somehow.
Here we take a wave shaper to modify the phase:


https://user-images.githubusercontent.com/1134412/170362396-4b37066c-5c43-41a2-8a09-3e62775e7e72.mp4


Other interesting possibilities are using all kind of filters (Fundamental VCF, VULT Tangents etc).

### PhS
![](images/PhS.png?raw=true)

This module provides the shaper extracted from Valley's Terrorform module for 
shaping a phase. There are a lot of different shapes available as shown. 
Each shape can be adjusted with the Amt parameter and input.

Here an example:



https://user-images.githubusercontent.com/1134412/170367214-95789be0-5650-4afd-9f26-70ebfc70dae6.mp4



### PHSR
![](images/PHSR.png?raw=true)

This module is a tiny Phasor module which can be used as start phase generator
It provides a polyphonic V/Oct input, a reset input and outputs for a clean saw wave, a sinus wave and a clean triangle wave.

### Other modules which can be phase driven
There are several modules which can be phase driven:
- SPL (new, see below): generates spline waveforms from up to 16 points
- GeneticTerrain (updated, now with phase input)
- Geneticsuperterrain (updated, now with phase input)
- SuperLFO (updated, if the frequency is set to the lowest value)
- Valley's Terrorform:
  connect the phase to the FM input, set the attenuator to 0.5 and set "zero mode" on.

## Sound Generators
![](images/soundgen.png?raw=true)

### SPL
![](images/spl.png?raw=true)

SPL takes up to 16 points through the polyphonic PTS input and generates a smooth spline wave.
Additionally, it outputs the line segments and step sequence of the points through the Line/Step outputs.

If SPL is fed with a phase i.e. the phase input is connected, then the V/Oct input is ignored.

Here an example. The spline wave is generated randomly and smoothly changed over time, and a bit strechted randomly over time with the PhS BEND shaper.



https://user-images.githubusercontent.com/1134412/170430990-c7cdd08d-1266-4b39-a2af-182c86453245.mp4




### Geneticterrain

Geneticterrain is a polyphonic oscillator using wave terrain synthesis.

The 27 provided base terrains can be combined 4 times so each terrain is a "genetic" sequence
with length 4 (sums up to 531441 possible terrains but don't worry many of them are similar).
There are 10 different curves available and each of it has a curve parameter. The curves can be moved on terrain
via dragging the blue circle or the X/Y Knobs. The view of the terrains can be moved by dragging not on the blue circle.

On the left side you can select the terrains, in v2 simple right-click on the knob and select.
The polyphonic GATE input is mainly for the display of the curve for each voice and has no
influence on the sound. If WYSIWYG make sure the GATE input reflects the GATE you are using
for e.g. the VCA or ADSR. 

On the right side there are the modulation parameters for the curve.
The modulation inputs are polyphonic, so each voice can be parametrized separately as shown 
in the image above.

NOTE: There is no band-limiting implemented for different reasons, so you have to handle possible
aliasing by yourself. In general: scale down the curves to cut down high frequencies.

NOTE: There are a lot of performance improvements implemented, but it costs still about 2% CPU per Quad Voice
so maximal about 8% when using 16 Voices also depending on how many and which Terrains are combined.

If the phs input is connected when driving it with an external phase the v/oct input is ignored.

### Geneticsuperterrain

Geneticsuperterrain is the same as Geneticterrain but with curves produced by the superformula.
(https://en.wikipedia.org/wiki/Superformula#Generalization)

Don't use it ;-)

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

### SuperLFO
The SuperLFO module outputs super formula curves (x and y).
It can be used as LFO and as normal polyphonic Oscillator by moving the freq button to maximum (261.63 HZ).

If the frequency is set to the lowest value it is treated as zero. Then it can act as phase driven oscillator
by using the phs input.

The current curve can be visualized by placing the plotter module on the right side of SuperLFO.
![](images/SuperLFO.png?raw=true)

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

New in 2.1.0: 
- The amp of the first partial can now be controlled with the amp knob - was 1.0 before.
- Presets will be applied directly without clicking on a knob.
- External parameter changes (e.g. from µMap) can be applied by sending a trigger signal to the new trig input (sse example below).  
- In the menu there can be set a fade time which causes that on every change the sound changes smoothly to the new.
- There is new method "static" which produces a descending partial sequence. If the seed is 0.5 all partials are part of the sequence (saw like wave).
  If the seed is zero only odd partials (pulse wave) are used and if the seed is one than only even partials are used. 
  Every seed in between will be interpolated.



https://user-images.githubusercontent.com/1134412/175531007-b2eea729-f7a9-4a7a-b85f-fd39fe1a0094.mp4




### PAD2
![](images/PAD2.png?raw=true)

This module makes pads using Paul Nascas PadSynth algorithm. It is maybe the first
module implementing the PadSynth algorithm  which is capable of smooth handling every parameter change. The wave is computed
completely in the background not affecting any CPU usage inside VCV Rack. But it may turn on the fan
of your computer if a lot of parameter changes are made in short distances e.g. connecting an LFO to the BW input.
The background processing may occupy another CPU with about 20-30%.

Changes to PAD:
- 48 partials can be edited
- There are polyphonic inputs for the controlling the partials.
- The random function updates the faders and the sound can be adjusted afterwards
- There are inputs for BW and SCL including CV attenuation.
- Advanced: The phases for the IFFT are random generated. With the Phs-Seed parameter
  the random seed can be changed, which does have a slight effect.
- Advanced: The fundamental frequency is set to a low 32.7 HZ by default. This can be changed
  with the Fund parameter. It matters if only high frequencies are played - then it may be useful to increase the fund parameter.
  Or when experimenting with vocal frequencies ....
  
Here an example:



https://user-images.githubusercontent.com/1134412/175530867-e04047da-146c-4667-b8dc-3d69b7ba6382.mp4



Thanks to Paul Piko and Dave Benham for testing and feature requests.

### µPad2

This is a small version of PAD2. It has to be fully controlled by the partial inputs and inputs for BW and Scl.

The [Faders](#Faders) module can be used for controlling  µPad2. The following example shows how to transfer
a sound from PAD2 to µPad2 using the Fader module. The Fader module has a special function for fetching the values of PAD2
by placing it on the right side of PAD2, placing the mouse cursor over the Faders module and pressing 'f'.



https://user-images.githubusercontent.com/1134412/175530919-0f10c530-6997-4db3-b431-832ff227e617.mp4



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

New in 2.1.0: The MVerb can now be run in a threaded mode (the default setting in the menu)
which puts  the heavy computing into a background thread. It should now run  below 1-2% CPU.


### YAC
Yet another chorus, this one can be modulated.

Long time I wondered why chorus, flangers almost only have an internal LFO with a sine wave. So I built this one.
If the ModL and ModR inputs are connected they were taken instead of the internal LFO. The rate parameter is ignored in this case
and the depth parameter is used as attenuator for the incoming modulation signal.

Here an example using Caudal from VULT as modulator. With intent a just chord is used to avoid
'built in' chorus effects which occur in 12edo (de-)tuning.



https://user-images.githubusercontent.com/1134412/171022165-66ec0706-2f26-4b07-ad21-0d18babe37e1.mp4



## Random
![](images/rnd.png?raw=true)

### RndH,RndC

*RndH* is a polyphonic, seedable random and hold with 3 Outputs serving different distributions:
- Min outputs the minimum of k uniform random values (k is adjusted with the Strength-Parameter). In bi-polar mode
  the output is randomly flipped at zero.
- WB outputs the weibull distribution with sigma = 1/k. In bi-polar mode
  the output is randomly flipped at zero.
- Tri outputs the triangular distribution i.e. outputs the average of k uniform random values. 
  With higher k it approximates the gaussian normal distribution.

If Strength = 1 all outputs deliver a uniform distribution. The image below shows the difference
between the distributions for higher strength values.

If the seed input is not connected or 0 the system time is used as seed on every RST trigger. 
Otherwise, the seed is built from the provided value 0<x<=10. In this case for each seed 
you will get exactly the same sequence on every RST Trigger. 

The number of output channels can be adjusted with the Chn Knob.

When no clock is connected RndH outputs the noise as audio stream.  

*RndC* is similar to RndH but outputs a (very) smooth polyphonic signal using cubic interpolation. 
Because of this RndC may produce lower/higher voltages as 
0V/10V in unipolar mode and -5V/5V in bipolar mode. I have foregone to scale it as it is done better
outside (and potentially different for each usecase). 
By scaling with 0.8 and adding 1V the normal range should be achieved. But this you may not want for
the min distribution or weibull.


![](images/rand2.png?raw=true)

This image shows from left to right: min, weibull, tri with strength 10 and uniform.


### RndG
The module does the same as Count Modulas Clocked Random Gates but is full polyphonic, seedable and a bit smaller sized.
The probabilities can be provided via the polyphonic prop input (-5V/5V, 0V = 50%).

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


## Sequencing
![](images/seq.png?raw=true)
### HexSeq
**HexSeq** is a **_12x64_** step trigger sequencer in a very compact format.
It accepts hex digits `0123456789ABCDEFabcdef*` an interprets one hex digit as 4 steps according
to the bit representation of the hex number. The original idea is from Steven Yi in his Csound live coding toolset.
The asterix (*) causes a hex char to be randomly choosen on every hit.

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

If the hex sequence is edited or changed it appers as red. You must press enter to activate it (makes it green).

As usual due to the one sample delay per cable, it is recommended always to feed the clock delayed by 2-10 samples
so that the rst is triggered for sure before the clock.

New features in 2.0.4:
- new key commands for active fields:
  - TAB: moves focus a field down without saving
  - UP: saves/activates the field and moves focus a field up
  - DOWN: saves/activates the field and moves focus a field down
  - p: randomizes the current active field, P: additionally activates/save
  - r: rotates the current active field one bit right, R: additionally activates/save
  - l: rotates the current active field one bit left,  L: additionally activates/save
- if the mouse is over the module the field 1-9 can be focused by pressing key 1-9

- new Expander **HexSeqExp**
  - The expander for HexSeq provides additional outputs:
    - Gte: outputs constant 10V if the last bit was set, 0V otherwise.
    - Clk: represents the incoming clock gate if bit is set
    - Inv: outputs the inverted trigger signal i.e. fires if the current bit is 0
    - NOTE: the expander can only be placed on the right side of HexSeq.
- Presets for learning purposes (Thanks to Andras Szabo)
- Menus for randomization settings - Bit density, Length Range
- Lights for the current Step - can be turned on/off in the menu

Thanks to Andras Szabo and Dave Benham for feature suggestions/requests.


### HexSeqP
**HexSeqP** is a **_16x16x64_** step trigger sequencer. Similar to HexSeq but with 16 selectable patterns.

The patterns can be selected via the pattern input in two modes (configurable in the menu):
- Poly Mode: The pattern is selected on a trigger on the respective polyphonic channel (left case in image below)
- Otherwise: The pattern is selected by the incoming voltage (0V-10V) (right case in image below)

![](images/seq2.png?raw=true)


For the description of the polyphonic outputs see HexSeq and HexSeqExp.

An internal incoming clock delay can be configured in the menu.


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
![](images/others.png?raw=true)

### Faders
![](images/Faders.png?raw=true)

This module is kind of similar to the unless towers module. It provides the following features:
- It has three fader blocks and three knobs
- There are 100 voltage addressable slots controlled by the Pat parameter and input
- There is a glide parameter which causes morphing from one slot to another
- On click on a fader the value according to the mouse position is immediately set.
- Adjustable voltage ranges {-10V,10V}, {-5V,5V}, {-3V,3V}, {-2V,2V}, {-1V,1V}, {0V,10V}, {0V,5V},
  {0V,3V}, {0V,2V},{0V,1V} in the menu
- Polyphonic channels configurable in the menu
- Snap function: "None","Semi","0.1","Pat 32","0.5","Pat 16","1","Pat 8". If another value than None is selected in the menu,
  the output value is quantized according to the given step value. 'Pat X' denotes 10V/X and is for CV addressing. 
- Each block can be randomized separately in the menu.

### PLC
This module does the same as PolyCon from Bogaudio but

- It is thinner
- The channels and ranges can be set in the menu.
- The values stay the same if the range is changed and the values are still in the range.

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

**DEPRECATED** , will be removed - use JTChords of dbRackSequencer instead.

Simulates a 31 Key Just Intonation keyboard.

The scales selectable via the scale knob are defined  in res/default_scales_31.json in the dbRackModules plugin directory.
You can copy this file to scales_31.json in the same directory and make your own version of it.
If it can be read without errors it will be taken instead otherwise it falls back to default_scales_31.json
(which should never be changed)

### GenScale
GenScale a polyphonic 16 note scale defined by the toggled buttons.

### Interface

Interface is a utility to define an input/output interface for a group of modules or saved selections.
It does not apply anything to the inputs.

### Plotter
Plots a polyphonic 2D Signal.
