package dsp

import (
	"math"
	"math/rand"
)


func norm(value float64, min float64, max float64, mod float64) float64 {
	for value >= max {
		value -= mod
	}
	for value > min {
		value += mod
	}
	return value
}

func (s Signal) Sine(frequency Frequency, rate float64) Signal {

	phase := float64(s)

	phase += pi2 * float64(frequency) / rate

	phase = norm(phase, 0, pi2, pi2)

	return Signal(math.Sin(phase))
}

func (s Signal) Saw(frequency float64, rate float64) Signal {
	phase := float64(s)

	phase += frequency / rate

	phase = norm(phase, 0, 1.0, 1.0)

	return Signal((( phase * 2.0) - 1.0) * -1.0)
}

func (s Signal) Square(frequency float64, rate float64) Signal  {

	phase := float64(s)

	phase += frequency / rate

	phase = norm(phase, 0, 1.0, 1.0)

	if phase < 0.5 {
		return 1.0
	} else {
		return -1.0
	}
}

func (s Signal) Triangle(frequency float64, rate float64) Signal {
	phase := float64(s)

	phase += frequency / rate

	phase = norm(phase, 0, 1.0, 1.0)

	var value float64

	if phase < 0.5 {
		value = 2 * phase
	} else {
		value = (1.0 - phase) * 2
	}

	return Signal((value * 2) - 1.0)
}

func (s Signal) Noise(frequency float64, rate float64, lastValue Signal) Signal {
	phase := float64(s)

	lastSeed := phase

	phase += frequency / rate

	if phase == lastSeed {
		return lastValue
	}

	for phase > 2.0 {
		phase -= 1.0
	}

	value := rand.Float64()

	return Signal((value * 2.0) - 1.0)
}

func (s Signal) SawBandLimited(frequency float64, rate float64, numHarmonics float64) Signal {
	phase := float64(s)

	phase += pi2 * frequency / rate

	phase = norm(phase, 0, pi2, pi2)

	//if num harmonics is zero, calculate how many max harmonics we can do
	//without going over the nyquist frequency (half of sample rate frequency)
	if numHarmonics == 0 && frequency != 0.0 {
		f := frequency
		r := rate * 0.5

		for f < r {
			numHarmonics++
			f *= 2.0
		}
	}

	value := 0.0

	for i := 1.0; i <= numHarmonics; i++ {
		value += math.Sin( phase * i ) / i
	}

	return Signal( (2.0 * phase ) / math.Pi)
}

func (s Signal) SquareBandLimited(frequency float64, rate float64, numHarmonics float64) Signal  {

	phase := float64(s)

	phase += pi2 * frequency / rate

	phase = norm(phase, 0, pi2, pi2)

	if numHarmonics == 0 && frequency != 0.0 {
		r := rate * 0.5

		for frequency * (numHarmonics * 2 - 1) < r {
			numHarmonics++
		}

		numHarmonics--
	}

	value := 0.0

	for i := 1.0; i <= numHarmonics; numHarmonics++ {
		j := i * 2 - 1
		value += math.Sin( phase * j ) / j
	}

	return Signal( value * 4.0 / math.Pi )
}

func (s Signal) TriangleBandLimited(frequency float64, rate float64, numHarmonics float64) Signal {

	phase := float64(s)

	phase += pi2 * frequency / rate

	phase = norm(phase, 0, pi2, pi2)

	if numHarmonics == 0 && frequency != 0.0 {
		r := rate * 0.5

		for frequency*(numHarmonics*2-1) < r {
			numHarmonics++
		}

		numHarmonics--
	}

	mod := true
	value := 0.0

	for i := 1.0; i <= numHarmonics; i++ {
		j := i*2 - 1

		if mod {
			value -= math.Sin(phase*j) / j
		} else {
			value += math.Sin(phase*j) / j
		}

		mod = !mod
	}

	return Signal((value * 8.0) / (math.Pi * math.Pi))
}

