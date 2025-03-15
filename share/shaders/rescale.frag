uniform sampler2D tex; // The input texture
uniform vec2 texelCount; // Number of texels in the input texture (width, height)
uniform vec2 pixelCount; // Number of output pixels (width, height)
uniform vec2 texelSize;  // = 1.0 / texelCount
uniform vec2 pixelSize;  // = 1.0 / pixelCount
uniform vec2 halfTexelSize; // = 0.5 * texelSize
uniform vec2 halfPixelSize; // = 0.5 * pixelSize

varying vec2 texCoord; // The texture coordinate for the current fragment

void main() {
	// Start and end of the current pixel in normalized coordinates (0..1)
	vec2 pixelStart = texCoord - halfPixelSize;
	vec2 pixelEnd   = texCoord + halfPixelSize;

	// Start and end texel indices in scaled coordinates (0..texelCount)
	ivec2 intTexelStart = ivec2(floor(pixelStart * texelCount));
	ivec2 intTexelEnd   = ivec2(ceil (pixelEnd   * texelCount));

	// Iterate over the contributing texels
	vec4 color = vec4(0.0);
	for (int y = intTexelStart.y; y < intTexelEnd.y; ++y) {
		for (int x = intTexelStart.x; x < intTexelEnd.x; ++x) {
			// Compute the texel position
			vec2 texelStart = vec2(x, y) * texelSize;
			vec2 texelEnd = texelStart + texelSize;

			// Compute the overlap between the pixel and texel sub-intervals
			vec2 overlapStart = max(pixelStart, texelStart);
			vec2 overlapEnd   = min(pixelEnd,   texelEnd  );
			vec2 overlapLength = overlapEnd - overlapStart;

			// Accumulate the color based on the overlap area
			vec4 texelColor = texture2D(tex, texelStart + halfTexelSize);
			color += texelColor * overlapLength.x * overlapLength.y;
		}
	}

	// Normalize
	gl_FragColor = color * pixelCount.x * pixelCount.y;
}
