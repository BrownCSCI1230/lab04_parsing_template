#include "scenefilereader.h"
#include "scenedata.h"

#include "glm/gtc/type_ptr.hpp"

#include <cassert>
#include <cstring>
#include <iostream>
#include <filesystem>

#include <QFile>
#include <QJsonArray>

#define ERROR_AT(e) "error at line " << e.lineNumber() << " col " << e.columnNumber() << ": "
#define PARSE_ERROR(e) std::cout << ERROR_AT(e) << "could not parse <" << e.tagName().toStdString() \
   << ">" << std::endl
#define UNSUPPORTED_ELEMENT(e) std::cout << ERROR_AT(e) << "unsupported element <" \
   << e.tagName().toStdString() << ">" << std::endl;

// Students, please ignore this file.

ScenefileReader::ScenefileReader(const std::string& name)
{
   file_name = name;

   memset(&m_cameraData, 0, sizeof(SceneCameraData));
   memset(&m_globalData, 0, sizeof(SceneGlobalData));
   m_objects.clear();
   m_lights.clear();
   m_nodes.clear();
}

ScenefileReader::~ScenefileReader()
{
   std::vector<SceneLightData*>::iterator lights;
   for (lights = m_lights.begin(); lights != m_lights.end(); lights++) {
       delete *lights;
   }

   // Delete all Scene Nodes
   for (unsigned int node = 0; node < m_nodes.size(); node++) {
       for (size_t i = 0; i < (m_nodes[node])->transformations.size(); i++) {
           delete (m_nodes[node])->transformations[i];
       }
       for (size_t i = 0; i < (m_nodes[node])->primitives.size(); i++) {
           delete (m_nodes[node])->primitives[i];
       }
       (m_nodes[node])->transformations.clear();
       (m_nodes[node])->primitives.clear();
       (m_nodes[node])->children.clear();
       delete m_nodes[node];
   }

   m_nodes.clear();
   m_lights.clear();
   m_objects.clear();
}

SceneGlobalData ScenefileReader::getGlobalData() const {
   return m_globalData;
}

SceneCameraData ScenefileReader::getCameraData() const {
   return m_cameraData;
}

std::vector<SceneLightData> ScenefileReader::getLights() const {
   std::vector<SceneLightData> ret{};
   ret.reserve(m_lights.size());

   for (auto light : m_lights) {
       ret.emplace_back(*light);
   }
   return ret;
}

SceneNode* ScenefileReader::getRootNode() const {
   std::map<std::string, SceneNode*>::iterator node = m_objects.find("root");
   if (node == m_objects.end())
       return nullptr;
   return m_objects["root"];
}

