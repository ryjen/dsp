namespace faust_generated {
/* ------------------------------------------------------------
name: "tremolo"
Code generated with Faust 2.85.5 (https://faust.grame.fr)
Compilation options: -a faust/architectures/standalone-header.cpp -lang cpp -i -inpl -fpga-mem-th 4 -light -nvi -ct 1 -cn TremoloFaust -scn FaustDspBase -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 0
------------------------------------------------------------ */

#ifndef  __TremoloFaust_H__
#define  __TremoloFaust_H__

#include <cstddef>

namespace faust_generated {

struct Soundfile;

struct Meta {
    virtual ~Meta() = default;
    virtual void declare(const char*, const char*) = 0;
};

struct UI {
    virtual ~UI() = default;
    virtual void openTabBox(const char*) = 0;
    virtual void openHorizontalBox(const char*) = 0;
    virtual void openVerticalBox(const char*) = 0;
    virtual void closeBox() = 0;
    virtual void addButton(const char*, float*) = 0;
    virtual void addCheckButton(const char*, float*) = 0;
    virtual void addVerticalSlider(const char*, float*, float, float, float, float) = 0;
    virtual void addHorizontalSlider(const char*, float*, float, float, float, float) = 0;
    virtual void addNumEntry(const char*, float*, float, float, float, float) = 0;
    virtual void addHorizontalBargraph(const char*, float*, float, float) = 0;
    virtual void addVerticalBargraph(const char*, float*, float, float) = 0;
    virtual void addSoundfile(const char*, const char*, Soundfile**) = 0;
    virtual void declare(float*, const char*, const char*) {}
};

class FaustDspBase {
public:
    virtual ~FaustDspBase() = default;
};

}  // namespace faust_generated

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <math.h>

#ifndef FAUSTCLASS 
#define FAUSTCLASS TremoloFaust
#endif

#ifdef __APPLE__ 
#define exp10f __exp10f
#define exp10 __exp10
#endif

#if defined(_WIN32)
#define RESTRICT __restrict
#else
#define RESTRICT __restrict__
#endif


class TremoloFaust final : public FaustDspBase {
	
 private:
	
	int iVec0[2];
	int fSampleRate;
	float fConst0;
	float fConst1;
	float fConst2;
	FAUSTFLOAT fHslider0;
	float fRec1[2];
	float fConst3;
	float fRec0[2];
	FAUSTFLOAT fHslider1;
	float fRec2[2];
	FAUSTFLOAT fHslider2;
	float fRec3[2];
	
 public:
	TremoloFaust() {
	}
	
	TremoloFaust(const TremoloFaust&) = default;
	
	virtual ~TremoloFaust() = default;
	
	TremoloFaust& operator=(const TremoloFaust&) = default;
	
	void metadata(Meta* m) { 
		m->declare("compile_options", "-a faust/architectures/standalone-header.cpp -lang cpp -i -inpl -fpga-mem-th 4 -light -nvi -ct 1 -cn TremoloFaust -scn FaustDspBase -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 0");
		m->declare("filename", "tremolo.dsp");
		m->declare("maths.lib/author", "GRAME");
		m->declare("maths.lib/copyright", "GRAME");
		m->declare("maths.lib/license", "LGPL with exception");
		m->declare("maths.lib/name", "Faust Math Library");
		m->declare("maths.lib/version", "2.9.0");
		m->declare("name", "tremolo");
		m->declare("oscillators.lib/name", "Faust Oscillator Library");
		m->declare("oscillators.lib/version", "1.7.0");
		m->declare("platform.lib/name", "Generic Platform Library");
		m->declare("platform.lib/version", "1.3.0");
		m->declare("signals.lib/name", "Faust Routing Library");
		m->declare("signals.lib/version", "1.6.0");
	}

	static constexpr int getStaticNumInputs() {
		return 0;
	}
	static constexpr int getStaticNumOutputs() {
		return 1;
	}
	int getNumInputs() {
		return 0;
	}
	int getNumOutputs() {
		return 1;
	}
	
	static void classInit(int sample_rate) {
	}
	
	void instanceConstants(int sample_rate) {
		fSampleRate = sample_rate;
		fConst0 = std::min<float>(1.92e+05f, std::max<float>(1.0f, static_cast<float>(fSampleRate)));
		fConst1 = 44.1f / fConst0;
		fConst2 = 1.0f - fConst1;
		fConst3 = 1.0f / fConst0;
	}
	
	void instanceResetUserInterface() {
		fHslider0 = static_cast<FAUSTFLOAT>(4.0f);
		fHslider1 = static_cast<FAUSTFLOAT>(0.0f);
		fHslider2 = static_cast<FAUSTFLOAT>(0.5f);
	}
	
	void instanceClear() {
		for (int l0 = 0; l0 < 2; l0 = l0 + 1) {
			iVec0[l0] = 0;
		}
		for (int l1 = 0; l1 < 2; l1 = l1 + 1) {
			fRec1[l1] = 0.0f;
		}
		for (int l2 = 0; l2 < 2; l2 = l2 + 1) {
			fRec0[l2] = 0.0f;
		}
		for (int l3 = 0; l3 < 2; l3 = l3 + 1) {
			fRec2[l3] = 0.0f;
		}
		for (int l4 = 0; l4 < 2; l4 = l4 + 1) {
			fRec3[l4] = 0.0f;
		}
	}
	
	void init(int sample_rate) {
		classInit(sample_rate);
		instanceInit(sample_rate);
	}
	
	void instanceInit(int sample_rate) {
		instanceConstants(sample_rate);
		instanceResetUserInterface();
		instanceClear();
	}
	
	TremoloFaust* clone() {
		return new TremoloFaust(*this);
	}
	
	int getSampleRate() {
		return fSampleRate;
	}
	
	void buildUserInterface(UI* ui_interface) {
		ui_interface->openVerticalBox("tremolo");
		ui_interface->addHorizontalSlider("depth", &fHslider2, FAUSTFLOAT(0.5f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1.0f), FAUSTFLOAT(0.001f));
		ui_interface->addHorizontalSlider("rate_hz", &fHslider0, FAUSTFLOAT(4.0f), FAUSTFLOAT(0.1f), FAUSTFLOAT(2e+01f), FAUSTFLOAT(0.01f));
		ui_interface->addHorizontalSlider("shape", &fHslider1, FAUSTFLOAT(0.0f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1.0f), FAUSTFLOAT(0.001f));
		ui_interface->closeBox();
	}
	
	void compute(int count, FAUSTFLOAT** inputs, FAUSTFLOAT** outputs) {
		FAUSTFLOAT* output0 = outputs[0];
		float fSlow0 = fConst1 * static_cast<float>(fHslider0);
		float fSlow1 = fConst1 * static_cast<float>(fHslider1);
		float fSlow2 = fConst1 * static_cast<float>(fHslider2);
		for (int i0 = 0; i0 < count; i0 = i0 + 1) {
			iVec0[0] = 1;
			fRec1[0] = fSlow0 + fConst2 * fRec1[1];
			float fTemp0 = ((1 - iVec0[1]) ? 0.0f : fRec0[1] + fConst3 * fRec1[0]);
			fRec0[0] = fTemp0 - std::floor(fTemp0);
			fRec2[0] = fSlow1 + fConst2 * fRec2[1];
			fRec3[0] = fSlow2 + fConst2 * fRec3[1];
			output0[i0] = static_cast<FAUSTFLOAT>(fRec3[0] * (0.5f * ((1.0f - fRec2[0]) * std::sin(6.2831855f * fRec0[0]) + fRec2[0] * (1.0f - 4.0f * std::fabs(fRec0[0] + -0.5f)) + 1.0f) + -1.0f) + 1.0f);
			iVec0[1] = iVec0[0];
			fRec1[1] = fRec1[0];
			fRec0[1] = fRec0[0];
			fRec2[1] = fRec2[0];
			fRec3[1] = fRec3[0];
		}
	}

};

#endif
} // namespace faust_generated
