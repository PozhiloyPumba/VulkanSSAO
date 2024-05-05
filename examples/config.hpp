#ifndef __CONFIG_HPP__
#define __CONFIG_HPP__

#include <string>
#include <glm/glm.hpp>

static struct Config {
    std::string name;
    glm::vec3 position;
    glm::vec3 rotation;
} config[] = {
    {"models/sponza/sponza.gltf", { 1.0f, 0.75f, 0.0f }, {0.0f, 90.0f, 0.0f}},
    {"models/FlightHelmet/glTF/FlightHelmet.gltf", { 0.0f, -0.1f, -1.0f }, {0.0f, 0.0f, 0.0f}},
    {"models/voyager.gltf", { -3.92927f, 0.628469f, -7.34199f }, {-7.5f, -34.5f, 0.0f}},
    {"models/deer.gltf", { -2.46584f, -0.0757071f, -2.06703f }, {1.75f, -51.5f, 0.0f}},
    {"models/oaktree.gltf", { -0.755051f, 0.991626f, -1.88991f }, {-1.5f, -23.75f, 0.0f}},
    {"models/CesiumMan/glTF/CesiumMan.gltf", { -0.755051f, 0.991626f, -1.88991f }, {-1.5f, -23.75f, 0.0f}},
};

static Config conf = config[5];
#endif