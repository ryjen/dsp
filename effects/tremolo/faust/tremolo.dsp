import("stdfaust.lib");

rateTarget = hslider("rate_hz", 4.0, 0.1, 20.0, 0.01);
depthTarget = hslider("depth", 0.5, 0.0, 1.0, 0.001);
shapeTarget = hslider("shape", 0.0, 0.0, 1.0, 0.001);

rate = rateTarget : si.smoo;
depth = depthTarget : si.smoo;
shape = shapeTarget : si.smoo;

phase = os.phasor(1.0, rate);
sine = sin(2.0 * ma.PI * phase);
triangle = 1.0 - 4.0 * abs(phase - 0.5);
lfo = (1.0 - shape) * sine + shape * triangle;
modulation = 0.5 * (1.0 + lfo);
gain = 1.0 - depth + depth * modulation;

process = gain;