// This is where it all goes down...
bool ScenefileReader::readJSON() {
   // Read the file
   QFile file(file_name.c_str());
   if (!file.open(QFile::ReadOnly)) {
       std::cout << "could not open " << file_name << std::endl;
       return false;
   }

   // Load the JSON document
   QString errorMessage;
   QByteArray fileContents = file.readAll();
   QJsonParseError jsonError;
   QJsonDocument doc = QJsonDocument::fromJson(fileContents, &jsonError);
   if (doc.isNull()) {
      std::cout << "could not parse " << file_name << std::endl;
         std::cout << "parse error at line " << jsonError.offset << ": "
                  << jsonError.errorString().toStdString() << std::endl;
         return false;
   }
   file.close();

   if (!doc.isObject()) {
      std::cout << "document is not an object" << std::endl;
      return false;
   }

   // Get the root element
   QJsonObject scenefile = doc.object();

   if (!scenefile.contains("globalData")) {
      std::cout << "missing required field \"globalData\" on root object" << std::endl;
      return false;
   }
   if (!scenefile.contains("cameraData")) {
      std::cout << "missing required field \"cameraData\" on root object" << std::endl;
      return false;
   }

   QStringList requiredFields = {"globalData", "cameraData"};
   QStringList optionalFields = {"name", "groups", "templateGroups"};
   // If other fields are present, raise an error
   QStringList allFields = requiredFields + optionalFields;
   for (auto field : scenefile.keys()) {
      if (!allFields.contains(field)) {
         std::cout << "unknown field \"" << field.toStdString() << "\" on root object" << std::endl;
         return false;
      }
   } 

   // Default camera
   m_cameraData.pos = glm::vec4(5.f, 5.f, 5.f, 1.f);
   m_cameraData.up = glm::vec4(0.f, 1.f, 0.f, 0.f);
   m_cameraData.look = glm::vec4(-1.f, -1.f, -1.f, 0.f);
   m_cameraData.heightAngle = 45 * M_PI / 180.f;

   // Default global data
   m_globalData.ka = 0.5f;
   m_globalData.kd = 0.5f;
   m_globalData.ks = 0.5f;

   // Parse the global data
   if (!parseGlobalData(scenefile["globalData"].toObject())) {
      std::cout << "could not parse \"globalData\"" << std::endl;
      return false;
   }

   // Parse the camera data
   if (!parseCameraData(scenefile["cameraData"].toObject())) {
      std::cout << "could not parse \"cameraData\"" << std::endl;
      return false;
   }

   // Parse the groups
   if (scenefile.contains("groups")) {
      QJsonObject groups = scenefile["groups"].toObject();
      for (auto group : groups.keys()) {
         if (!parseGroupData(groups[group].toObject())) {
            std::cout << "could not parse group \"" << group.toStdString() << "\"" << std::endl;
            return false;
         }
      }
   }

   std::cout << "Finished reading " << file_name << std::endl;
   return true;
}

/**
* Helper function to parse a single value, the name of which is stored in
* name.  For example, to parse <length v="0"/>, name would need to be "v".
*/
bool parseInt(const QDomElement &single, int &a, const char *name) {
   if (!single.hasAttribute(name))
       return false;
   a = single.attribute(name).toInt();
   return true;
}

/**
* Helper function to parse a single value, the name of which is stored in
* name.  For example, to parse <length v="0"/>, name would need to be "v".
*/
template <typename T> bool parseSingle(const QDomElement &single, T &a, const QString &str) {
   if (!single.hasAttribute(str))
       return false;
   a = single.attribute(str).toDouble();
   return true;
}

/**
* Helper function to parse a triple.  Each attribute is assumed to take a
* letter, which are stored in chars in order.  For example, to parse
* <pos x="0" y="0" z="0"/>, chars would need to be "xyz".
*/
template <typename T> bool parseTriple(
       const QDomElement &triple,
       T &a,
       T &b,
       T &c,
       const QString &str_a,
       const QString &str_b,
       const QString &str_c) {
   if (!triple.hasAttribute(str_a) ||
       !triple.hasAttribute(str_b) ||
       !triple.hasAttribute(str_c))
       return false;
   a = triple.attribute(str_a).toDouble();
   b = triple.attribute(str_b).toDouble();
   c = triple.attribute(str_c).toDouble();
   return true;
}

/**
* Helper function to parse a quadruple.  Each attribute is assumed to take a
* letter, which are stored in chars in order.  For example, to parse
* <color r="0" g="0" b="0" a="0"/>, chars would need to be "rgba".
*/
template <typename T> bool parseQuadruple(
       const QDomElement &quadruple,
       T &a,
       T &b,
       T &c,
       T &d,
       const QString &str_a,
       const QString &str_b,
       const QString &str_c,
       const QString &str_d) {
   if (!quadruple.hasAttribute(str_a) ||
       !quadruple.hasAttribute(str_b) ||
       !quadruple.hasAttribute(str_c) ||
       !quadruple.hasAttribute(str_d))
       return false;
   a = quadruple.attribute(str_a).toDouble();
   b = quadruple.attribute(str_b).toDouble();
   c = quadruple.attribute(str_c).toDouble();
   d = quadruple.attribute(str_d).toDouble();
   return true;
}

