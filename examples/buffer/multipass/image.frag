/* CNN Digit Recognizer by @kishimisu (2023) -
  https://www.shadertoy.com/view/msVXWD

    A Convolutional Neural Network implementation pre-trained on the MNIST
  dataset.

    - Draw digits between 0-9 in the right square
    - It can take some time to compile
    - Exactly 2023 trainable parameters!

    Python notebook created for this project:
    - https://colab.research.google.com/drive/1bOAI1xb8Z4D5LoNd_lAm6BIuLJ3OYjNi

    ***

    I wasn't very satisfied with my previous version (see fork) that was using a
    very basic network containing one hidden layer only. It achieved 92%
  accuracy during training but translated poorly in shaders.

    This time I've used a more efficient approach for this kind of task: a basic
  CNN with only 2 convolution + max pooling layers, and only one fully connected
  layer. It achieved a 98% accuracy during training, and it seems to make better
  predictions in shadertoy despite having half less parameters (2023 vs 4120).

    Note that it seem to struggle to predict the '6' and '9', and also if the
  digits are too off-centered. I couldn't augment the data for such a tiny
  network size!

  ### Technical issues ###

    The first CNN architecture I tried to implement was very similar to this
  one, but instead of having 8 and 5 feature maps respectively, I had 10 and 10.
    Increasing the number of feature map exponentially increases the number of
  weights, thus the amount of computations, characters and compilation time.

    Here the computational intensity doesn't seem to be a problem, but the
  compilation time is what really made me struggle with this project.

    If you look around in the code, you will see that there are no loops for the
  network calculation, despite the fact that a CNN heavily relies on loops and
  nested loops. When using loops, the compilation time went to the moon and it
  was very impractical, this is why I preferred to decompose every calculation,
  at the cost of a big increase in the character count (this shader could be
  twice as short) and degraded readability.

  ### Network Architecture ###

  (Buffer A)
  * Input layer
   - 28x28 = 784 inputs

  (Buffer B)
  * First convolution group (8 feature maps):
   - Convolution Layer (kernel size=5x5, stride=1)   : 28x28 => 8x24x24
   - ReLU activation function
   - Max Pooling Layer (kernel size=2x2, stride=2x2) : 8x24x24 => 8x12x12

  (Buffer C)
  * Second convolution group (5 feature maps):
   - Convolution Layer (kernel size=5x5, stride=1)   : 8x12x12 => 5x8x8
   - ReLU activation function
   - Max Pooling Layer (kernel size=2x2, stride=2x2) : 5x8x8 => 5x4x4

  (Buffer D)
  * Fully connected layer
   - 5x4x4 = 80 => 10

  (Image)
  * Softmax function

   Total number of parameters: 2,023!
*/

// Text utilities
float char(vec2 u, vec2 p) {
  return texture(iChannel3, (u + p) / 16.).r * step(0., min(u.x, u.y)) *
         step(max(u.x, u.y), 1.);
}
float printInputLayer(vec2 u) { print _I _n _p _u _t _sp _L _a _y _e _r _end }
float printConv(vec2 u, int i) {
  print _C _o _n _v _o _l _u _t _i _o _n _sp _dig(i) _end
}
float printMaxPooling(vec2 u, int i) {
  print _M _a _x _sp _P _o _o _l _i _n _g _sp _dig(i) _end
}
float printFullyConnected(vec2 u) {
  print _F _u _l _l _y _sp _C _o _n _n _e _c _t _e _d _end
}
float printOutputLayer(vec2 u) {
  print _O _u _t _p _u _t _sp _L _a _y _e _r _end
}
float printPrediction(vec2 u) { print _P _r _e _d _i _c _t _i _o _n _dd _end }

float rect(vec2 p) {
  vec2 d = abs(p - .5) - .5;
  return length(max(d, 0.)) + min(max(d.x, d.y), 0.);
}

// Color
float sigmoid(float x) { return 2.0 / (1.0 + exp(-x)) - 1.0; }
vec3 pal(float x) {
  x /= 8.;
  x = 1. - x;
  return cos(6.28318 * (vec3(1.258, 0.838, 0.708) * x + vec3(0.5))) * .5 + .5;
}

// Calculates the denominator part for the softmax function,
// And take advantage of the loop to store the predicted outcome
float getExpSumAndPrediction(inout vec2 prediction) {
  float sum = 0.;
  float upp = -1e7;

  for (float i = 0.; i < num_classes; i++) {
    float val = texture(iChannel2, vec2(i + .5, .5) / R).r;
    if (val > upp) {
      upp = val;
      prediction = vec2(i, exp(val));
    }
    sum += exp(val);
  }
  return sum;
}

