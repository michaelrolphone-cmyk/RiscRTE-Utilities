#pragma once
#include "RiscSceneComponentsV1.h"
typedef struct {risc_components_document_v1 document;risc_scene_navigation_v1 navigation;uint64_t scene,canvas,text;uint32_t flags,width,height,scale,transforms;unsigned page;bool running,cursor;} spectrum_test_view;
typedef void(*spectrum_test_observer)(spectrum_test_view*);