/**
* Helper function to parse a matrix. Assumes the input matrix is row-major, which is converted to
* a column-major glm matrix.
*
* Example matrix:
*
* <matrix>
*   <row a="1" b="0" c="0" d="0"/>
*   <row a="0" b="1" c="0" d="0"/>
*   <row a="0" b="0" c="1" d="0"/>
*   <row a="0" b="0" c="0" d="1"/>
* </matrix>
*/
bool parseMatrix(const QDomElement &matrix, glm::mat4 &m) {
   float *valuePtr = glm::value_ptr(m);
   QDomNode childNode = matrix.firstChild();
   int col = 0;

   while (!childNode.isNull()) {
       QDomElement e = childNode.toElement();
       if (e.isElement()) {
           float a, b, c, d;
           if (!parseQuadruple(e, a, b, c, d, "a", "b", "c", "d")
                   && !parseQuadruple(e, a, b, c, d, "v1", "v2", "v3", "v4")) {
               PARSE_ERROR(e);
               return false;
           }
           valuePtr[0*4 + col] = a;
           valuePtr[1*4 + col] = b;
           valuePtr[2*4 + col] = c;
           valuePtr[3*4 + col] = d;
           if (++col == 4) break;
       }
       childNode = childNode.nextSibling();
   }

   return (col == 4);
}

/**
* Helper function to parse a color.  Will parse an element with r, g, b, and
* a attributes (the a attribute is optional and defaults to 1).
*/
bool parseColor(const QDomElement &color, SceneColor &c) {
   c.a = 1;
   return parseQuadruple(color, c.r, c.g, c.b, c.a, "r", "g", "b", "a") ||
          parseQuadruple(color, c.r, c.g, c.b, c.a, "x", "y", "z", "w") ||
          parseTriple(color, c.r, c.g, c.b, "r", "g", "b") ||
          parseTriple(color, c.r, c.g, c.b, "x", "y", "z");
}

/**
* Helper function to parse a texture map tag. The texture map image should be relative to the root directory of
* scenefile root. Example texture map tag:
* <texture file="/image/andyVanDam.jpg" u="1" v="1"/>
*/
bool parseMap(const QDomElement &e, SceneFileMap &map, const std::filesystem::path &basepath) {
   if (!e.hasAttribute("file"))
       return false;

   std::filesystem::path fileRelativePath(e.attribute("file").toStdString());

   map.filename = (basepath / fileRelativePath).string();
   map.repeatU = e.hasAttribute("u") ? e.attribute("u").toFloat() : 1;
   map.repeatV = e.hasAttribute("v") ? e.attribute("v").toFloat() : 1;
   map.isUsed = true;
   return true;
}

/**
* Parse a globalData field and fill in m_globalData.
*/
bool ScenefileReader::parseGlobalData(const QJsonObject &globalData) {
   QStringList requiredFields = {"ambientCoeff", "diffuseCoeff", "specularCoeff"};
   QStringList optionalFields = {"transparentCoeff"};
   QStringList allFields = requiredFields + optionalFields;
   for (auto field : globalData.keys()) {
      if (!allFields.contains(field)) {
         std::cout << "unknown field \"" << field.toStdString() << "\" on globalData object" << std::endl;
         return false;
      }
   }
   for (auto field : requiredFields) {
      if (!globalData.contains(field)) {
         std::cout << "missing required field \"" << field.toStdString() << "\" on globalData object" << std::endl;
         return false;
      }
   }
   // Parse the global data
   if (globalData["ambientCoeff"].isDouble()) {
      m_globalData.ka = globalData["ambientCoeff"].toDouble();
   } else {
      std::cout << "globalData ambientCoeff must be a floating-point value" << std::endl;
      return false;
   }
   if (globalData["diffuseCoeff"].isDouble()) {
      m_globalData.kd = globalData["diffuseCoeff"].toDouble();
   } else {
      std::cout << "globalData diffuseCoeff must be a floating-point value" << std::endl;
      return false;
   }
   if (globalData["specularCoeff"].isDouble()) {
      m_globalData.ks = globalData["specularCoeff"].toDouble();
   } else {
      std::cout << "globalData specularCoeff must be a floating-point value" << std::endl;
      return false;
   }
   if (globalData.contains("transparentCoeff")) {
      if (globalData["transparentCoeff"].isDouble()) {
         m_globalData.kt = globalData["transparentCoeff"].toDouble();
      } else {
         std::cout << "globalData transparentCoeff must be a floating-point value" << std::endl;
         return false;
      }
   }

   return true;
}