// Displays a portion of a texture containing feature map data
// p : normalized uv coordinates (0-1)
// s : scaling factor
// r.xy : x/y start (in pixels)
// r.zw : x/y end   (in pixels)
vec3 displayFeatureMaps(vec2 p, vec2 s, vec4 r, float feature_maps,
                        sampler2D smp) {
  p = p * s + vec2(1. - s.x, 0) / 2.; // scale and center uvs

  float fp = 1. / feature_maps;
  float id = floor(p.x * feature_maps); // current feature map id
  float m = 1.1;                        // border width

  // check bounds
  if (min(p.x, p.y) < 0. || max(p.x, p.y) > 1. ||
      abs(mod(p.x, fp) - fp * .5) > fp / m / 2. || abs(p.y - .5) > 1. / m / 2.)
    return vec3(0);

  m *= .999;                 // fix overflow issue
  p *= m;                    // scale down
  p.x -= (m - 1.) * id * fp; // offset x from id
  p.x -= (m - 1.) * .5 * fp; // re-center x
  p.y -= (m - 1.) * .5;      // re-center y
  p *= r.zw / R;             // crop
  p.xy += r.xy / R;          // offset origin

  float val = texture(smp, p).r; // Get value
  if (r.y > .0)
    val = 1. -
          sigmoid(val) *
              1.3; // Change color map for convolution (as it can go negative)
  return pal(val); // Return mapped color
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
  vec2 uv = fragCoord / R;
  vec3 col;

  vec2 prediction; // index, confidence
  float exp_sum = getExpSumAndPrediction(prediction);

  uv.x *= iAspect;

  // Left part of the screen (visualizations)
  if (uv.x < iAspect - 1.) {
    uv.x += 1.;
    // Prediction panel
    if (uv.y < .12) {
      uv = vec2(uv.x - 1.05, uv.y) * 10.;
      col += printPrediction(uv);
      col += vec3(1, 0, 0) *
             char(uv * .7 - vec2(3.5, -.1), vec2(prediction.x, 12));
      col = mix(pal(prediction.y / exp_sum * 3.), col, length(col));
    }
    // Output panel
    else if (uv.y < .47) {
      // Transform to upper right area
      vec2 tuv = vec2((uv.x - 1.) / (iAspect - 1.), (uv.y - .12) / (.43 - .12));
      // Current output index
      float idx = floor(tuv.x * num_classes);

      // Output value for the current index
      float val = texture(iChannel2, vec2(idx + .5, .5) / R).r;
      // Apply the softmax function
      val = exp(val) / exp_sum;

      // Draw bars
      col = mix(vec3(1, 0, 0), vec3(0, 1, 0), val) *
            smoothstep(0., .01, val - tuv.y);
      col = pal(val * 2.5) * smoothstep(0., .01, val - tuv.y);

      // Draw "output" text
      uv = vec2(uv.x - .98, uv.y - .41) * 22.;
      col += printOutputLayer(uv);

      // Draw digits
      tuv = vec2(fract(tuv.x * num_classes), tuv.y * 4.);
      col += vec3(char(tuv, vec2(idx, 12)));
    }
    // Fully connected layer
    else if (uv.y < .5) {
      vec2 tuv = vec2((uv.x - 1.) / (iAspect - 1.), (uv.y - .47) / (.48 - .47));
      float id = floor(tuv.x * 16. * feature_maps_2);
      float x = mod(id, 4. * feature_maps_2);
      float y = floor(id / (4. * feature_maps_2));

      col = pal(texelFetch(iChannel1, ivec2(x, y), 0).r);
    }
    // Feature maps
    else {
      // Display texts
      uv = (uv - vec2(.98, .96)) * 24.;
      col += printConv(uv, 1);

      uv.y += 3.;
      col += printMaxPooling(uv, 1);

      uv.y += 2.85;
      col += printConv(uv, 2);

      uv.y += 2.5;
      col += printMaxPooling(uv, 2);

      uv.y += 2.4;
      col += printFullyConnected(uv);

      // Remap uv
      uv = fragCoord / R;
      uv.x = uv.x / (1. - R.y / R.x);

      // Display feature maps
      uv.y -= .88;
      col += displayFeatureMaps(uv, vec2(10. / feature_maps_1, 12),
                                vec4(0, 12, 24. * feature_maps_1, 24),
                                feature_maps_1, iChannel0);

      uv.y += .115;
      col += displayFeatureMaps(uv, vec2(1.2 * 10. / feature_maps_1, 1.2 * 12.),
                                vec4(0, 0, 12. * feature_maps_1, 12),
                                feature_maps_1, iChannel0);

      uv.y += .11;
      col += displayFeatureMaps(
          uv, vec2(1.95 * 10. / feature_maps_1, 1.95 * 8.),
          vec4(0, 4, 8. * feature_maps_2, 8), feature_maps_2, iChannel1);

      uv.y += .1;
      col += displayFeatureMaps(uv, vec2(2.5 * 10. / feature_maps_1, 2.5 * 8.),
                                vec4(0, 0, 4. * feature_maps_2, 4),
                                feature_maps_2, iChannel1);
    }
  }
  // Display drawing
  else {
    uv.x = uv.x - iAspect + 1.;
    col += printInputLayer((uv - vec2(0, .9)) * 15.);
    col += vec3(0.14, 1, 0.51) * smoothstep(.015, 0.004, abs(rect(uv)));
    col += texture(iChannel0, fragCoord / R).r;
  }

  fragColor = vec4(col, 1);
}
