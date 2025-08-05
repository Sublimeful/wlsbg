/* This buffer is responsible for:
   - Convolution Layer 1 (kernel size=5x5, stride=1)   : 28x28 => 8x24x24
   - ReLU activation function
   - Max Pooling Layer 1 (kernel size=2x2, stride=2x2) : 8x24x24 => 8x12x12

   This is done as a 2-frame process: first the Convolution feature maps
   are calculated and stored in Buffer B, which is then read again to
   calculate and store the Max Pooling feature maps.
   It also draws the (pixelated) input layer on the right.
*/

#define pool_size 12.

// Get normalized input pixel
// p: [0, input_res-1]
float i(vec2 p) {
  p = vec2((p.x + .5) / iAspect, p.y + .5) / input_res;
  p.x += 1. - 1. / iAspect;
  float val = textureLod(iChannel0, p, log2(iResolution.y / input_res)).r;
  return (val - mean) / std; // Normalize
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
  vec2 uv = fragCoord / R;
  vec3 col = vec3(0);

  vec2 f = fragCoord - .5;
  ivec2 F = ivec2(f);

  // Max Pooling
  if (F.x < int(pool_size * feature_maps_1) && F.y < int(pool_size)) {
    ivec2 p = ivec2(f * 2.) + ivec2(0, pool_size);

    float x = max(texelFetch(iChannel1, p, 0).r,
                  max(texelFetch(iChannel1, p + ivec2(0, 1), 0).r,
                      max(texelFetch(iChannel1, p + ivec2(1, 0), 0).r,
                          texelFetch(iChannel1, p + ivec2(1, 1), 0).r)));

    // ReLU activation function
    x = max(0., x);

    col = vec3(x);
  }
  // Convolution
  else if (F.x < int(pool_size * feature_maps_1 * 2.) &&
           F.y >= int(pool_size) && F.y < int(pool_size * 3.)) {
    int fmap = int(f.x / (pool_size * 2.));
    vec2 p = mod(f - vec2(0, pool_size), pool_size * 2.);
    float x;

    if (fmap == 0)
      x = -.256220 * i(p) - .263963 * i(p + vec2(1, 0)) -
          .222650 * i(p + vec2(2, 0)) - .121450 * i(p + vec2(3, 0)) +
          .008962 * i(p + vec2(4, 0)) - .286535 * i(p + vec2(0, 1)) -
          .128346 * i(p + vec2(1, 1)) - .119005 * i(p + vec2(2, 1)) -
          .105653 * i(p + vec2(3, 1)) + .106190 * i(p + vec2(4, 1)) -
          .196784 * i(p + vec2(0, 2)) + .015316 * i(p + vec2(1, 2)) +
          .154883 * i(p + vec2(2, 2)) + .169439 * i(p + vec2(3, 2)) +
          .155174 * i(p + vec2(4, 2)) - .088119 * i(p + vec2(0, 3)) +
          .207242 * i(p + vec2(1, 3)) + .153355 * i(p + vec2(2, 3)) +
          .170941 * i(p + vec2(3, 3)) + .058379 * i(p + vec2(4, 3)) +
          .179226 * i(p + vec2(0, 4)) + .170095 * i(p + vec2(1, 4)) +
          .187621 * i(p + vec2(2, 4)) + .115604 * i(p + vec2(3, 4)) +
          .211174 * i(p + vec2(4, 4)) + .026794;
    else if (fmap == 1)
      x = -.208491 * i(p) - .246941 * i(p + vec2(1, 0)) -
          .118710 * i(p + vec2(2, 0)) + .029216 * i(p + vec2(3, 0)) +
          .134078 * i(p + vec2(4, 0)) + .175747 * i(p + vec2(0, 1)) +
          .050075 * i(p + vec2(1, 1)) - .259553 * i(p + vec2(2, 1)) -
          .155585 * i(p + vec2(3, 1)) - .184678 * i(p + vec2(4, 1)) +
          .216247 * i(p + vec2(0, 2)) + .372172 * i(p + vec2(1, 2)) +
          .049592 * i(p + vec2(2, 2)) - .160362 * i(p + vec2(3, 2)) +
          .087013 * i(p + vec2(4, 2)) - .088406 * i(p + vec2(0, 3)) +
          .108477 * i(p + vec2(1, 3)) + .387428 * i(p + vec2(2, 3)) +
          .045766 * i(p + vec2(3, 3)) + .116031 * i(p + vec2(4, 3)) -
          .219892 * i(p + vec2(0, 4)) - .266197 * i(p + vec2(1, 4)) +
          .089884 * i(p + vec2(2, 4)) + .015118 * i(p + vec2(3, 4)) +
          .100712 * i(p + vec2(4, 4)) + .024362;
    else if (fmap == 2)
      x = -.152888 * i(p) + .072108 * i(p + vec2(1, 0)) +
          .141602 * i(p + vec2(2, 0)) + .300121 * i(p + vec2(3, 0)) -
          .126745 * i(p + vec2(4, 0)) - .232032 * i(p + vec2(0, 1)) -
          .066877 * i(p + vec2(1, 1)) + .099079 * i(p + vec2(2, 1)) +
          .214114 * i(p + vec2(3, 1)) + .106322 * i(p + vec2(4, 1)) -
          .143861 * i(p + vec2(0, 2)) - .108627 * i(p + vec2(1, 2)) +
          .071171 * i(p + vec2(2, 2)) + .197545 * i(p + vec2(3, 2)) +
          .184703 * i(p + vec2(4, 2)) - .112155 * i(p + vec2(0, 3)) -
          .106069 * i(p + vec2(1, 3)) - .093000 * i(p + vec2(2, 3)) +
          .101620 * i(p + vec2(3, 3)) + .161687 * i(p + vec2(4, 3)) -
          .083832 * i(p + vec2(0, 4)) - .058263 * i(p + vec2(1, 4)) -
          .031437 * i(p + vec2(2, 4)) + .034528 * i(p + vec2(3, 4)) +
          .223640 * i(p + vec2(4, 4)) - .389324;
    else if (fmap == 3)
      x = .127065 * i(p) + .067651 * i(p + vec2(1, 0)) +
          .237392 * i(p + vec2(2, 0)) + .122720 * i(p + vec2(3, 0)) +
          .263380 * i(p + vec2(4, 0)) + .272143 * i(p + vec2(0, 1)) +
          .229455 * i(p + vec2(1, 1)) + .201271 * i(p + vec2(2, 1)) +
          .158494 * i(p + vec2(3, 1)) + .083364 * i(p + vec2(4, 1)) +
          .087063 * i(p + vec2(0, 2)) + .005914 * i(p + vec2(1, 2)) -
          .107822 * i(p + vec2(2, 2)) + .037689 * i(p + vec2(3, 2)) -
          .241690 * i(p + vec2(4, 2)) - .144014 * i(p + vec2(0, 3)) -
          .320903 * i(p + vec2(1, 3)) - .222353 * i(p + vec2(2, 3)) -
          .283808 * i(p + vec2(3, 3)) - .332431 * i(p + vec2(4, 3)) -
          .049706 * i(p + vec2(0, 4)) - .170751 * i(p + vec2(1, 4)) -
          .223001 * i(p + vec2(2, 4)) - .079481 * i(p + vec2(3, 4)) +
          .056033 * i(p + vec2(4, 4)) - .100076;
    else if (fmap == 4)
      x = .033839 * i(p) + .143132 * i(p + vec2(1, 0)) -
          .216749 * i(p + vec2(2, 0)) - .075409 * i(p + vec2(3, 0)) -
          .010285 * i(p + vec2(4, 0)) + .097860 * i(p + vec2(0, 1)) +
          .241089 * i(p + vec2(1, 1)) + .214650 * i(p + vec2(2, 1)) -
          .225313 * i(p + vec2(3, 1)) - .184724 * i(p + vec2(4, 1)) -
          .270974 * i(p + vec2(0, 2)) - .187646 * i(p + vec2(1, 2)) +
          .235756 * i(p + vec2(2, 2)) + .272372 * i(p + vec2(3, 2)) -
          .101422 * i(p + vec2(4, 2)) - .151017 * i(p + vec2(0, 3)) +
          .042975 * i(p + vec2(1, 3)) - .064422 * i(p + vec2(2, 3)) +
          .240397 * i(p + vec2(3, 3)) - .020506 * i(p + vec2(4, 3)) +
          .119857 * i(p + vec2(0, 4)) - .080291 * i(p + vec2(1, 4)) -
          .152493 * i(p + vec2(2, 4)) - .076686 * i(p + vec2(3, 4)) +
          .018775 * i(p + vec2(4, 4)) - .021946;
    else if (fmap == 5)
      x = -.408473 * i(p) - .134044 * i(p + vec2(1, 0)) -
          .069418 * i(p + vec2(2, 0)) + .135106 * i(p + vec2(3, 0)) +
          .173311 * i(p + vec2(4, 0)) - .176028 * i(p + vec2(0, 1)) -
          .379614 * i(p + vec2(1, 1)) - .162828 * i(p + vec2(2, 1)) -
          .082388 * i(p + vec2(3, 1)) - .015580 * i(p + vec2(4, 1)) -
          .027795 * i(p + vec2(0, 2)) - .173466 * i(p + vec2(1, 2)) -
          .435070 * i(p + vec2(2, 2)) - .292225 * i(p + vec2(3, 2)) -
          .234535 * i(p + vec2(4, 2)) + .202488 * i(p + vec2(0, 3)) +
          .089885 * i(p + vec2(1, 3)) - .082089 * i(p + vec2(2, 3)) -
          .095222 * i(p + vec2(3, 3)) - .332411 * i(p + vec2(4, 3)) +
          .152386 * i(p + vec2(0, 4)) + .277061 * i(p + vec2(1, 4)) +
          .114890 * i(p + vec2(2, 4)) + .061884 * i(p + vec2(3, 4)) -
          .197509 * i(p + vec2(4, 4)) + .094496;
    else if (fmap == 6)
      x = -.160558 * i(p) - .262113 * i(p + vec2(1, 0)) +
          .070456 * i(p + vec2(2, 0)) + .071991 * i(p + vec2(3, 0)) +
          .223662 * i(p + vec2(4, 0)) - .250266 * i(p + vec2(0, 1)) -
          .110144 * i(p + vec2(1, 1)) - .006879 * i(p + vec2(2, 1)) +
          .095233 * i(p + vec2(3, 1)) + .134715 * i(p + vec2(4, 1)) -
          .217089 * i(p + vec2(0, 2)) - .253514 * i(p + vec2(1, 2)) -
          .046932 * i(p + vec2(2, 2)) + .212780 * i(p + vec2(3, 2)) +
          .138915 * i(p + vec2(4, 2)) - .067716 * i(p + vec2(0, 3)) -
          .162607 * i(p + vec2(1, 3)) - .162463 * i(p + vec2(2, 3)) -
          .006180 * i(p + vec2(3, 3)) + .204881 * i(p + vec2(4, 3)) -
          .164372 * i(p + vec2(0, 4)) - .196290 * i(p + vec2(1, 4)) -
          .336171 * i(p + vec2(2, 4)) - .003429 * i(p + vec2(3, 4)) +
          .090525 * i(p + vec2(4, 4)) - .214098;
    else if (fmap == 7)
      x = .314642 * i(p) + .069870 * i(p + vec2(1, 0)) -
          .232515 * i(p + vec2(2, 0)) - .196920 * i(p + vec2(3, 0)) -
          .258605 * i(p + vec2(4, 0)) + .182785 * i(p + vec2(0, 1)) -
          .227795 * i(p + vec2(1, 1)) - .121011 * i(p + vec2(2, 1)) -
          .160892 * i(p + vec2(3, 1)) - .082476 * i(p + vec2(4, 1)) +
          .000842 * i(p + vec2(0, 2)) - .182144 * i(p + vec2(1, 2)) -
          .192695 * i(p + vec2(2, 2)) - .022503 * i(p + vec2(3, 2)) -
          .187570 * i(p + vec2(4, 2)) - .158919 * i(p + vec2(0, 3)) -
          .226402 * i(p + vec2(1, 3)) - .036234 * i(p + vec2(2, 3)) +
          .014082 * i(p + vec2(3, 3)) - .162542 * i(p + vec2(4, 3)) -
          .310638 * i(p + vec2(0, 4)) - .177386 * i(p + vec2(1, 4)) -
          .002980 * i(p + vec2(2, 4)) - .114051 * i(p + vec2(3, 4)) -
          .113300 * i(p + vec2(4, 4)) + .144752;

    // x = max(0., x);
    col = vec3(x);
  }
  // Displays the pixelated version of the input (on the right)
  else {
    uv.x -= 1. - 1. / iAspect;
    uv.x *= iAspect;
    uv = floor(uv * input_res);
    col = vec3(i(uv) * std + mean);
  }

  fragColor = vec4(col, 1);
}