/**
* Parse a Light and add a new CS123SceneLightData to m_lights.
*/
bool ScenefileReader::parseLightData(const QDomElement &lightdata) {
   // Create a default light
   SceneLightData* light = new SceneLightData();
   m_lights.push_back(light);
   memset(light, 0, sizeof(SceneLightData));
   light->pos = glm::vec4(3.f, 3.f, 3.f, 1.f);
   light->dir = glm::vec4(0.f, 0.f, 0.f, 0.f);
   light->color.r = light->color.g = light->color.b = 1;
   light->function = glm::vec3(1, 0, 0);

   // Iterate over child elements
   QDomNode childNode = lightdata.firstChild();
   while (!childNode.isNull()) {
       QDomElement e = childNode.toElement();
       if (e.tagName() == "id") {
           if (!parseInt(e, light->id, "v")) {
               PARSE_ERROR(e);
               return false;
           }
       } else if (e.tagName() == "type") {
           if (!e.hasAttribute("v")) {
               PARSE_ERROR(e);
               return false;
           }
           if (e.attribute("v") == "directional") light->type = LightType::LIGHT_DIRECTIONAL;
           else if (e.attribute("v") == "point") light->type = LightType::LIGHT_POINT;
           else if (e.attribute("v") == "spot") light->type = LightType::LIGHT_SPOT;
           else if (e.attribute("v") == "area") light->type = LightType::LIGHT_AREA;
           else {
               std::cout << ERROR_AT(e) << "unknown light type " << e.attribute("v").toStdString() << std::endl;
               return false;
           }
       } else if (e.tagName() == "color") {
           if (!parseColor(e, light->color)) {
               PARSE_ERROR(e);
               return false;
           }
       } else if (e.tagName() == "function") {
           if (!parseTriple(e, light->function.x, light->function.y, light->function.z, "a", "b", "c") &&
               !parseTriple(e, light->function.x, light->function.y, light->function.z, "x", "y", "z") &&
               !parseTriple(e, light->function.x, light->function.y, light->function.z, "v1", "v2", "v3")) {
               PARSE_ERROR(e);
               return false;
           }
       } else if (e.tagName() == "position") {
           if (light->type == LightType::LIGHT_DIRECTIONAL) {
               std::cout << ERROR_AT(e) << "position is not applicable to directional lights" << std::endl;
               return false;
           }
           if (!parseTriple(e, light->pos.x, light->pos.y, light->pos.z, "x", "y", "z")) {
               PARSE_ERROR(e);
               return false;
           }
       } else if (e.tagName() == "direction") {
           if (light->type == LightType::LIGHT_POINT) {
               std::cout << ERROR_AT(e) << "direction is not applicable to point lights" << std::endl;
               return false;
           }
           if (!parseTriple(e, light->dir.x, light->dir.y, light->dir.z, "x", "y", "z")) {
               PARSE_ERROR(e);
               return false;
           }
       } else if (e.tagName() == "penumbra") {
           if (light->type != LightType::LIGHT_SPOT) {
               std::cout << ERROR_AT(e) << "penumbra is only applicable to spot lights" << std::endl;
               return false;
           }
           float penumbra = 0.f;
           if (!parseSingle(e, penumbra, "v")) {
               PARSE_ERROR(e);
               return false;
           }

           light->penumbra = penumbra * M_PI / 180.f;
       } else if (e.tagName() == "angle") {
           if (light->type != LightType::LIGHT_SPOT) {
               std::cout << ERROR_AT(e) << "angle is only applicable to spot lights" << std::endl;
               return false;
           }

           float angle = 0.f;
           if (!parseSingle(e, angle, "v")) {
               PARSE_ERROR(e);
               return false;
           }
           light->angle = angle * M_PI / 180.f;
       } else if (e.tagName() == "width") {
           if (light->type != LightType::LIGHT_AREA) {
               std::cout << ERROR_AT(e) << "width is only applicable to area lights" << std::endl;
               return false;
           }
           if (!parseSingle(e, light->width, "v")) {
               PARSE_ERROR(e);
               return false;
           }
       } else if (e.tagName() == "height") {
           if (light->type != LightType::LIGHT_AREA) {
               std::cout << ERROR_AT(e) << "height is only applicable to area lights" << std::endl;
               return false;
           }
           if (!parseSingle(e, light->height, "v")) {
               PARSE_ERROR(e);
               return false;
           }
       } else if (!e.isNull()) {
           UNSUPPORTED_ELEMENT(e);
           return false;
       }
       childNode = childNode.nextSibling();
   }

   return true;
}

