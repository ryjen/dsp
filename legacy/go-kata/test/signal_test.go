package dsp

import (
	"micrantha.com/dsp/pkg"
	"testing"
)

func TestSignalDeviation(t *testing.T) {

	expected := dsp.Signal(5)

	signal := dsp.Signal(25)

	actual := signal.Deviation()

	if expected != actual {
		t.Error("expected ", expected, " actual ", actual)
	}
}
