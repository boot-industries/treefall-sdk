/*
  ==============================================================================

    OrbVisualizer.cpp
    Created: Shmui-to-JUCE Audio Visualization Port

    Implementation of the 3D orb visualization with OpenGL shaders.

  ==============================================================================
*/

#include "OrbVisualizer.h"
#include "../Utils/ColorUtils.h"

namespace shmui
{

// Embedded shader source (loaded from files at compile time)
static const char* vertexShaderSource = R"(
attribute vec4 aPosition;
attribute vec2 aTexCoord;
varying vec2 vUv;
void main()
{
    vUv = aTexCoord;
    gl_Position = aPosition;
}
)";

static const char* fragmentShaderSource = R"(
#ifdef GL_ES
precision highp float;
#endif

uniform float uTime;
uniform float uAnimation;
uniform float uInverted;
uniform float uOffsets[7];
uniform vec3 uColor1;
uniform vec3 uColor2;
uniform float uInputVolume;
uniform float uOutputVolume;
uniform float uOpacity;
uniform sampler2D uPerlinTexture;

varying vec2 vUv;

const float PI = 3.14159265358979323846;

bool drawOval(vec2 polarUv, vec2 polarCenter, float a, float b, bool reverseGradient, float softness, out vec4 color) {
    vec2 p = polarUv - polarCenter;
    float oval = (p.x * p.x) / (a * a) + (p.y * p.y) / (b * b);
    float edge = smoothstep(1.0, 1.0 - softness, oval);
    if (edge > 0.0) {
        float gradient = reverseGradient ? (1.0 - (p.x / a + 1.0) / 2.0) : ((p.x / a + 1.0) / 2.0);
        gradient = mix(0.5, gradient, 0.1);
        color = vec4(vec3(gradient), 0.85 * edge);
        return true;
    }
    return false;
}

vec3 colorRamp(float grayscale, vec3 color1, vec3 color2, vec3 color3, vec3 color4) {
    if (grayscale < 0.33) {
        return mix(color1, color2, grayscale * 3.0);
    } else if (grayscale < 0.66) {
        return mix(color2, color3, (grayscale - 0.33) * 3.0);
    } else {
        return mix(color3, color4, (grayscale - 0.66) * 3.0);
    }
}

vec2 hash2(vec2 p) {
    return fract(sin(vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)))) * 43758.5453);
}

float noise2D(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    float n = mix(
        mix(dot(hash2(i + vec2(0.0, 0.0)), f - vec2(0.0, 0.0)),
            dot(hash2(i + vec2(1.0, 0.0)), f - vec2(1.0, 0.0)), u.x),
        mix(dot(hash2(i + vec2(0.0, 1.0)), f - vec2(0.0, 1.0)),
            dot(hash2(i + vec2(1.0, 1.0)), f - vec2(1.0, 1.0)), u.x),
        u.y
    );
    return 0.5 + 0.5 * n;
}

float sharpRing(vec3 decomposed, float time) {
    float ringStart = 1.0;
    float ringWidth = 0.3;
    float noiseScale = 5.0;
    float noise = mix(
        noise2D(vec2(decomposed.x, time) * noiseScale),
        noise2D(vec2(decomposed.y, time) * noiseScale),
        decomposed.z
    );
    noise = (noise - 0.5) * 2.5;
    return ringStart + noise * ringWidth * 1.5;
}

float smoothRing(vec3 decomposed, float time) {
    float ringStart = 0.9;
    float ringWidth = 0.2;
    float noiseScale = 6.0;
    float noise = mix(
        noise2D(vec2(decomposed.x, time) * noiseScale),
        noise2D(vec2(decomposed.y, time) * noiseScale),
        decomposed.z
    );
    noise = (noise - 0.5) * 5.0;
    return ringStart + noise * ringWidth;
}

float flow(vec3 decomposed, float time) {
    return mix(
        texture2D(uPerlinTexture, vec2(time, decomposed.x / 2.0)).r,
        texture2D(uPerlinTexture, vec2(time, decomposed.y / 2.0)).r,
        decomposed.z
    );
}