/**
* Parse cameraData and fill in m_cameraData.
*/
bool ScenefileReader::parseCameraData(const QJsonObject &cameradata) {
   QStringList requiredFields = {"position", "up", "heightAngle"};
   QStringList optionalFields = {"aperture", "focalLength", "look", "focus"};
   QStringList allFields = requiredFields + optionalFields;
   for (auto field : cameradata.keys()) {
      if (!allFields.contains(field)) {
         std::cout << "unknown field \"" << field.toStdString() << "\" on cameraData object" << std::endl;
         return false;
      }
   }
    for (auto field : requiredFields) {
        if (!cameradata.contains(field)) {
            std::cout << "missing required field \"" << field.toStdString() << "\" on cameraData object" << std::endl;
            return false;
        }
    }

   // Must have either look or focus, but not both
   if (cameradata.contains("look") && cameradata.contains("focus")) {
      std::cout << "cameraData cannot contain both \"look\" and \"focus\"" << std::endl;
      return false;
   }

   // Parse the camera data
   if (cameradata["position"].isArray()) {
      QJsonArray position = cameradata["position"].toArray();
      if (position.size() != 3) {
         std::cout << "cameraData position must have 3 elements" << std::endl;
         return false;
      }
      if (!position[0].isDouble() || !position[1].isDouble() || !position[2].isDouble()) {
         std::cout << "cameraData position must be a floating-point value" << std::endl;
         return false;
      }
      m_cameraData.pos.x = position[0].toDouble();
      m_cameraData.pos.y = position[1].toDouble();
      m_cameraData.pos.z = position[2].toDouble();
   } else {
      std::cout << "cameraData position must be an array" << std::endl;
      return false;
   }

   if (cameradata["up"].isArray()) {
      QJsonArray up = cameradata["up"].toArray();
      if (up.size() != 3) {
         std::cout << "cameraData up must have 3 elements" << std::endl;
         return false;
      }
      if (!up[0].isDouble() || !up[1].isDouble() || !up[2].isDouble()) {
         std::cout << "cameraData up must be a floating-point value" << std::endl;
         return false;
      }
      m_cameraData.up.x = up[0].toDouble();
      m_cameraData.up.y = up[1].toDouble();
      m_cameraData.up.z = up[2].toDouble();
   } else {
      std::cout << "cameraData up must be an array" << std::endl;
      return false;
   }

    if (cameradata["heightAngle"].isDouble()) {
        m_cameraData.heightAngle = cameradata["heightAngle"].toDouble() * M_PI / 180.f;
    } else {
        std::cout << "cameraData heightAngle must be a floating-point value" << std::endl;
        return false;
    }

    if (cameradata.contains("aperture")) {
        if (cameradata["aperture"].isDouble()) {
            m_cameraData.aperture = cameradata["aperture"].toDouble();
        } else {
            std::cout << "cameraData aperture must be a floating-point value" << std::endl;
            return false;
        }
    }

    if (cameradata.contains("focalLength")) {
        if (cameradata["focalLength"].isDouble()) {
            m_cameraData.focalLength = cameradata["focalLength"].toDouble();
        } else {
            std::cout << "cameraData focalLength must be a floating-point value" << std::endl;
            return false;
        }
    }

    // Parse the look or focus
    // if the focus is specified, we will convert it to a look vector later
    if (cameradata.contains("look")) {
        if (cameradata["look"].isArray()) {
            QJsonArray look = cameradata["look"].toArray();
            if (look.size() != 3) {
                std::cout << "cameraData look must have 3 elements" << std::endl;
                return false;
            }
            if (!look[0].isDouble() || !look[1].isDouble() || !look[2].isDouble()) {
                std::cout << "cameraData look must be a floating-point value" << std::endl;
                return false;
            }
            m_cameraData.look.x = look[0].toDouble();
            m_cameraData.look.y = look[1].toDouble();
            m_cameraData.look.z = look[2].toDouble();
        } else {
            std::cout << "cameraData look must be an array" << std::endl;
            return false;
        }
    } else if (cameradata.contains("focus")) {
        if (cameradata["focus"].isArray()) {
            QJsonArray focus = cameradata["focus"].toArray();
            if (focus.size() != 3) {
                std::cout << "cameraData focus must have 3 elements" << std::endl;
                return false;
            }
            if (!focus[0].isDouble() || !focus[1].isDouble() || !focus[2].isDouble()) {
                std::cout << "cameraData focus must be a floating-point value" << std::endl;
                return false;
            }
            m_cameraData.look.x = focus[0].toDouble();
            m_cameraData.look.y = focus[1].toDouble();
            m_cameraData.look.z = focus[2].toDouble();
        } else {
            std::cout << "cameraData focus must be an array" << std::endl;
            return false;
        }
    }
    // Convert the focus point (stored in the look vector) into a
    // look vector from the camera position to that focus point.
    if (cameradata.contains("focus")) {
        m_cameraData.look -= m_cameraData.pos;
    }

   return true;
}

