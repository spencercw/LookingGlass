#version 460

//todo common
const uint COLOR_SPACE_SRGB = 0;
const uint COLOR_SPACE_EXTENDED_SRGB_LINEAR = 1;
const uint COLOR_SPACE_HDR10_ST2084 = 2;

layout (constant_id = 0) const uint colorSpace = COLOR_SPACE_SRGB;

layout (input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput framebuffer;
layout (input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput imGui;

layout (location = 0) out vec4 color;

float srgbToLinear(float value)
{
  const float U = 0.04045;
  const float A = 12.92;
  const float C = 0.055;
  const float G = 2.4;

  if (value <= U)
  {
    return value / A;
  }
  else
  {
    return pow((value + C) / (1.0 + C), G);
  }
}

vec3 srgbToLinear(vec3 value)
{
  return vec3(
    srgbToLinear(value.r),
    srgbToLinear(value.g),
    srgbToLinear(value.b));
}

float linearToSrgb(float value)
{
  const float V = 0.0031308;
  const float A = 12.92;
  const float C = 0.055;
  const float G_INV = 1.0 / 2.4;

  if (value <= V)
  {
    return A * value;
  }
  else
  {
    return (1.0 + C) * pow(value, G_INV) - C;
  }
}

vec3 linearToSrgb(vec3 value)
{
  return vec3(
    linearToSrgb(value.r),
    linearToSrgb(value.g),
    linearToSrgb(value.b));
}

vec3 st2084ToLinear(vec3 value)
{
  // SMPTE ST 2084 perceptual quantizer (PQ) EOTF
  const float M1_INV = 8192.0 / 1305.0;
  const float M2_INV = 32.0 / 2523.0;
  const float C1 = 107.0 / 128.0;
  const float C2 = 2413.0 / 128.0;
  const float C3 = 2392.0 / 128.0;

  vec3 num = max(pow(value, vec3(M2_INV)) - C1, 0.0);
  vec3 den = C2 - C3 * pow(value, vec3(M2_INV));
  vec3 l = pow(num / den, vec3(M1_INV));
  vec3 c = 10000.0 * l;

  return c;
}

vec3 linearToSt2084(vec3 value)
{
  // SMPTE ST 2084 perceptual quantizer (PQ) inverse EOTF
  const float M1 = 1305.0 / 8192.0;
  const float M2 = 2523.0 / 32.0;
  const float C1 = 107.0 / 128.0;
  const float C2 = 2413.0 / 128.0;
  const float C3 = 2392.0 / 128.0;

  vec3 l = value / 10000.0;
  vec3 lM1 = pow(l, vec3(M1));
  vec3 num = C1 + C2 * lM1;
  vec3 den = 1.0 + C3 * lM1;
  vec3 n = pow(num / den, vec3(M2));

  return n;
}

vec3 bt709ToBt2020(vec3 value)
{
  // Matrix values from BT.2087-0
  const mat3 BT709_TO_BT2020 = mat3(
    0.6274, 0.0691, 0.0164,
    0.3293, 0.9195, 0.0880,
    0.0433, 0.0114, 0.8956
  );
  return BT709_TO_BT2020 * value;
}

vec3 bt2020ToBt709(vec3 value)
{
  // Matrix values from BT.2407-0
  const mat3 BT2020_TO_BT709 = mat3(
    1.6605, -0.1246, -0.0182,
    -0.5876, 1.1329, -0.1006,
    -0.0728, -0.0083, 1.1187
  );
  return BT2020_TO_BT709 * value;
}

void main()
{
  // TODO: Get this from.. somewhere
  const float WHITE_LEVEL_NITS = 80.0;

  //// This works for SDR

  /*vec4 fbPx = subpassLoad(framebuffer);

  // sRGB non-linear
  vec4 imGuiPx = subpassLoad(imGui);

  // Pre-multiplied alpha
  vec3 blended = imGuiPx.rgb + fbPx.rgb * (1.0 - imGuiPx.a);

  color = vec4(blended, max(fbPx.a, imGuiPx.a));*/



  //// This looks wrong because we blend in linear space

  /*vec4 fbPx = subpassLoad(framebuffer);
  vec4 imGuiPx = subpassLoad(imGui);

  switch (colorSpace)
  {
    case COLOR_SPACE_HDR10_ST2084:
      fbPx.rgb = st2084ToLinear(fbPx.rgb);
      break;
  }

  switch (colorSpace)
  {
    case COLOR_SPACE_HDR10_ST2084:
      imGuiPx.rgb = bt709ToBt2020(srgbToLinear(imGuiPx.rgb)) * WHITE_LEVEL_NITS;
      break;
  }

  vec3 blended = imGuiPx.rgb + fbPx.rgb * (1.0 - imGuiPx.a);

  switch (colorSpace)
  {
    case COLOR_SPACE_HDR10_ST2084:
    {
      blended.rgb = linearToSt2084(blended.rgb);
      break;
    }
  }

  color = vec4(blended, max(fbPx.a, imGuiPx.a));*/


  //// This works pretty well


  /*vec4 imGuiPx = subpassLoad(imGui);
  if (imGuiPx.a == 0.0)
    discard;

  vec4 fbPx = subpassLoad(framebuffer);


  switch (colorSpace)
  {
    case COLOR_SPACE_HDR10_ST2084:
      fbPx.rgb = st2084ToLinear(fbPx.rgb) / WHITE_LEVEL_NITS;
      fbPx.rgb = linearToSrgb(clamp(fbPx.rgb, 0.0, 1.0));
      break;
  }

  switch (colorSpace)
  {
    case COLOR_SPACE_HDR10_ST2084:
      imGuiPx.rgb = linearToSrgb(bt709ToBt2020(srgbToLinear(imGuiPx.rgb)));
      break;
  }

  vec3 blended = imGuiPx.rgb + fbPx.rgb * (1.0 - imGuiPx.a);

  switch (colorSpace)
  {
    case COLOR_SPACE_HDR10_ST2084:
    {
      blended.rgb = linearToSt2084(srgbToLinear(blended.rgb) * WHITE_LEVEL_NITS);
      break;
    }
  }

  color = vec4(blended, max(fbPx.a, imGuiPx.a));*/



  //// This works even better


  /*vec4 imGuiPx = subpassLoad(imGui);
  if (imGuiPx.a == 0.0)
    discard;

  vec4 fbPx = subpassLoad(framebuffer);


  switch (colorSpace)
  {
    case COLOR_SPACE_HDR10_ST2084:
    fbPx.rgb = st2084ToLinear(fbPx.rgb);
    float maxNits = mix(10000.0, WHITE_LEVEL_NITS, imGuiPx.a);
    fbPx.rgb = clamp(fbPx.rgb, 0.0, maxNits);
    fbPx.rgb = fbPx.rgb / WHITE_LEVEL_NITS;
    fbPx.rgb = linearToSrgb(fbPx.rgb);
    break;
  }

  switch (colorSpace)
  {
    case COLOR_SPACE_HDR10_ST2084:
    imGuiPx.rgb = linearToSrgb(bt709ToBt2020(srgbToLinear(imGuiPx.rgb)));
    break;
  }

  vec3 blended = imGuiPx.rgb + fbPx.rgb * (1.0 - imGuiPx.a);

  switch (colorSpace)
  {
    case COLOR_SPACE_HDR10_ST2084:
    {
      blended.rgb = linearToSt2084(srgbToLinear(blended.rgb) * WHITE_LEVEL_NITS);
      break;
    }
  }

  color = vec4(blended, max(fbPx.a, imGuiPx.a));*/


  //todo investigate intermittent validation error. image layout is sometimes undefined instead of shader read only

  //// This looks perfect!



  /* We do blending here in non-linear sRGB space. This is not really 'correct'
   * (blending is typically done in linear space), but this is how the EGL
   * renderer does it, so we do the same here in order to keep the output
   * visually consistent, regardless of which renderer is in use, and whether we
   * are rendering SDR or HDR.
   */

  // If no overlay was rendered, leave the framebuffer unmodified
  vec4 imGuiPx = subpassLoad(imGui);
  if (imGuiPx.a == 0.0)
    discard;



  vec4 fbPx = subpassLoad(framebuffer);

  switch (colorSpace)
  {
    //todo other color spaces
    case COLOR_SPACE_HDR10_ST2084:
    fbPx.rgb = st2084ToLinear(fbPx.rgb);
    float maxNits = mix(10000.0, WHITE_LEVEL_NITS, pow(imGuiPx.a, 0.02));
    fbPx.rgb = clamp(fbPx.rgb, 0.0, maxNits);
    fbPx.rgb = fbPx.rgb / WHITE_LEVEL_NITS;
    fbPx.rgb = linearToSrgb(bt2020ToBt709(fbPx.rgb));
    break;
  }

  // The ImGui rendered output is pre-multiplied alpha, so the source color
  // blend factor here is 1.0 (i.e., omitted)
  vec3 blended = imGuiPx.rgb + fbPx.rgb * (1.0 - imGuiPx.a);

  switch (colorSpace)
  {
    //todo other color spaces
    case COLOR_SPACE_HDR10_ST2084:
    {
      blended.rgb = linearToSt2084(bt709ToBt2020(srgbToLinear(blended.rgb)) * WHITE_LEVEL_NITS);
      break;
    }
  }

  color = vec4(blended, max(fbPx.a, imGuiPx.a));
}
