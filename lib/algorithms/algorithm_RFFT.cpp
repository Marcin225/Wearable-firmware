#include "algorithm_RFFT.h"
#include "fft_tables.h"

void calculate_angles(int32_t *wr, int32_t *wi, int angle_idx) {
    if (angle_idx <= 512) {
        *wr = sin_table_q31[512 - angle_idx];
        *wi = -sin_table_q31[angle_idx];
    } else {
        *wr = -sin_table_q31[angle_idx - 512];
        *wi = -sin_table_q31[1024 - angle_idx];
    }
}

void swap(int32_t *tab1, int32_t *tab2) {
    int32_t temp = *tab2;
    *tab2 = *tab1;
    *tab1 = temp;
}

// even samples 0, 2, 4 ... + zero padding move to re, 
// odd samples 1, 3, 5 ... + zero padding move to im

void rfft(int32_t *re, int32_t *im, int N) { 
    
    int M = N / 2;
    
    int j = 0;
    for (int i = 0; i < M - 1; i++) {
        if (i < j) {
            swap(&re[i], &re[j]);
            swap(&im[i], &im[j]);
        }
        int k = M / 2;
        while (k <= j) {
            j -= k;
            k /= 2;
        }
        j += k;
    }
    
    for (int stage_size = 2; stage_size <= M; stage_size *= 2) {
        
        int half_step = stage_size / 2;
        
        for (int k = 0; k < half_step; k++) {
            int angle_idx = k * 2048 / stage_size;
            int32_t wr, wi;
            calculate_angles(&wr, &wi, angle_idx);
            
            for (int i = k; i < M; i += stage_size) {
                int top_idx = i;
                int bottom_idx = i + half_step;
                
                int64_t temp_r1 = (int64_t)re[bottom_idx] * wr;
                int64_t temp_r2 = (int64_t)im[bottom_idx] * wi;
                int64_t temp_i1 = (int64_t)re[bottom_idx] * wi;
                int64_t temp_i2 = (int64_t)im[bottom_idx] * wr;
                
                int32_t tr = (int32_t)((temp_r1 - temp_i1 + (1LL << 30)) >> 31);
                int32_t ti = (int32_t)((temp_r2 + temp_i2 + (1LL << 30)) >> 31);
                
                int32_t temp_re_bottom = (re[top_idx] - tr + 1) >> 1;
                int32_t temp_im_bottom = (im[top_idx] - ti + 1) >> 1;
                int32_t temp_re_top = (re[top_idx] + tr + 1) >> 1;
                int32_t temp_im_top = (im[top_idx] + ti + 1) >> 1;
                
                re[bottom_idx] = temp_re_bottom;
                im[bottom_idx] = temp_im_bottom;

                re[top_idx] = temp_re_top;
                im[top_idx] = temp_im_top;
            }
        }
    }
    
    int32_t dc_value = re[0] + im[0];
    int32_t nyquist_value = re[0] - im[0];
    
    re[0] = dc_value;
    im[0] = 0;
    
    for (int k = 1; k < M; k++) {
        int mirror_k = M - k;
        
        int32_t Er = (re[k] + re[mirror_k] + 1) >> 1;
        int32_t Ei = (im[k] - im[mirror_k] + 1) >> 1;
        
        int32_t Or = (im[k] + im[mirror_k] + 1) >> 1;
        int32_t Oi = (re[mirror_k] - re[k] + 1) >> 1;
        
        int32_t wr, wi;
        calculate_angles(&wr, &wi, k * (2048 / N)); 
        
        int64_t temp_r1 = (int64_t)Or * wr;
        int64_t temp_r2 = (int64_t)Or * wi;
        int64_t temp_i1 = (int64_t)Oi * wi;
        int64_t temp_i2 = (int64_t)Oi * wr;
        
        int32_t tr = (int32_t)((temp_r1 - temp_i1 + (1LL << 30)) >> 31);
        int32_t ti = (int32_t)((temp_r2 + temp_i2 + (1LL << 30)) >> 31);
        
        int32_t temp_re = Er + tr;
        int32_t temp_im = Ei + ti;
        int32_t temp_re_mirror = Er - tr;
        int32_t temp_im_mirror = ti - Ei;
        
        re[k] = temp_re;
        im[k] = temp_im;
        
        re[mirror_k] = temp_re_mirror;
        im[mirror_k] = temp_im_mirror;
    }
    
    re[M] = nyquist_value;
    im[M] = 0;
}