/**
* Parse a group object and create a new CS123SceneNode in m_nodes.
*/
bool ScenefileReader::parseGroupData(const QJsonObject &object) {
   QStringList optionalFields = {"name", "translate", "rotate", "scale", "matrix", "lights", "primitives", "children"};
   QStringList allFields = optionalFields;
   for (auto field : object.keys()) {
      if (!allFields.contains(field)) {
         std::cout << "unknown field \"" << field.toStdString() << "\" on group object" << std::endl;
         return false;
      }
   }
   
   SceneNode *node = new SceneNode;
   m_nodes.push_back(node);

   if (!object.hasAttribute("name")) {
       PARSE_ERROR(object);
       return false;
   }

   if (object.attribute("type") != "tree") {
       std::cout << "top-level <object> elements must be of type tree" << std::endl;
       return false;
   }

   std::string name = object.attribute("name").toStdString();

   // Check that this object does not exist
   if (m_objects[name]) {
       std::cout << ERROR_AT(object) << "two objects with the same name: " << name << std::endl;
       return false;
   }

   // Create the object and add to the map
   SceneNode *node = new SceneNode;
   m_nodes.push_back(node);
   m_objects[name] = node;

   // Iterate over child elements
   QDomNode childNode = object.firstChild();
   while (!childNode.isNull()) {
       QDomElement e = childNode.toElement();
       if (e.tagName() == "transblock") {
           SceneNode *child = new SceneNode;
           m_nodes.push_back(child);
           if (!parseTransBlock(e, child)) {
               PARSE_ERROR(e);
               return false;
           }
           node->children.push_back(child);
       } else if (!e.isNull()) {
           UNSUPPORTED_ELEMENT(e);
           return false;
       }
       childNode = childNode.nextSibling();
   }

   return true;
}

