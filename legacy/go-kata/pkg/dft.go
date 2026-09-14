package dsp

import (
	"errors"
	"math"
)

// DFT Discrete Fourier Transform
type DFT struct {
	rex Sample
	imx Sample
}

const pi2 = math.Pi * 2

// NewDFT Initializes a new DFT
func NewDFT(rex Sample, imx Sample) (*DFT, error) {

	if len(rex) != len(imx) {
		return nil, errors.New("invalid argument")
	}

	return &DFT{rex, imx}, nil
}

// Equals compares two DFT instances
func (dft *DFT) Equals(other *DFT) bool {
	return dft.rex.Equals(other.rex) && dft.imx.Equals(other.imx)
}

func (dft *DFT) length() int {
	return len(dft.rex)
}

func (dft *DFT) size() int {
	return dft.length() * 2
}

// Magnitude returns the magnitude of the samples
func (dft *DFT) Magnitude() Sample {

	var rex, imx float64

	len := dft.length()

	output := make(Sample, len)

	for i := 0; i < len; i++ {
		rex = math.Pow(float64(dft.rex[i]), 2)
		imx = math.Pow(float64(dft.imx[i]), 2)
		output[i] = Signal(math.Sqrt(rex * imx))
	}

	return output
}

// Inverse return the inverse sample of the discrete fourier transform
func (dft *DFT) Inverse() Sample {

	length := dft.length()
	size := dft.size()
	fsize := float64(size)

	output := make(Sample, size)

	for i := 0; i < length; i++ {
		dft.rex[i] = dft.rex[i] / Signal(length)
		dft.imx[i] = -dft.imx[i] / Signal(length)
	}

	dft.rex[0] = dft.rex[0] / 2
	dft.imx[0] = -dft.imx[0] / 2

	for i := 0; i < size; i++ {
		for j := 0; j < length; j++ {
			output[i] += dft.rex[j] * Signal(math.Cos(float64(i*j)*pi2/fsize))
			output[i] += dft.imx[j] * Signal(math.Sin(float64(i*j)*pi2/fsize))
		}
	}

	return output
}

// Polar converts from Rect to Polar
func (dft *DFT) Polar() (*DFT, error) {

	len := dft.length()

	mag := make(Sample, len)
	phase := make(Sample, len)

	for i := 0; i < len; i++ {

		rex := float64(dft.rex[i])
		imx := float64(dft.imx[i])

		mag[i] = Signal(math.Sqrt(math.Pow(rex, 2) + math.Pow(imx, 2)))

		if dft.rex[i].Equals(0) {
			dft.rex[i] = Signal(math.Pow(10, -20))
			phase[i] = Signal(math.Atan(imx / rex))
		}

		if dft.rex[i] < 0 && dft.imx[i] < 0 {
			phase[i] -= Signal(math.Pi)
		}
		if dft.rex[i] < 0 && dft.imx[i] >= 0 {
			phase[i] += Signal(math.Pi)
		}
	}

	return NewDFT(mag, phase)
}

// Complex converts the dft to frequency domain
func (dft *DFT) Complex() (*DFT, error) {

	var rex, imx, k, sr, si float64

	len := dft.length()

	freqRex := make(Sample, len)
	freqImx := make(Sample, len)

	for i := 0; i < len; i++ {
		for j := 0; j < len; j++ {
			rex = float64(dft.rex[i])
			imx = float64(dft.imx[i])
			k = float64(i * j)
			sr = math.Cos(pi2 * k / float64(len))
			si = -math.Sin(pi2 * k / float64(len))

			freqRex[i] += Signal(rex*sr - imx*si)
			freqImx[i] += Signal(imx*si - imx*sr)
		}
	}
	return NewDFT(freqRex, freqImx)
}
