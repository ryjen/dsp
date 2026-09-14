package dsp

import (
	"bufio"
	"math"
	"os"
	"strconv"
)

// Sample a time sliced array of signals
type Sample []Signal

func NewSample(size int) Sample {
	return make([]Signal, size)
}

// LoadSample loads a sample from a file
func LoadSample(fileName string) (Sample, error) {

	file, err := os.Open(fileName)

	if err != nil {
		return nil, err
	}

	defer file.Close()

	sample := Sample{}

	scanner := bufio.NewScanner(file)

	for scanner.Scan() {
		sig, err := strconv.ParseFloat(scanner.Text(), 64)

		if err != nil {
			return sample, err
		}

		sample = append(sample, Signal(sig))
	}

	err = scanner.Err()

	return sample, err
}

// AlmostEquals compares samples with a specified tolerance
func (s Sample) AlmostEquals(other Sample, tolerance float64) bool {

	if len(s) != len(other) {
		return false
	}

	for i, val := range s {
		if !val.AlmostEquals(other[i], tolerance) {
			return false
		}
	}
	return true
}

// Equals compares samples with a tolerance of sig digits
func (s Sample) Equals(other Sample) bool {
	return s.AlmostEquals(other, 0.000001)
}

// Mean returns the statistical mean of a sample
func (s Sample) Mean() Signal {
	mean := Signal(0)

	for _, val := range s {
		mean += val
	}

	return mean / Signal(len(s))
}

func (s Sample) variance(mean Signal) Signal {
	variance := Signal(0)
	for _, val := range s {
		variance += Signal(math.Pow(float64(val-mean), 2))
	}
	return variance / Signal(len(s)-1)
}

// Deviation returns the standard deviation for a sample
func (s Sample) Deviation() Signal {
	return s.variance(s.Mean()).Deviation()
}

// Convolution

// RunningSum returns a summed sample
func (s Sample) RunningSum() Sample {
	output := make(Sample, len(s))
	for i, val := range s {
		if i == 0 {
			output[i] = val
		} else {
			output[i] = output[i-1] + val
		}
	}
	return output
}

// Convolution returns the convolution of the sample and an input
func (s Sample) Convolution(input Sample) Sample {

	output := make(Sample, len(s)+len(input))

	for i, val := range s {
		for j, in := range input {
			output[i+j] += val * in
		}
	}
	return output
}

// FirstDifference returns the first difference sample
func (s Sample) FirstDifference() Sample {

	output := make(Sample, len(s))

	for i := range s {
		if i == 0 {
			output[i] = s[i]
		} else {
			output[i] -= s[i-1]
		}
	}
	return output
}

// DFT returns the discrete fourier transform of the sample
func (s Sample) DFT() (*DFT, error) {

	size := len(s)
	halfSize := size / 2

	rex := make(Sample, halfSize)
	imx := make(Sample, halfSize)

	for i := 0; i < halfSize; i++ {
		for j, val := range s {
			rex[i] += val * Signal(math.Cos(float64(i*j)*pi2/float64(size)))
			imx[i] -= val * Signal(math.Sin(float64(i*j)*pi2/float64(size)))
		}
	}

	return NewDFT(rex, imx)
}
