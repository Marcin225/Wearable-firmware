#include <stdio.h>
#include <math.h>

#define PI 3.14159265358979323846f

// even samples 0, 2, 4 ... move to re, 
// odd samples 1, 3, 5 ... move to im

void swap(float *tab1, float *tab2) {
    float temp = *tab2;
    *tab2 = *tab1;
    *tab1 = temp;
}

void rfft(float *re, float *im, int N) { // N -> 1024
    
    int j = 0;
    for (int i = 0; i < N-1; i++) {
        if (i < j) {
            swap(&re[i], &re[j]);
            swap(&im[i], &im[j]);
        }
        int k = N / 2;
        while (k <= j) {
            j -= k;
            k /= 2;
        }
        j += k;
    }
    
    for (int stage_size = 2; stage_size <= N; stage_size *= 2) {
        
        int half_step = stage_size / 2;
        float step_angle = -2.0f * PI / stage_size;
        
        for (int k = 0; k < half_step; k++) {
            float current_angle = k * step_angle;
            float wr = cosf(current_angle); 
            float wi = sinf(current_angle);
            
            for (int i = k; i < N; i += stage_size) {
                int top_idx = i;
                int bottom_idx = i + half_step;
                
                float tr = (re[bottom_idx] * wr) - (im[bottom_idx] * wi);
                float ti = (re[bottom_idx] * wi) + (im[bottom_idx] * wr);
                
                re[bottom_idx] = re[top_idx] - tr;
                im[bottom_idx] = im[top_idx] - ti;

                re[top_idx] = re[top_idx] + tr;
                im[top_idx] = im[top_idx] + ti;
                
            }
        }
    }
    
    // unpacking
    int M = N / 2;
    
    float dc_value = re[0] + im[0];
    float nyquist_value = re[0] - im[0];
    
    re[0] = dc_value;
    im[0] = 0;
    
    for (int k = 1; k <= M; k++) {
        int mirror_k = N - k;
        
        float Er = (re[k] + re[mirror_k]) * 0.5f;
        float Ei = (im[k] - im[mirror_k]) * 0.5f;
        
        float Or = (im[k] + im[mirror_k]) * 0.5f;
        float Oi = (re[mirror_k] - re[k]) * 0.5f;
        
        float correction_angle = -PI * (float)k / (float)N;
        float wr = cosf(correction_angle);
        float wi = sinf(correction_angle);
        
        float tr = Or * wr - Oi * wi;
        float ti = Or * wi + Oi * wr;
        
        re[k] = Er + tr;
        im[k] = Ei + ti;
        
        re[mirror_k] = Er - tr;
        im[mirror_k] = ti - Ei;
    }
    
}