#version 450

// vertex attrs
layout(location = 0) in vec2 xy;

// instanced attrs
layout(location = 1) in vec3 pos;
layout(location = 2) in vec3 rot;
layout(location = 3) in vec3 scale;

layout(binding = 0) uniform UBO1 {
    mat4 proj;
    mat4 view;
} ubo1;

layout(location = 0) out vec2 fragTexCoord;

// generate model matrix from position, rotation, and scale
mat4 generateModelMatrix(vec3 position, vec3 rotation, vec3 scale) {
    // Rotation matrices for each axis
    mat4 rotX = mat4(
        1.0, 0.0, 0.0, 0.0,
        0.0, cos(rotation.x), -sin(rotation.x), 0.0,
        0.0, sin(rotation.x), cos(rotation.x), 0.0,
        0.0, 0.0, 0.0, 1.0
    );
    mat4 rotY = mat4(
        cos(rotation.y), 0.0, sin(rotation.y), 0.0,
        0.0, 1.0, 0.0, 0.0,
        -sin(rotation.y), 0.0, cos(rotation.y), 0.0,
        0.0, 0.0, 0.0, 1.0
    );
    mat4 rotZ = mat4(
        cos(rotation.z), -sin(rotation.z), 0.0, 0.0,
        sin(rotation.z), cos(rotation.z), 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0
    );

    // Combine rotation matrices
    mat4 rotationMatrix = rotX * rotY * rotZ;

    // Scale matrix
    mat4 scaleMatrix = mat4(
        scale.x, 0.0, 0.0, 0.0,
        0.0, scale.y, 0.0, 0.0,
        0.0, 0.0, scale.z, 0.0,
        0.0, 0.0, 0.0, 1.0
    );

    // Translation matrix
    mat4 translationMatrix = mat4(
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        position.x, position.y, position.z, 1.0
    );

    // Combine translation, rotation, and scale
    mat4 modelMatrix = translationMatrix * rotationMatrix * scaleMatrix;

    return modelMatrix;
}

void main() {
    mat4 model = generateModelMatrix(pos, rot, scale);
    gl_Position = ubo1.proj * ubo1.view * model * vec4(-xy.x, xy.y, 0.0, 1.0);

    if (xy.x == 0.5 && xy.y == -0.5) {
        fragTexCoord = vec2(0, 0); // top-left
    }
    else if (xy.x == -0.5 && xy.y == -0.5) {
        fragTexCoord = vec2(1, 0); // top-right
    }
    else if (xy.x == -0.5 && xy.y == 0.5) {
        fragTexCoord = vec2(1, 1); // bottom-right
    }
    else if (xy.x == 0.5 && xy.y == 0.5) {
        fragTexCoord = vec2(0, 1); // bottom-left
    }
}