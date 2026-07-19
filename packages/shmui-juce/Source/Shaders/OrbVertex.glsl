// OrbVertex.glsl - Vertex shader for orb visualization
// Ported from orb.tsx

attribute vec4 aPosition;
attribute vec2 aTexCoord;

varying vec2 vUv;

void main()
{
    vUv = aTexCoord;
    gl_Position = aPosition;
}
