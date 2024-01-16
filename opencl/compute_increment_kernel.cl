__kernel void square(__global float* output, const unsigned int count) {
  int i = get_global_id(0);
  for (int j = 0; j < count ; j++) {
    output[i] = output[i] + 1;
   }
}
