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

<<includeIntrinsic>>
<<includeclass>>
