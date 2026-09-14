package dsp

type Sound struct {
	data Sample
	rate float64
	seconds int
	channels int
	phase float64
	frequency Frequency
}

func NewSound(rate int, seconds int, channels int, frequency Frequency) *Sound {
	numSamples := rate * fseconds * channels
	return &Sound {
		data: NewSample(numSamples),
		rate: float64(rate),
		seconds: seconds,
		channels: channels,
		phase: 0.0,
		frequency: frequency,
	}
}


func (s Sound) Saw() {
	for i := 0; i < len(s.data); i++ {
		s.data[i] = s.data[i].Saw(s.frequency.Value(), s.rate)
	}
}

