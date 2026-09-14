package dsp

import "math"

type Frequency float64

func StandardC() Frequency {
	return NewFrequency(3, 3)
}

func NewFrequency(octave float64, note float64) Frequency {
	/*
	Calculate the frequency of any note!
	frequency = 440�(2^(n/12))

	N=0 is A4
	N=1 is A#4
	etc...

	notes go like so...
	0  = A
	1  = A#
	2  = B
	3  = C
	4  = C#
	5  = D
	6  = D#
	7  = E
	8  = F
	9  = F#
	10 = G
	11 = G#
*/
	return Frequency(440*math.Pow(2.0,((octave-4)*12+note)/12.0))
}

func (f Frequency) Value() float64  {
	return float64(f)
}