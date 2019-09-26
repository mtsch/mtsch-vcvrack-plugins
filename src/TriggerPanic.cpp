#include "mtsch.hpp"
#include <iostream>

// Buffer length in seconds.
#define BUFFER_LENGTH 10

#define MAIN_X 25
#define MAIN_Y 40
#define Y_SPACING 80
#define X_INPUT_OFFSET 7

// Trigger and button
#define TRIGGER_X MAIN_X + X_INPUT_OFFSET
#define TRIGGER_Y MAIN_Y

#define PANIC_X MAIN_X + 70 -1
#define PANIC_Y MAIN_Y - 1
#define BUFF_X MAIN_X + 100
#define BUFF_Y MAIN_Y + 10

// Feedback and Mix positions.
#define MIX_GROUP_X MAIN_X
#define MIX_GROUP_Y MAIN_Y + Y_SPACING
#define FB_GROUP_X MAIN_X
#define FB_GROUP_Y MAIN_Y + 2*Y_SPACING

// Feedback and Mix offsets.
#define MF_GROUP_CV_AMT_X 44
#define MF_GROUP_CV_AMT_Y 9
#define MF_GROUP_CV_IN_X 70
#define MF_GROUP_CV_IN_Y 6

// Aux group.
#define AUX_X MAIN_X + X_INPUT_OFFSET
#define AUX_Y MAIN_Y + 3*Y_SPACING + 5
#define AUX_IN_X 7
#define AUX_LIGHT_Y AUX_Y + 9
#define AUX_LIGHT_X MAIN_X + MF_GROUP_CV_AMT_X + 3
#define AUX_OUT_X MAIN_X + MF_GROUP_CV_IN_X

#define AUDIO_IN_X MAIN_X + X_INPUT_OFFSET
#define AUDIO_OUT_X AUX_OUT_X
#define AUDIO_IO_Y MAIN_Y + 3*Y_SPACING + Y_SPACING/2 + 10

struct TriggerPanic : Module {
    enum ParamIds {
        FEEDBACK_KNOB,
        MIX_KNOB,

        // CV knobs.
        FEEDBACK_CV_AMT,
        MIX_CV_AMT,

        // Buttons.
        PANIC_BUTTON,

        NUM_PARAMS
    };
    enum InputIds {
        // Audio.
        AUDIO_IN,
        AUX_IN,

        // Trig.
        TRIGGER_IN,

        // CVs.
        FEEDBACK_CV,
        MIX_CV,

        NUM_INPUTS
    };
    enum OutputIds {
        AUDIO_OUT,
        AUX_OUT,
        NUM_OUTPUTS
    };
    enum LightIds {
        BUFF_FULL_LIGHT,
        AUX_ACTIVE_LIGHT,

        NUM_LIGHTS
    };

    dsp::SchmittTrigger trigger;
    dsp::SchmittTrigger panic;
    dsp::SchmittTrigger clear;
    float *buff;
    int current_i;
    int buff_len;

    TriggerPanic() {
      config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
      configParam(TriggerPanic::PANIC_BUTTON, 0, 1, 0);
      configParam(TriggerPanic::MIX_KNOB, 0, 1, 0.5);
      configParam(TriggerPanic::MIX_CV_AMT, -1, 1, 0);
      configParam(TriggerPanic::FEEDBACK_KNOB, 0, 1, 0.5);
      configParam(TriggerPanic::FEEDBACK_CV_AMT, -1, 1, 0);
      buff_len = BUFFER_LENGTH * APP->engine->getSampleRate();
      buff = new float[buff_len];
    }

    void onReset() override {
        for (int i = 0; i < buff_len; i++) {
            buff[i] = 0;
        }
        current_i = 0;
    }

    void process(const ProcessArgs& args) override;
};

