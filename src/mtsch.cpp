#include "mtsch.hpp"

Plugin *pluginInstance;

void init(rack::Plugin *p) {
    pluginInstance = p;

    p->addModel(modelSum);
    p->addModel(modelRationals);
    p->addModel(modelTriggerPanic);

}
