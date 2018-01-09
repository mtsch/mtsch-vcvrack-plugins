#include "mtsch.hpp"
#include <iostream>
#include "dsp/digital.hpp"

// Buffer length in seconds.
#define BUFFER_LENGTH 10

#define MAIN_Y 130

// Feedback and Mix positions.
#define MIX_GROUP_X 20
#define MIX_GROUP_Y MAIN_Y
#define FB_GROUP_X 60
#define FB_GROUP_Y MAIN_Y

// Feedback and Mix offsets.
#define MF_GROUP_CV_AMT_Y 44
#define MF_GROUP_CV_AMT_X 9
#define MF_GROUP_CV_IN_Y 70
#define MF_GROUP_CV_IN_X 6

#define AUDIO_IN_X 25
#define AUDIO_OUT_X 100
#define AUDIO_IO_Y 320

// Aux group.
#define AUX_X 110
#define AUX_Y MAIN_Y
#define AUX_IN_Y 7
#define AUX_LIGHT_X 9
#define AUX_LIGHT_Y MF_GROUP_CV_AMT_Y + 5
#define AUX_OUT_Y MF_GROUP_CV_IN_Y

// Panic / buff full.
#define PANIC_X 15
#define PANIC_Y MAIN_Y + 110
#define BUFF_X 30
#define BUFF_Y 10

// Trigger
#define TRIGGER_X 15
#define TRIGGER_Y 20

struct TriggerFish : Module {
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

    SchmittTrigger trigger;
    SchmittTrigger panic;
    SchmittTrigger clear;
    float *buff;
    int current_i;
    int buff_len;

    TriggerFish() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
        buff_len = BUFFER_LENGTH * engineGetSampleRate();
        buff = new float[buff_len];
    }

    void onReset() { // override
        for (int i = 0; i < buff_len; i++) {
            buff[i] = 0;
        }
        current_i = 0;
    }

    void step() override;
};

void TriggerFish::step() {
    // TODO: cv control
    //       panic button
    //       gui

    // Reset on trigger.
    if (trigger.process(inputs[TRIGGER_IN].value)) {
        current_i = 0;
    }
    // Panic button.
    if (panic.process(params[PANIC_BUTTON].value)) {
        onReset();
        outputs[AUDIO_OUT].value = 0;
        outputs[AUX_OUT].value = 0;
        return;
    }

    if (current_i < buff_len) {
        lights[BUFF_FULL_LIGHT].value = 0;

        // Get feedback amount value.
        float feedback_amt =
            clampf(params[FEEDBACK_KNOB].value +
                   inputs[FEEDBACK_CV].value/5 * params[FEEDBACK_CV_AMT].value,
                   0, 1);

        // Direct feedback when AUX_IN or AUX_OUT are disconnected.
        float feedback;
        if (inputs[AUX_IN].active && outputs[AUX_OUT].active) {
            lights[AUX_ACTIVE_LIGHT].value = 0;
            outputs[AUX_OUT].value = buff[current_i];
            feedback = inputs[AUX_IN].value * feedback_amt;
        } else {
            lights[AUX_ACTIVE_LIGHT].value = 1;
            outputs[AUX_OUT].value = buff[current_i];
            feedback = (inputs[AUX_IN].value + buff[current_i]) * feedback_amt;
        }
        float mix =
            clampf(params[MIX_KNOB].value +
                   (params[MIX_CV_AMT].value * inputs[MIX_CV].value/5),
                   0, 1);
        outputs[AUDIO_OUT].value = (1-mix)*inputs[AUDIO_IN].value + mix*buff[current_i];

        buff[current_i] = clampf(inputs[AUDIO_IN].value + feedback, -5, 5);

        current_i++;
    } else {
        lights[BUFF_FULL_LIGHT].value = 1;
        outputs[AUDIO_OUT].value = 0;
    }
}

TriggerFishWidget::TriggerFishWidget() {

    TriggerFish *module = new TriggerFish();
    setModule(module);
    box.size = Vec(10 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);

    {
        SVGPanel *panel = new SVGPanel();
        panel->box.size = box.size;
        panel->setBackground(SVG::load(assetPlugin(plugin, "res/TriggerFish.svg")));
        addChild(panel);
    }


    // Trigger in.
    addInput(createInput<PJ301MPort>(Vec(TRIGGER_X, TRIGGER_Y),
                                     module, TriggerFish::TRIGGER_IN));

    // Audio IO.
    addInput(createInput<PJ301MPort>(Vec(AUDIO_IN_X, AUDIO_IO_Y),
                                     module, TriggerFish::AUDIO_IN));
    addOutput(createOutput<PJ301MPort>(Vec(AUDIO_OUT_X, AUDIO_IO_Y),
                                       module, TriggerFish::AUDIO_OUT));

    // Aux IO.
    addInput(createInput<PJ301MPort>(Vec(AUX_X, AUX_Y + AUX_IN_Y),
                                     module, TriggerFish::AUX_IN));
    addChild(createLight<SmallLight<RedLight>>(Vec(AUX_X + AUX_LIGHT_X, AUX_Y + AUX_LIGHT_Y),
                                               module, TriggerFish::AUX_ACTIVE_LIGHT));
    addOutput(createOutput<PJ301MPort>(Vec(AUX_X, AUX_Y + AUX_OUT_Y),
                                       module, TriggerFish::AUX_OUT));

    // Feedback.
    addParam(createParam<Davies1900hBlackKnob>(Vec(FB_GROUP_X, FB_GROUP_Y),
                                               module, TriggerFish::FEEDBACK_KNOB, 0, 1, 0.5));
    addParam(createParam<Trimpot>(Vec(FB_GROUP_X + MF_GROUP_CV_AMT_X, FB_GROUP_Y + MF_GROUP_CV_AMT_Y),
                                  module, TriggerFish::FEEDBACK_CV_AMT, -1, 1, 0));
    addInput(createInput<PJ301MPort>(Vec(FB_GROUP_X + MF_GROUP_CV_IN_X, FB_GROUP_Y + MF_GROUP_CV_IN_Y),
                                     module, TriggerFish::FEEDBACK_CV));

    // Mix.
    addParam(createParam<Davies1900hBlackKnob>(Vec(MIX_GROUP_X, MIX_GROUP_Y),
                                               module, TriggerFish::MIX_KNOB, 0, 1, 0.5));
    addParam(createParam<Trimpot>(Vec(MIX_GROUP_X + MF_GROUP_CV_AMT_X, MIX_GROUP_Y + MF_GROUP_CV_AMT_Y),
                                  module, TriggerFish::MIX_CV_AMT, -1, 1, 0));
    addInput(createInput<PJ301MPort>(Vec(MIX_GROUP_X + MF_GROUP_CV_IN_X, MIX_GROUP_Y + MF_GROUP_CV_IN_Y),
                                     module, TriggerFish::MIX_CV));



    // Lights.
    addParam(createParam<BefacoPush>(Vec(PANIC_X, PANIC_Y),
                                     module, TriggerFish::PANIC_BUTTON, 0, 1, 0));
    addChild(createLight<SmallLight<RedLight>>(Vec(PANIC_X + BUFF_X, PANIC_Y + BUFF_Y),
                                               module, TriggerFish::BUFF_FULL_LIGHT));
}