void main() {
    vec2 uv = vUv * 2.0 - 1.0;
    float radius = length(uv);
    float theta = atan(uv.y, uv.x);
    if (theta < 0.0) theta += 2.0 * PI;

    vec3 decomposed = vec3(
        theta / (2.0 * PI),
        mod(theta / (2.0 * PI) + 0.5, 1.0) + 1.0,
        abs(theta / PI - 1.0)
    );

    float noise = flow(decomposed, radius * 0.03 - uAnimation * 0.2) - 0.5;
    theta += noise * mix(0.08, 0.25, uOutputVolume);

    vec4 color = vec4(1.0, 1.0, 1.0, 1.0);

    float originalCenters[7];
    originalCenters[0] = 0.0;
    originalCenters[1] = 0.5 * PI;
    originalCenters[2] = 1.0 * PI;
    originalCenters[3] = 1.5 * PI;
    originalCenters[4] = 2.0 * PI;
    originalCenters[5] = 2.5 * PI;
    originalCenters[6] = 3.0 * PI;

    float centers[7];
    for (int i = 0; i < 7; i++) {
        centers[i] = originalCenters[i] + 0.5 * sin(uTime / 20.0 + uOffsets[i]);
    }

    float a, b;
    vec4 ovalColor;

    for (int i = 0; i < 7; i++) {
        float noiseVal = texture2D(uPerlinTexture, vec2(mod(centers[i] + uTime * 0.05, 1.0), 0.5)).r;
        a = 0.5 + noiseVal * 0.3;
        b = noiseVal * mix(3.5, 2.5, uInputVolume);
        bool reverseGradient = (mod(float(i), 2.0) == 1.0);

        float distTheta = min(
            abs(theta - centers[i]),
            min(
                abs(theta + 2.0 * PI - centers[i]),
                abs(theta - 2.0 * PI - centers[i])
            )
        );
        float distRadius = radius;
        float softness = 0.6;

        if (drawOval(vec2(distTheta, distRadius), vec2(0.0, 0.0), a, b, reverseGradient, softness, ovalColor)) {
            color.rgb = mix(color.rgb, ovalColor.rgb, ovalColor.a);
            color.a = max(color.a, ovalColor.a);
        }
    }

    float ringRadius1 = sharpRing(decomposed, uTime * 0.1);
    float ringRadius2 = smoothRing(decomposed, uTime * 0.1);

    float inputRadius1 = radius + uInputVolume * 0.2;
    float inputRadius2 = radius + uInputVolume * 0.15;
    float opacity1 = mix(0.2, 0.6, uInputVolume);
    float opacity2 = mix(0.15, 0.45, uInputVolume);

    float ringAlpha1 = (inputRadius2 >= ringRadius1) ? opacity1 : 0.0;
    float ringAlpha2 = smoothstep(ringRadius2 - 0.05, ringRadius2 + 0.05, inputRadius1) * opacity2;
    float totalRingAlpha = max(ringAlpha1, ringAlpha2);

    vec3 ringColor = vec3(1.0);
    color.rgb = 1.0 - (1.0 - color.rgb) * (1.0 - ringColor * totalRingAlpha);

    vec3 c1 = vec3(0.0, 0.0, 0.0);
    vec3 c2 = uColor1;
    vec3 c3 = uColor2;
    vec3 c4 = vec3(1.0, 1.0, 1.0);

    float luminance = mix(color.r, 1.0 - color.r, uInverted);
    color.rgb = colorRamp(luminance, c1, c2, c3, c4);
    color.a *= uOpacity;

    gl_FragColor = color;
}
)";

//==============================================================================

OrbVisualizer::OrbVisualizer()
{
    // Initialize random offsets
    Interpolation::SeedRandom rng(seed);
    for (size_t i = 0; i < offsets.size(); ++i)
    {
        offsets[i] = rng.next() * juce::MathConstants<float>::twoPi;
    }

    // Setup OpenGL
    openGLContext.setRenderer(this);
    openGLContext.attachTo(*this);

    setOpaque(false);
    startTimerHz(60);
}

OrbVisualizer::~OrbVisualizer()
{
    stopTimer();
    openGLContext.detach();
}

void OrbVisualizer::setAgentState(AgentState state)
{
    agentState = state;
}

void OrbVisualizer::setVolumeMode(OrbVolumeMode mode)
{
    volumeMode = mode;
}

void OrbVisualizer::setInputVolume(float volume)
{
    manualInput = Interpolation::clamp01(volume);
}

void OrbVisualizer::setOutputVolume(float volume)
{
    manualOutput = Interpolation::clamp01(volume);
}

void OrbVisualizer::setColors(const juce::Colour& c1, const juce::Colour& c2)
{
    color1 = c1;
    color2 = c2;
    targetColor1 = c1;
    targetColor2 = c2;
}

