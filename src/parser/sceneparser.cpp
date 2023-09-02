#include "sceneparser.h"
#include "scenefilereader.h"
#include <glm/gtx/transform.hpp>

#include <chrono>
#include <iostream>

struct Word {
    std::string word;
    Word* left;
    Word* right;
};

void dfsPrintTree(Word* w, std::string sentence) {
    // Task 4: Debug this function!!! (Hint: you may need to write/add some code...)
    std::string newSentence = w->word + sentence;
    if (w->left == nullptr && w->right == nullptr) {
        std::cout << newSentence << std::endl;
    }
    dfsPrintTree(w->left, newSentence);
    dfsPrintTree(w->right, newSentence);
}

Word* initTree(std::vector<Word> &words) {
    // STUDENTS - DO NOT TOUCH THIS FUNCTION
    words.reserve(8);
    words.push_back(Word{"2D graphics ", nullptr, nullptr});
    words.push_back(Word{"3D graphics ", nullptr, nullptr});
    words.push_back(Word{"making ", &words[0], &words[1]});
    words.push_back(Word{"CS123 ", nullptr, nullptr});
    words.push_back(Word{"love ", &words[3], &words[2]});
    words.push_back(Word{"bugs ", nullptr, nullptr});
    words.push_back(Word{"hate ", nullptr, &words[5]});
    words.push_back(Word{"I ", &words[4], &words[6]});
    return &words[7];
}

void SceneParser::debugDFS() {
    // Task 4: Uncomment this function
//    std::vector<Word> words;
//    Word* root = initTree(words);
//    std::string sentence = "";
//    dfsPrintTree(root, sentence);
}

bool SceneParser::parse(std::string filepath, RenderData &renderData) {
    ScenefileReader fileReader = ScenefileReader(filepath);
    bool success = fileReader.readJSON();
    if (!success) {
        return false;
    }

    // Task 5: populate renderData with global data, and camera data;

    // Task 6: populate renderData's list of primitives and their transforms.
    //         This will involve traversing the scene graph, and we recommend you
    //         create a helper function to do so!
    return true;
}