void TriggerPanic::process(const ProcessArgs& args) {
    // TODO: cv control
    //       panic button
    //       gui

    // Reset on trigger.
    if (trigger.process(inputs[TRIGGER_IN].getVoltage())) {
        current_i = 0;
    }
    // Panic button.
    if (panic.process(params[PANIC_BUTTON].getValue())) {
        onReset();
        outputs[AUDIO_OUT].setVoltage(0);
        outputs[AUX_OUT].setVoltage(0);
        return;
    }

    if (current_i < buff_len) {
        lights[BUFF_FULL_LIGHT].value = 0;

        // Get feedback amount value.
        float feedback_amt =
            clamp(params[FEEDBACK_KNOB].getValue() +
                   inputs[FEEDBACK_CV].getVoltage()/5 * params[FEEDBACK_CV_AMT].getValue(),
                   0.0f, 1.0f);

        // Direct feedback when AUX_IN or AUX_OUT are disconnected.
        float feedback;
        if (inputs[AUX_IN].isConnected() && outputs[AUX_OUT].isConnected()) {
            lights[AUX_ACTIVE_LIGHT].value = 0;
            outputs[AUX_OUT].setVoltage(buff[current_i]);
            feedback = inputs[AUX_IN].getVoltage() * feedback_amt;
        } else {
            lights[AUX_ACTIVE_LIGHT].value = 1;
            outputs[AUX_OUT].setVoltage(buff[current_i]);
            feedback = (inputs[AUX_IN].getVoltage() + buff[current_i]) * feedback_amt;
        }
        float mix =
            clamp(params[MIX_KNOB].getValue() +
                   (params[MIX_CV_AMT].getValue() * inputs[MIX_CV].getVoltage()/5),
                   0.0f, 1.0f);
        outputs[AUDIO_OUT].setVoltage((1-mix)*inputs[AUDIO_IN].getVoltage() + mix*buff[current_i]);

        buff[current_i] = clamp(inputs[AUDIO_IN].getVoltage() + feedback, -5.0f, 5.0f);

        current_i++;
    } else {
        lights[BUFF_FULL_LIGHT].value = 1;
        outputs[AUDIO_OUT].setVoltage(0);
    }
}

struct TriggerPanicWidget : ModuleWidget {
    TriggerPanicWidget(TriggerPanic *module);
};

TriggerPanicWidget::TriggerPanicWidget(TriggerPanic *module) {
		setModule(module);
    box.size = Vec(10 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);

    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/TriggerPanic.svg")));

    addChild(createWidget<ScrewBlack>(Vec(15, 0)));
    addChild(createWidget<ScrewBlack>(Vec(box.size.x-30, 0)));
    addChild(createWidget<ScrewBlack>(Vec(15, 365)));
    addChild(createWidget<ScrewBlack>(Vec(box.size.x-30, 365)));

    // Trigger in.
    addInput(createInput<PJ301MPort>(Vec(TRIGGER_X, TRIGGER_Y), module, TriggerPanic::TRIGGER_IN));

    // Panic.
    addParam(createParam<BefacoPush>(Vec(PANIC_X, PANIC_Y), module, TriggerPanic::PANIC_BUTTON));
    addChild(createLight<SmallLight<RedLight>>(Vec(BUFF_X, BUFF_Y), module, TriggerPanic::BUFF_FULL_LIGHT));

    // Mix.
    addParam(createParam<Davies1900hBlackKnob>(Vec(MIX_GROUP_X, MIX_GROUP_Y), module, TriggerPanic::MIX_KNOB));
    addParam(createParam<Trimpot>(Vec(MIX_GROUP_X + MF_GROUP_CV_AMT_X, MIX_GROUP_Y + MF_GROUP_CV_AMT_Y), module, TriggerPanic::MIX_CV_AMT));
    addInput(createInput<PJ301MPort>(Vec(MIX_GROUP_X + MF_GROUP_CV_IN_X, MIX_GROUP_Y + MF_GROUP_CV_IN_Y), module, TriggerPanic::MIX_CV));

    // Feedback.
    addParam(createParam<Davies1900hBlackKnob>(Vec(FB_GROUP_X, FB_GROUP_Y), module, TriggerPanic::FEEDBACK_KNOB));
    addParam(createParam<Trimpot>(Vec(FB_GROUP_X + MF_GROUP_CV_AMT_X, FB_GROUP_Y + MF_GROUP_CV_AMT_Y), module, TriggerPanic::FEEDBACK_CV_AMT));
    addInput(createInput<PJ301MPort>(Vec(FB_GROUP_X + MF_GROUP_CV_IN_X, FB_GROUP_Y + MF_GROUP_CV_IN_Y), module, TriggerPanic::FEEDBACK_CV));

    // Aux IO.
    addInput(createInput<PJ301MPort>(Vec(AUX_X, AUX_Y), module, TriggerPanic::AUX_IN));
    addChild(createLight<SmallLight<GreenLight>>(Vec(AUX_LIGHT_X, AUX_LIGHT_Y), module, TriggerPanic::AUX_ACTIVE_LIGHT));
    addOutput(createOutput<PJ301MPort>(Vec(AUX_OUT_X, AUX_Y), module, TriggerPanic::AUX_OUT));

    // Audio IO.
    addInput(createInput<PJ301MPort>(Vec(AUDIO_IN_X, AUDIO_IO_Y), module, TriggerPanic::AUDIO_IN));
    addOutput(createOutput<PJ301MPort>(Vec(AUDIO_OUT_X, AUDIO_IO_Y), module, TriggerPanic::AUDIO_OUT));

}

Model *modelTriggerPanic = createModel<TriggerPanic, TriggerPanicWidget>("TriggerPanic");
