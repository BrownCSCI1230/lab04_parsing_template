#include "sceneparser.h"
#include "scenefilereader.h"
#include "glm/gtx/transform.hpp"

#include <chrono>
#include <memory>
#include <iostream>

bool SceneParser::parse(std::string filepath, RenderData &renderData) {
    ScenefileReader fileReader = ScenefileReader(filepath);
    bool success = fileReader.readXML();
    if (!success) {
        return false;
    }

    // Task 4: Load the global data

    // Task 5: Load the camera data

    // Task 6: Load the light data

    // Task 7: Get the root node of the scene graph

    // Task 8: Perform DFS on the scene graph

    return true;
}
