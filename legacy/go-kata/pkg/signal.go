package dsp

import "math"

// Signal a single time slice value
type Signal float64

func (s Signal) Value() float64 {
	return float64(s)
}

// Deviation returns the standard deviation
func (s Signal) Deviation() Signal {
	return Signal(math.Sqrt(float64(s)))
}

// AlmostEquals a float comparison by tolerance
func (s Signal) AlmostEquals(other Signal, tolerance float64) bool {
	diff := math.Abs(math.Round(float64(s-other) * tolerance))
	return diff < tolerance
}

// Equals a float comparison with 6 digit mantissa comparison
func (s Signal) Equals(other Signal) bool {
	return s.AlmostEquals(other, 0.000001)
}
