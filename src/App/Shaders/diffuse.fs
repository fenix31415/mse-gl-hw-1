#version 330 core

in vec2 fragCoord;

out vec4 out_col;

out float gl_FragDepth;

uniform vec2 screen_pos;
uniform float screen_scale;
uniform int max_iterations;
uniform float border2;
uniform vec4 color_ranges;

int try_check()
{
  vec2 P = fragCoord * screen_scale + screen_pos;
  
  float x = P.x;
  float y = P.y;
  float x_old = x;
  float y_old = y;
  
  for (int ans = 0; ans < max_iterations; ++ans)
  {
    float x_ = x;
    x = (x * x - y * y) + x_old;
    y = (2.0 * x_ * y) + y_old;
    
    if (x * x + y * y > border2)
      return ans;
  }
  
  return max_iterations;
}

void main() {
	int iters = try_check();
  float iters_f = float(iters);
  float ratio = iters_f / max_iterations;
  gl_FragDepth = ratio;
  vec4 color_0 = vec4(0.0f, 0.0f, 0.0f, 1.0f);
  vec4 color_1 = vec4(0.0f, 1.0f, 0.0f, 1.0f);
  vec4 color_2 = vec4(0.0f, 0.0f, 1.0f, 1.0f);
  vec4 color_3 = vec4(1.0f, 0.0f, 0.0f, 1.0f);

  float fraction = 0.0f;
  if (ratio < color_ranges[1])
  {
    fraction = (ratio - color_ranges[0]) / (color_ranges[1] - color_ranges[0]);
    out_col = mix(color_0, color_1, fraction);
  }
  else if(ratio < color_ranges[2])
  {
    fraction = (ratio - color_ranges[1]) / (color_ranges[2] - color_ranges[1]);
    out_col = mix(color_1, color_2, fraction);
  }
  else
  {
    fraction = (ratio - color_ranges[2]) / (color_ranges[3] - color_ranges[2]);
    out_col = mix(color_2, color_3, fraction);
  }
}