/**
* Parse a <transblock> tag into node, which consists of any number of
* <translate>, <rotate>, <scale>, or <matrix> elements followed by one
* <object> element.  That <object> element is either a master reference,
* a subtree, or a primitive.  If it's a master reference, we handle it
* here, otherwise we will call other methods.  Example <transblock>:
*
* <transblock>
*   <translate x="1" y="2" z="3"/>
*   <rotate x="0" y="1" z="0" a="90"/>
*   <scale x="1" y="2" z="1"/>
*   <object type="primitive" name="sphere"/>
* </transblock>
*/
bool ScenefileReader::parseTransBlock(const QDomElement &transblock, SceneNode* node) {
   // Iterate over child elements
   QDomNode childNode = transblock.firstChild();
   while (!childNode.isNull()) {
       QDomElement e = childNode.toElement();
       if (e.tagName() == "translate") {
           SceneTransformation *t = new SceneTransformation();
           node->transformations.push_back(t);
           t->type = TransformationType::TRANSFORMATION_TRANSLATE;

           if (!parseTriple(e, t->translate.x, t->translate.y, t->translate.z, "x", "y", "z")) {
               PARSE_ERROR(e);
               return false;
           }
       } else if (e.tagName() == "rotate") {
           SceneTransformation *t = new SceneTransformation();
           node->transformations.push_back(t);
           t->type = TransformationType::TRANSFORMATION_ROTATE;

           float angle;
           if (!parseQuadruple(e, t->rotate.x, t->rotate.y, t->rotate.z, angle, "x", "y", "z", "angle")) {
               PARSE_ERROR(e);
               return false;
           }

           // Convert to radians
           t->angle = angle * M_PI / 180;
       } else if (e.tagName() == "scale") {
           SceneTransformation *t = new SceneTransformation();
           node->transformations.push_back(t);
           t->type = TransformationType::TRANSFORMATION_SCALE;

           if (!parseTriple(e, t->scale.x, t->scale.y, t->scale.z, "x", "y", "z")) {
               PARSE_ERROR(e);
               return false;
           }
       } else if (e.tagName() == "matrix") {
           SceneTransformation* t = new SceneTransformation();
           node->transformations.push_back(t);
           t->type = TransformationType::TRANSFORMATION_MATRIX;

           if (!parseMatrix(e, t->matrix)) {
               PARSE_ERROR(e);
               return false;
           }
       } else if (e.tagName() == "object") {
           if (e.attribute("type") == "master") {
               std::string masterName = e.attribute("name").toStdString();
               if (!m_objects[masterName]) {
                   std::cout << ERROR_AT(e) << "invalid master object reference: " << masterName << std::endl;
                   return false;
               }
               node->children.push_back(m_objects[masterName]);
           } else if (e.attribute("type") == "tree") {
               QDomNode subNode = e.firstChild();
               while (!subNode.isNull()) {
                   QDomElement e = subNode.toElement();
                   if (e.tagName() == "transblock") {
                       SceneNode* n = new SceneNode;
                       m_nodes.push_back(n);
                       node->children.push_back(n);
                       if (!parseTransBlock(e, n)) {
                           PARSE_ERROR(e);
                           return false;
                       }
                   } else if (!e.isNull()) {
                       UNSUPPORTED_ELEMENT(e);
                       return false;
                   }
                   subNode = subNode.nextSibling();
               }
           } else if (e.attribute("type") == "primitive") {
               if (!parsePrimitive(e, node)) {
                   PARSE_ERROR(e);
                   return false;
               }
           } else {
               std::cout << ERROR_AT(e) << "invalid object type: " << e.attribute("type").toStdString() << std::endl;
               return false;
           }
       } else if (!e.isNull()) {
           UNSUPPORTED_ELEMENT(e);
           return false;
       }
       childNode = childNode.nextSibling();
   }

   return true;
}

