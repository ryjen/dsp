package dsp

import (
	"micrantha.com/dsp/pkg"
	"testing"
)

func TestNewDFT(t *testing.T) {

	rex := make(dsp.Sample, 50)
	imx := make(dsp.Sample, 25)

	_, err := dsp.NewDFT(rex, imx)

	if err == nil {
		t.Error("Invalid arguments allowed for dft")
	}
}

func TestDFTMagnitude(t *testing.T) {

	expected := testSamples[output15khzMagnitude]

	dft, err := testSamples[input15khzSignal].DFT()

	if err != nil {
		t.Error("generating dft: ", err.Error())
	}

	actual := dft.Magnitude()

	if !actual.Equals(expected) {
		t.Error("expected ", expected, " actual ", actual)
	}
}

func TestSampleInverse(t *testing.T) {

	expected := testSamples[outputIdft]

	dft, err := testSamples[input15khzSignal].DFT()

	if err != nil {
		t.Error("generating dft: ", err.Error())
	}

	actual := dft.Inverse()

	if !actual.Equals(expected) {
		t.Error("expected ", expected, " actual ", actual)
	}
}

func TestDFTPolar(t *testing.T) {

	expected, err := dsp.NewDFT(
		testSamples[outputDftRex],
		testSamples[outputDftImx])

	if err != nil {
		t.Error("creating dft: ", err.Error())
	}

	dft, err := testSamples[input15khzSignal].DFT()

	if err != nil {
		t.Error("generating dft: ", err.Error())
	}

	actual, err := dft.Polar()

	if err != nil {
		t.Error("generating polar dft: ", err.Error())
	}

	if !actual.Equals(expected) {
		t.Error("expected ", expected, " actual ", actual)
	}
}

func TestDFTComplex(t *testing.T) {

	expected, err := dsp.NewDFT(
		testSamples[outputComplexRex],
		testSamples[outputComplexImx])

	if err != nil {
		t.Error("creating dft: ", err.Error())
	}

	dft, err := dsp.NewDFT(testSamples[input20khzRex],
		testSamples[input20khzImx])

	if err != nil {
		t.Error("generating dft: ", err.Error())
	}

	actual, err := dft.Complex()

	if err != nil {
		t.Error("generating complex dft: ", err.Error())
	}

	if !actual.Equals(expected) {
		t.Error("expected ", expected, " actual ", actual)
	}
}
