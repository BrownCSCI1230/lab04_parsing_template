#include "sceneparser.h"
#include "scenefilereader.h"
#include <glm/gtx/transform.hpp>

#include <chrono>
#include <iostream>

bool SceneParser::parse(std::string filepath, RenderData &renderData) {
    ScenefileReader fileReader = ScenefileReader(filepath);
    bool success = fileReader.readXML();
    if (!success) {
        return false;
    }

    // Task 4: populate renderData with global data, lights, and camera data

    // Task 5: populate renderData's list of primitives and their transforms.
    //         This will involve traversing the scene graph, and we recommend you
    //         create a helper function to do so!

    return true;
}
