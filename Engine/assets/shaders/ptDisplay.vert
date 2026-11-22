#version 460

// Fullscreen triangle with no vertex buffer. Vertex IDs 0,1,2 map to clip-space
// (-1,-1), (3,-1), (-1,3): one oversized triangle that covers the whole viewport.
// Compared to a two-triangle quad there's no diagonal seam where fragments get shaded twice.
void main()
{
    vec2 uv     = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
}