/**
* Parse an <object type="primitive"> tag into node.
*/
bool ScenefileReader::parsePrimitive(const QDomElement &prim, SceneNode* node) {
   // Default primitive
   ScenePrimitive* primitive = new ScenePrimitive();
   SceneMaterial& mat = primitive->material;
   mat.clear();
   primitive->type = PrimitiveType::PRIMITIVE_CUBE;
   mat.textureMap.isUsed = false;
   mat.bumpMap.isUsed = false;
   mat.cDiffuse.r = mat.cDiffuse.g = mat.cDiffuse.b = 1;
   node->primitives.push_back(primitive);

   std::filesystem::path basepath = std::filesystem::path(file_name).parent_path().parent_path();

   // Parse primitive type
   std::string primType = prim.attribute("name").toStdString();
   if (primType == "sphere") primitive->type = PrimitiveType::PRIMITIVE_SPHERE;
   else if (primType == "cube") primitive->type = PrimitiveType::PRIMITIVE_CUBE;
   else if (primType == "cylinder") primitive->type = PrimitiveType::PRIMITIVE_CYLINDER;
   else if (primType == "cone") primitive->type = PrimitiveType::PRIMITIVE_CONE;
   else if (primType == "torus") primitive->type = PrimitiveType::PRIMITIVE_TORUS;
   else if (primType == "mesh") {
       primitive->type = PrimitiveType::PRIMITIVE_MESH;
       if (prim.hasAttribute("meshfile")) {
           std::filesystem::path relativePath(prim.attribute("meshfile").toStdString());
           primitive->meshfile = (basepath / relativePath).string();
       } else if (prim.hasAttribute("filename")) {
           std::filesystem::path relativePath(prim.attribute("filename").toStdString());
           primitive->meshfile = (basepath / relativePath).string();
       } else {
           std::cout << "mesh object must specify filename" << std::endl;
           return false;
       }
   }

   // Iterate over child elements
   QDomNode childNode = prim.firstChild();
   while (!childNode.isNull()) {
       QDomElement e = childNode.toElement();
       if (e.tagName() == "diffuse") {
           if (!parseColor(e, mat.cDiffuse)) {
               PARSE_ERROR(e);
               return false;
           }
       } else if (e.tagName() == "ambient") {
           if (!parseColor(e, mat.cAmbient)) {
               PARSE_ERROR(e);
               return false;
           }
       } else if (e.tagName() == "reflective") {
           if (!parseColor(e, mat.cReflective)) {
               PARSE_ERROR(e);
               return false;
           }
       } else if (e.tagName() == "specular") {
           if (!parseColor(e, mat.cSpecular)) {
               PARSE_ERROR(e);
               return false;
           }
       } else if (e.tagName() == "emissive") {
           if (!parseColor(e, mat.cEmissive)) {
               PARSE_ERROR(e);
               return false;
           }
       } else if (e.tagName() == "transparent") {
           if (!parseColor(e, mat.cTransparent)) {
               PARSE_ERROR(e);
               return false;
           }
       } else if (e.tagName() == "shininess") {
           if (!parseSingle(e, mat.shininess, "v")) {
               PARSE_ERROR(e);
               return false;
           }
       } else if (e.tagName() == "ior") {
           if (!parseSingle(e, mat.ior, "v")) {
               PARSE_ERROR(e);
               return false;
           }
       } else if (e.tagName() == "texture") {
           if (!parseMap(e, mat.textureMap, basepath)) {
               PARSE_ERROR(e);
               return false;
           }
       } else if (e.tagName() == "bumpmap") {
           if (!parseMap(e, mat.bumpMap, basepath)) {
               PARSE_ERROR(e);
               return false;
           }
       } else if (e.tagName() == "blend") {
           if (!parseSingle(e, mat.blend, "v")) {
               PARSE_ERROR(e);
               return false;
           }
       } else {
           UNSUPPORTED_ELEMENT(e);
           return false;
       }
       childNode = childNode.nextSibling();
   }

   return true;
}