void OrbVisualizer::setSeed(uint32_t newSeed)
{
    seed = newSeed;

    Interpolation::SeedRandom rng(seed);
    for (size_t i = 0; i < offsets.size(); ++i)
    {
        offsets[i] = rng.next() * juce::MathConstants<float>::twoPi;
    }
}

void OrbVisualizer::setInverted(bool inv)
{
    inverted = inv;
}

void OrbVisualizer::newOpenGLContextCreated()
{
    createShaders();
    createNoiseTexture();

    // Create vertex buffers for fullscreen quad
    const GLfloat vertices[] = {
        -1.0f, -1.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 0.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f,  1.0f, 0.0f, 1.0f
    };

    const GLfloat texCoords[] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f
    };

    openGLContext.extensions.glGenBuffers(1, &vertexBuffer);
    openGLContext.extensions.glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    openGLContext.extensions.glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    openGLContext.extensions.glGenBuffers(1, &texCoordBuffer);
    openGLContext.extensions.glBindBuffer(GL_ARRAY_BUFFER, texCoordBuffer);
    openGLContext.extensions.glBufferData(GL_ARRAY_BUFFER, sizeof(texCoords), texCoords, GL_STATIC_DRAW);
}

void OrbVisualizer::renderOpenGL()
{
    if (shader == nullptr)
        return;

    juce::OpenGLHelpers::clear(juce::Colours::transparentBlack);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    shader->use();

    // Set uniforms
    shader->setUniform("uTime", time);
    shader->setUniform("uAnimation", animationTime);
    shader->setUniform("uInverted", inverted ? 1.0f : 0.0f);
    shader->setUniform("uInputVolume", smoothedInput);
    shader->setUniform("uOutputVolume", smoothedOutput);
    shader->setUniform("uOpacity", opacity);

    // Set colors
    shader->setUniform("uColor1",
                       currentColor1.getFloatRed(),
                       currentColor1.getFloatGreen(),
                       currentColor1.getFloatBlue());
    shader->setUniform("uColor2",
                       currentColor2.getFloatRed(),
                       currentColor2.getFloatGreen(),
                       currentColor2.getFloatBlue());

    // Set offsets array - use glGetUniformLocation directly for array uniforms
    GLint offsetsLocation = openGLContext.extensions.glGetUniformLocation(shader->getProgramID(), "uOffsets");
    if (offsetsLocation >= 0)
    {
        glUniform1fv(offsetsLocation, 7, offsets.data());
    }

    // Bind texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, noiseTexture);
    shader->setUniform("uPerlinTexture", 0);

    // Draw quad
    openGLContext.extensions.glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    if (auto* posAttr = shader->getAttributeIDFromName("aPosition"))
    {
        openGLContext.extensions.glVertexAttribPointer(posAttr->attributeID, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
        openGLContext.extensions.glEnableVertexAttribArray(posAttr->attributeID);
    }

    openGLContext.extensions.glBindBuffer(GL_ARRAY_BUFFER, texCoordBuffer);
    if (auto* texAttr = shader->getAttributeIDFromName("aTexCoord"))
    {
        openGLContext.extensions.glVertexAttribPointer(texAttr->attributeID, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        openGLContext.extensions.glEnableVertexAttribArray(texAttr->attributeID);
    }

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void OrbVisualizer::openGLContextClosing()
{
    shader.reset();

    if (noiseTexture != 0)
    {
        glDeleteTextures(1, &noiseTexture);
        noiseTexture = 0;
    }

    if (vertexBuffer != 0)
    {
        openGLContext.extensions.glDeleteBuffers(1, &vertexBuffer);
        vertexBuffer = 0;
    }

    if (texCoordBuffer != 0)
    {
        openGLContext.extensions.glDeleteBuffers(1, &texCoordBuffer);
        texCoordBuffer = 0;
    }
}

void OrbVisualizer::paint(juce::Graphics&)
{
    // OpenGL handles rendering
}

void OrbVisualizer::resized()
{
    // OpenGL context will handle viewport
}

void OrbVisualizer::timerCallback()
{
    const float deltaTime = 1.0f / 60.0f;

    // Update time
    time += deltaTime * 0.5f;

    // Update opacity fade-in
    if (opacity < 1.0f)
    {
        opacity = std::min(1.0f, opacity + deltaTime * 2.0f);
    }

    // Update animation targets based on state
    updateAnimationTargets();

    // Smooth volume
    smoothedInput += (targetInput - smoothedInput) * kSmoothingFactor;
    smoothedOutput += (targetOutput - smoothedOutput) * kSmoothingFactor;

    // Update animation speed
    const float targetSpeed = 0.1f + (1.0f - std::pow(smoothedOutput - 1.0f, 2.0f)) * 0.9f;
    animationSpeed += (targetSpeed - animationSpeed) * 0.12f;
    animationTime += deltaTime * animationSpeed;

    // Lerp colors
    currentColor1 = ColorUtils::lerpColour(currentColor1, targetColor1, kColorLerpFactor);
    currentColor2 = ColorUtils::lerpColour(currentColor2, targetColor2, kColorLerpFactor);

    openGLContext.triggerRepaint();
}

void OrbVisualizer::updateAnimationTargets()
{
    if (volumeMode == OrbVolumeMode::Manual)
    {
        targetInput = manualInput;
        targetOutput = manualOutput;
        return;
    }

    // Auto mode based on agent state (from orb.tsx)
    const float t = time * 2.0f;

    switch (agentState)
    {
        case AgentState::Idle:
            targetInput = 0.0f;
            targetOutput = 0.3f;
            break;

        case AgentState::Listening:
            targetInput = Interpolation::clamp01(0.55f + std::sin(t * 3.2f) * 0.35f);
            targetOutput = 0.45f;
            break;

        case AgentState::Speaking:  // "talking" in React orb.tsx
            targetInput = Interpolation::clamp01(0.65f + std::sin(t * 4.8f) * 0.22f);
            targetOutput = Interpolation::clamp01(0.75f + std::sin(t * 3.6f) * 0.22f);
            break;

        case AgentState::Thinking:
        {
            const float base = 0.38f + 0.07f * std::sin(t * 0.7f);
            const float wander = 0.05f * std::sin(t * 2.1f) * std::sin(t * 0.37f + 1.2f);
            targetInput = Interpolation::clamp01(base + wander);
            targetOutput = Interpolation::clamp01(0.48f + 0.12f * std::sin(t * 1.05f + 0.6f));
            break;
        }

        case AgentState::Connecting:
        case AgentState::Initializing:
            // Treat connecting/initializing like thinking
            targetInput = Interpolation::clamp01(0.38f + 0.07f * std::sin(t * 0.7f));
            targetOutput = Interpolation::clamp01(0.48f + 0.12f * std::sin(t * 1.05f + 0.6f));
            break;
    }
}

void OrbVisualizer::createShaders()
{
    shader = std::make_unique<juce::OpenGLShaderProgram>(openGLContext);

    if (!shader->addVertexShader(vertexShaderSource))
    {
        DBG("Vertex shader compile error: " + shader->getLastError());
        shader.reset();
        return;
    }

    if (!shader->addFragmentShader(fragmentShaderSource))
    {
        DBG("Fragment shader compile error: " + shader->getLastError());
        shader.reset();
        return;
    }

    if (!shader->link())
    {
        DBG("Shader link error: " + shader->getLastError());
        shader.reset();
        return;
    }
}

void OrbVisualizer::createNoiseTexture()
{
    // Generate a simple Perlin-like noise texture
    const int size = 256;
    std::vector<uint8_t> data(size * size);

    Interpolation::SeedRandom rng(12345);

    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            // Simple value noise
            const float fx = static_cast<float>(x) / size;
            const float fy = static_cast<float>(y) / size;

            float value = 0.0f;
            float amplitude = 1.0f;
            float frequency = 4.0f;

            for (int octave = 0; octave < 4; ++octave)
            {
                const int ix = static_cast<int>(fx * frequency) % size;
                const int iy = static_cast<int>(fy * frequency) % size;

                // Use seeded random for deterministic noise
                const float noise = Interpolation::seededRandom(
                    static_cast<float>(ix * 1000 + iy + octave * 10000));

                value += noise * amplitude;
                amplitude *= 0.5f;
                frequency *= 2.0f;
            }

            data[y * size + x] = static_cast<uint8_t>(juce::jlimit(0.0f, 255.0f, value * 128.0f));
        }
    }

    glGenTextures(1, &noiseTexture);
    glBindTexture(GL_TEXTURE_2D, noiseTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, size, size, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, data.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

} // namespace shmui
