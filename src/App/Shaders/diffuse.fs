#version 330 core

in vec2 fragCoord;

out vec4 out_col;

uniform vec2 screen_pos;
uniform float screen_scale;
uniform int max_iterations;
uniform float border2;

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
  float ratio = float(iters) / max_iterations;
  out_col = vec4(0.0f, ratio, 0.0f, 1.0f);
}
