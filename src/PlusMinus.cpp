#include "mtsch.hpp"
#include <iostream>

#define NUM_CHANNELS 8
#define SPACING 35
#define X_POSITION 10
#define Y_INPUT_POSITION 77
#define Y_OUTPUT_POSITION 23

struct Sum : Module {
	enum ParamIds {
        PARAMS,
		NUM_PARAMS = PARAMS + NUM_CHANNELS
	};
	enum InputIds {
        INPUTS,
		NUM_INPUTS = INPUTS + NUM_CHANNELS
	};
	enum OutputIds {
		OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	Sum() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {}
	void step() override;

	// For more advanced Module features, read Rack's engine.hpp header file
	// - toJson, fromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu
};

void Sum::step() {
    // Sum the inputs.
    float acc = 0;

    for (int i = 0; i < NUM_CHANNELS; i++) {
      acc += inputs[INPUTS + i].value * params[PARAMS + i].value;
    }

    outputs[OUTPUT].value = acc;
}

SumWidget::SumWidget() {
	Sum *module = new Sum();
	setModule(module);
	box.size = Vec(4 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);

	{
		SVGPanel *panel = new SVGPanel();
		panel->box.size = box.size;
		panel->setBackground(SVG::load(assetPlugin(plugin, "res/Sum.svg")));
		addChild(panel);
	}

	addChild(createScrew<ScrewSilver>(Vec(0, 0)));
	addChild(createScrew<ScrewSilver>(Vec(0, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    for (int i = 0; i < NUM_CHANNELS; i++) {
      addInput(createInput<PJ301MPort>(Vec(X_POSITION, Y_INPUT_POSITION + i * SPACING),
                                       module, Sum::INPUTS + i));
      addParam(createParam<CKSSThree>(Vec(X_POSITION + 30, Y_INPUT_POSITION + i * SPACING),
                                      module, Sum::PARAMS + i, -1, 1, 1));
    }

	addOutput(createOutput<PJ301MPort>(Vec(X_POSITION, Y_OUTPUT_POSITION), module, Sum::OUTPUT));
}
