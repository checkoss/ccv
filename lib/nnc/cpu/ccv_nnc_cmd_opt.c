#include <ccv.h>
#include <ccv_internal.h>
#include <nnc/ccv_nnc.h>
#include <nnc/ccv_nnc_easy.h>
#include <nnc/ccv_nnc_internal.h>
#if defined(HAVE_SSE2)
#include <xmmintrin.h>
#elif defined(HAVE_NEON)
#include <arm_neon.h>
#endif
#ifdef USE_OPENMP
#include <omp.h>
#endif
#ifdef USE_DISPATCH
#include <dispatch/dispatch.h>
#endif

enum {
	CCV_NNC_CMD_OPT_CONV_ALGO_DC, // Direct convolution
	CCV_NNC_CMD_OPT_CONV_ALGO_GEMM, // GEMM (for 1x1)
	CCV_NNC_CMD_OPT_CONV_ALGO_WINOGRAD, // Winograd algorithm
	CCV_NNC_CMD_OPT_CONV_ALGO_FFT, // Fast Fourier transform
	CCV_NNC_CMD_OPT_CONV_ALGO_COUNT
};

#define set_n_m_dim(i, x, wd, ad) \
	do { \
		n[x] = ccv_max((i) * hint.stride.dim[x + 1] - hint.border.begin[x + 1], 0) - ((i) * hint.stride.dim[x + 1] - hint.border.begin[x + 1]); \
		m[x] = wd[x + 1] - n[x] - ((i) * hint.stride.dim[x + 1] - hint.border.begin[x + 1] + wd[x + 1] - ccv_min(ad[x + 1], (i) * hint.stride.dim[x + 1] - hint.border.begin[x + 1] + wd[x + 1])); \
	} while (0)

inline static void _ccv_nnc_winograd_4x4_3x3_gwtg_ref(const float* w, const int c, float* gwtg)
{
	int i;
	for (i = 0; i < c; i++)
	{
		float g[18];
		/*
		 * a0, b1, c2
		 * d3, e4, f5
		 * g6, h7, i8
		 * {{a/4, b/4, c/4},
		 * {1/6 (-a - d - g), 1/6 (-b - e - h), 1/6 (-c - f - i)},
		 * {1/6 (-a + d - g), 1/6 (-b + e - h), 1/6 (-c + f - i)},
		 * {1/24 (a + 2 d + 4 g), 1/24 (b + 2 e + 4 h), 1/24 (c + 2 f + 4 i)},
		 * {1/24 (a - 2 d + 4 g), 1/24 (b - 2 e + 4 h), 1/24 (c - 2 f + 4 i)},
		 * {g, h, i}}
		 */
		/* row 1 */
		g[0] = w[i] / 4;
		g[1] = w[c + i] / 4;
		g[2] = w[2 * c + i] / 4;
		/* row 2 */
		g[3] = -(w[i] + w[3 * c + i] + w[6 * c + i]) / 6;
		g[4] = -(w[c + i] + w[4 * c + i] + w[7 * c + i]) / 6;
		g[5] = -(w[2 * c + i] + w[5 * c + i] + w[8 * c + i]) / 6;
		/* row 3 */
		g[6] = (-w[i] + w[3 * c + i] - w[6 * c + i]) / 6;
		g[7] = (-w[c + i] + w[4 * c + i] - w[7 * c + i]) / 6;
		g[8] = (-w[2 * c + i] + w[5 * c + i] - w[8 * c + i]) / 6;
		/* row 4 */
		g[9] = (w[i] + 2 * w[3 * c + i] + 4 * w[6 * c + i]) / 24;
		g[10] = (w[c + i] + 2 * w[4 * c + i] + 4 * w[7 * c + i]) / 24;
		g[11] = (w[2 * c + i] + 2 * w[5 * c + i] + 4 * w[8 * c + i]) / 24;
		/* row 5 */
		g[12] = (w[i] - 2 * w[3 * c + i] + 4 * w[6 * c + i]) / 24;
		g[13] = (w[c + i] - 2 * w[4 * c + i] + 4 * w[7 * c + i]) / 24;
		g[14] = (w[2 * c + i] - 2 * w[5 * c + i] + 4 * w[8 * c + i]) / 24;
		/* row 6 */
		g[15] = w[6 * c + i];
		g[16] = w[7 * c + i];
		g[17] = w[8 * c + i];
		/*
		 * a0, b1, c2
		 * d3, e4, f5
		 * g6, h7, i8
		 * j9, k10,l11
		 * m12,n13,o14
		 * p15,q16,r17
		 * {{a/4, 1/6 (-a - b - c), 1/6 (-a + b - c), 1/24 (a + 2 b + 4 c), 1/24 (a - 2 b + 4 c), c},
		 * {d/4, 1/6 (-d - e - f), 1/6 (-d + e - f), 1/24 (d + 2 e + 4 f), 1/24 (d - 2 e + 4 f), f},
		 * {g/4, 1/6 (-g - h - i), 1/6 (-g + h - i), 1/24 (g + 2 h + 4 i), 1/24 (g - 2 h + 4 i), i},
		 * {j/4, 1/6 (-j - k - l), 1/6 (-j + k - l), 1/24 (j + 2 k + 4 l), 1/24 (j - 2 k + 4 l), l},
		 * {m/4, 1/6 (-m - n - o), 1/6 (-m + n - o), 1/24 (m + 2 n + 4 o), 1/24 (m - 2 n + 4 o), o},
		 * {p/4, 1/6 (-p - q - r), 1/6 (-p + q - r), 1/24 (p + 2 q + 4 r), 1/24 (p - 2 q + 4 r), r}}
		 */
		/* row 1 */
		gwtg[0] = g[0] / 4;
		gwtg[c] = -(g[0] + g[1] + g[2]) / 6;
		gwtg[2 * c] = (-g[0] + g[1] - g[2]) / 6;
		gwtg[3 * c] = (g[0] + 2 * g[1] + 4 * g[2]) / 24;
		gwtg[4 * c] = (g[0] - 2 * g[1] + 4 * g[2]) / 24;
		gwtg[5 * c] = g[2];
		/* row 2 */
		gwtg[6 * c] = g[3] / 4;
		gwtg[7 * c] = -(g[3] + g[4] + g[5]) / 6;
		gwtg[8 * c] = (-g[3] + g[4] - g[5]) / 6;
		gwtg[9 * c] = (g[3] + 2 * g[4] + 4 * g[5]) / 24;
		gwtg[10 * c] = (g[3] - 2 * g[4] + 4 * g[5]) / 24;
		gwtg[11 * c] = g[5];
		/* row 3 */
		gwtg[12 * c] = g[6] / 4;
		gwtg[13 * c] = -(g[6] + g[7] + g[8]) / 6;
		gwtg[14 * c] = (-g[6] + g[7] - g[8]) / 6;
		gwtg[15 * c] = (g[6] + 2 * g[7] + 4 * g[8]) / 24;
		gwtg[16 * c] = (g[6] - 2 * g[7] + 4 * g[8]) / 24;
		gwtg[17 * c] = g[8];
		/* row 4 */
		gwtg[18 * c] = g[9] / 4;
		gwtg[19 * c] = -(g[9] + g[10] + g[11]) / 6;
		gwtg[20 * c] = (-g[9] + g[10] - g[11]) / 6;
		gwtg[21 * c] = (g[9] + 2 * g[10] + 4 * g[11]) / 24;
		gwtg[22 * c] = (g[9] - 2 * g[10] + 4 * g[11]) / 24;
		gwtg[23 * c] = g[11];
		/* row 5 */
		gwtg[24 * c] = g[12] / 4;
		gwtg[25 * c] = -(g[12] + g[13] + g[14]) / 6;
		gwtg[26 * c] = (-g[12] + g[13] - g[14]) / 6;
		gwtg[27 * c] = (g[12] + 2 * g[13] + 4 * g[14]) / 24;
		gwtg[28 * c] = (g[12] - 2 * g[13] + 4 * g[14]) / 24;
		gwtg[29 * c] = g[14];
		/* row 6 */
		gwtg[30 * c] = g[15] / 4;
		gwtg[31 * c] = -(g[15] + g[16] + g[17]) / 6;
		gwtg[32 * c] = (-g[15] + g[16] - g[17]) / 6;
		gwtg[33 * c] = (g[15] + 2 * g[16] + 4 * g[17]) / 24;
		gwtg[34 * c] = (g[15] - 2 * g[16] + 4 * g[17]) / 24;
		gwtg[35 * c] = g[17];
		++gwtg;
	}
}

static int _ccv_nnc_conv_forw_4x4_3x3_winograd_ref(const ccv_nnc_tensor_view_t* a, const ccv_nnc_tensor_t* w, const ccv_nnc_tensor_t* bias, const ccv_nnc_hint_t hint, ccv_nnc_tensor_view_t* b)
{
	const int* ainc = CCV_IS_TENSOR_VIEW(a) ? a->inc : a->info.dim;
	const int* binc = CCV_IS_TENSOR_VIEW(b) ? b->inc : b->info.dim;
	assert(hint.border.begin[1] <= 1);
	assert(hint.border.begin[2] <= 1);
	assert(w->info.dim[1] == 3);
	assert(w->info.dim[2] == 3);
	const int jump_dim[CCV_NNC_MAX_DIM] = {
		(b->info.dim[1] + 3) / 4, (b->info.dim[2] + 3) / 4
	};
	// allocating workspace memory for kernel reshaping and input reshaping.
#if FOR_IS_PARALLEL
	// If we do parallel for, we need to allocate input reshaping for each block.
	float* const workmem = (float*)ccmalloc(sizeof(float) * (36 * a->info.dim[0] * jump_dim[1] + 36 * w->info.dim[0] * w->info.dim[3]));
#else
	// Otherwise, just one block.
	float* const workmem = (float*)ccmalloc(sizeof(float) * (36 * a->info.dim[0] + 36 * w->info.dim[0] * w->info.dim[3]));
#endif
	if (!workmem)
		return CCV_NNC_EXEC_OOM;
	// Convert w to a 6x6 matrix, by computing G.w.T(G) // T for transpose.
	float* const gwtg = workmem;
	float* const btdb = workmem + 36 * w->info.dim[0] * w->info.dim[3];
	parallel_for(k, w->info.dim[3]) {
		_ccv_nnc_winograd_4x4_3x3_gwtg_ref(w->data.f32 + k * w->info.dim[2] * w->info.dim[1] * w->info.dim[0], w->info.dim[0], gwtg + k * 36 * w->info.dim[0]);
	} parallel_endfor
	// kernel weight for one dim.
	const float* const biasval = bias->data.f32;
	// Workaround issues of dispatch_apply (cannot reference to on-stack array)
	const int tile_dim_s[CCV_NNC_MAX_DIM_ALLOC] = {
		w->info.dim[0], 6, 6, w->info.dim[3]
	};
	const int* const tile_dim = tile_dim_s;
	// This block will be cause in each for-loop, therefore, you can use it to generate some temporary variables.
	parallel_for(i, jump_dim[1]) {
		const int y = i * 4; // i is unsigned.
		int j, x, k, c;
		int n[CCV_NNC_MAX_DIM];
		int m[CCV_NNC_MAX_DIM];
		int z[CCV_NNC_MAX_DIM];
		set_n_m_dim(y, 1, tile_dim, a->info.dim);
		z[1] = ccv_min(y + 4, b->info.dim[2]) - y;
		const float* ap = a->data.f32 + ccv_max(y - hint.border.begin[2], 0) * ainc[1] * ainc[0];
		float* bp = b->data.f32 + y * binc[1] * binc[0];
		for (x = 0; x < b->info.dim[1]; x += 4)
		{
			set_n_m_dim(x, 0, tile_dim, a->info.dim);
			z[0] = ccv_min(x + 4, b->info.dim[1]) - x;
#if FOR_IS_PARALLEL
			float* g = btdb + i * 36 * a->info.dim[0];
#else
			float* g = btdb;
#endif
			// zero g such that we can have zero-padding.
			memset(g, 0, sizeof(float) * 36 * a->info.dim[0]);
			int dx, dy;
			const float* apz = ap + ccv_max(x - hint.border.begin[1], 0) * ainc[0];
			float* gz = g + (n[1] * 6 + n[0]) * a->info.dim[0];
			#pragma unroll 6
			for (dy = 0; dy < m[1]; dy++)
			{
				#pragma unroll 6
				for (dx = 0; dx < m[0]; dx++)
				{
					float* const gzu = gz + (dy * 6 + dx) * a->info.dim[0];
					for (c = 0; c < a->info.dim[0]; c++)
						gzu[c] = apz[dx * ainc[0] + c];
				}
				apz += ainc[1] * ainc[0];
			}
			for (c = 0; c < a->info.dim[0]; c++)
			{
				/*
				 * a0, a1, a2, a3, a4, a5,
				 * b6, b7, b8, b9, b10,l11,
				 * c12,c13,c14,c15,c16,c17,
				 * d18,d19,d20,d21,d22,d23,
				 * e24,e25,e26,e27,e28,e29,
				 * f30,f31,f32,f33,f34,f35
				 * {{4 a0 - 5 c12 + e24, 4 a1 - 5 c13 + e25, 4 a2 - 5 c14 + e26, 4 a3 - 5 c15 + e27, 4 a4 - 5 c16 + e28, 4 a5 - 5 c17 + e29},
				 * {-4 b6 - 4 c12 + d18 + e24, -4 b7 - 4 c13 + d19 + e25, -4 b8 - 4 c14 + d20 + e26, -4 b9 - 4 c15 + d21 + e27, -4 b10 - 4 c16 + d22 + e28, -4 b11 - 4 c17 + d23 + e29},
				 * {4 b6 - 4 c12 - d18 + e24, 4 b7 - 4 c13 - d19 + e25, 4 b8 - 4 c14 - d20 + e26, 4 b9 - 4 c15 - d21 + e27, 4 b10 - 4 c16 - d22 + e28, 4 b11 - 4 c17 - d23 + e29},
				 * {-2 b6 - c12 + 2 d18 + e24, -2 b7 - c13 + 2 d19 + e25, -2 b8 - c14 + 2 d20 + e26, -2 b9 - c15 + 2 d21 + e27, -2 b10 - c16 + 2 d22 + e28, -2 b11 - c17 + 2 d23 + e29},
				 * {2 b6 - c12 - 2 d18 + e24, 2 b7 - c13 - 2 d19 + e25, 2 b8 - c14 - 2 d20 + e26, 2 b9 - c15 - 2 d21 + e27, 2 b10 - c16 - 2 d22 + e28, 2 b11 - c17 - 2 d23 + e29},
				 * {4 b6 - 5 d18 + f30, 4 b7 - 5 d19 + f31, 4 b8 - 5 d20 + f32, 4 b9 - 5 d21 + f33, 4 b10 - 5 d22 + f34, 4 b11 - 5 d23 + f35}}
				 */
				float d[36];
				/* BT.d */
				#pragma unroll 6
				for (j = 0; j < 6; j++)
				{
					float g0 = g[j * a->info.dim[0]];
					float g12 = g[(12 + j) * a->info.dim[0]];
					float g24 = g[(24 + j) * a->info.dim[0]];
					/* row 1 */
					d[j] = 4 * g0 - 5 * g12 + g24;
					float g6 = g[(6 + j) * a->info.dim[0]];
					float g18 = g[(18 + j) * a->info.dim[0]];
					/* row 2 */
					d[6 + j] = -4 * (g6 + g12) + g18 + g24;
					/* row 3 */
					d[12 + j] = 4 * (g6 - g12) - g18 + g24;
					/* row 4 */
					d[18 + j] = 2 * (g18 - g6) - g12 + g24;
					/* row 5 */
					d[24 + j] = 2 * (g6 - g18) - g12 + g24;
					float g30 = g[(30 + j) * a->info.dim[0]];
					/* row 6 */
					d[30 + j] = 4 * g6 - 5 * g18 + g30;
				}
				/*
				 * a0, a1, a2, a3, a4, a5,
				 * b6, b7, b8, b9, b10,l11,
				 * c12,c13,c14,c15,c16,c17,
				 * d18,d19,d20,d21,d22,d23,
				 * e24,e25,e26,e27,e28,e29,
				 * f30,f31,f32,f33,f34,f35
				 * {{4 a0 - 5 a2 + a4, -4 a1 - 4 a2 + a3 + a4, 4 a1 - 4 a2 - a3 + a4, -2 a1 - a2 + 2 a3 + a4, 2 a1 - a2 - 2 a3 + a4, 4 a1 - 5 a3 + a5},
				 * {b10 + 4 b6 - 5 b8, b10 - 4 b7 - 4 b8 + b9, b10 + 4 b7 - 4 b8 - b9, b10 - 2 b7 - b8 + 2 b9, b10 + 2 b7 - b8 - 2 b9, b11 + 4 b7 - 5 b9},
				 * {4 c12 - 5 c14 + c16, -4 c13 - 4 c14 + c15 + c16, 4 c13 - 4 c14 - c15 + c16, -2 c13 - c14 + 2 c15 + c16, 2 c13 - c14 - 2 c15 + c16, 4 c13 - 5 c15 + c17},
				 * {4 d18 - 5 d20 + d22, -4 d19 - 4 d20 + d21 + d22, 4 d19 - 4 d20 - d21 + d22, -2 d19 - d20 + 2 d21 + d22, 2 d19 - d20 - 2 d21 + d22, 4 d19 - 5 d21 + d23},
				 * {4 e24 - 5 e26 + e28, -4 e25 - 4 e26 + e27 + e28, 4 e25 - 4 e26 - e27 + e28, -2 e25 - e26 + 2 e27 + e28, 2 e25 - e26 - 2 e27 + e28, 4 e25 - 5 e27 + e29},
				 * {4 f30 - 5 f32 + f34, -4 f31 - 4 f32 + f33 + f34, 4 f31 - 4 f32 - f33 + f34, -2 f31 - f32 + 2 f33 + f34, 2 f31 - f32 - 2 f33 + f34, 4 f31 - 5 f33 + f35}}
				 */
				/* BT.d.B */
				#pragma unroll 6
				for (j = 0; j < 6; j++)
				{
					/* row 1 - 6 */
					float* const gz = g + j * 6 * a->info.dim[0];
					float* const dz = d + j * 6;
					gz[0] = 4 * dz[0] - 5 * dz[2] + dz[4];
					gz[a->info.dim[0]] = -4 * (dz[1] + dz[2]) + dz[3] + dz[4];
					gz[2 * a->info.dim[0]] = 4 * (dz[1] - dz[2]) - dz[3] + dz[4];
					gz[3 * a->info.dim[0]] = 2 * (dz[3] - dz[1]) - dz[2] + dz[4];
					gz[4 * a->info.dim[0]] = 2 * (dz[1] - dz[3]) - dz[2] + dz[4];
					gz[5 * a->info.dim[0]] = 4 * dz[1] - 5 * dz[3] + dz[5];
				}
				// move to the next channel
				++g;
			}
			const float* wpz = gwtg;
			for (k = 0; k < w->info.dim[3]; k++)
			{
				float q[36];
#if FOR_IS_PARALLEL
				g = btdb + i * 36 * a->info.dim[0];
#else
				g = btdb;
#endif
				for (j = 0; j < 36; j++)
				{
					float b = 0;
					for (c = 0; c < a->info.dim[0]; c++)
						b += g[c] * wpz[c];
					q[j] = b;
					g += a->info.dim[0];
					wpz += a->info.dim[0];
				}
				/*
				 * a0, a1, a2, a3, a4, a5,
				 * b6, b7, b8, b9, b10,l11,
				 * c12,c13,c14,c15,c16,c17,
				 * d18,d19,d20,d21,d22,d23,
				 * e24,e25,e26,e27,e28,e29,
				 * f30,f31,f32,f33,f34,f35
				 * {{a0 + b6 + c12 + d18 + e24, a1 + b7 + c13 + d19 + e25, a2 + b8 + c14 + d20 + e26, a3 + b9 + c15 + d21 + e27, a4 + b10 + c16 + d22 + e28, a5 + b11 + c17 + d23 + e29},
				 * {b6 - c12 + 2 d18 - 2 e24, b7 - c13 + 2 d19 - 2 e25, b8 - c14 + 2 d20 - 2 e26, b9 - c15 + 2 d21 - 2 e27, b10 - c16 + 2 d22 - 2 e28, b11 - c17 + 2 d23 - 2 e29},
				 * {b6 + c12 + 4 (d18 + e24), b7 + c13 + 4 (d19 + e25), b8 + c14 + 4 (d20 + e26), b9 + c15 + 4 (d21 + e27), b10 + c16 + 4 (d22 + e28), b11 + c17 + 4 (d23 + e29)},
				 * {b6 - c12 + 8 d18 - 8 e24 + f30, b7 - c13 + 8 d19 - 8 e25 + f31, b8 - c14 + 8 d20 - 8 e26 + f32, b9 - c15 + 8 d21 - 8 e27 + f33, b10 - c16 + 8 d22 - 8 e28 + f34, b11 - c17 + 8 d23 - 8 e29 + f35}}
				 */
				float d[24];
				/* row 1 */
				d[0] = q[0] + q[6] + q[12] + q[18] + q[24];
				d[1] = q[1] + q[7] + q[13] + q[19] + q[25];
				d[2] = q[2] + q[8] + q[14] + q[20] + q[26];
				d[3] = q[3] + q[9] + q[15] + q[21] + q[27];
				d[4] = q[4] + q[10] + q[16] + q[22] + q[28];
				d[5] = q[5] + q[11] + q[17] + q[23] + q[29];
				/* row 2 */
				d[6] = q[6] - q[12] + 2 * (q[18] - q[24]);
				d[7] = q[7] - q[13] + 2 * (q[19] - q[25]);
				d[8] = q[8] - q[14] + 2 * (q[20] - q[26]);
				d[9] = q[9] - q[15] + 2 * (q[21] - q[27]);
				d[10] = q[10] - q[16] + 2 * (q[22] - q[28]);
				d[11] = q[11] - q[17] + 2 * (q[23] - q[29]);
				/* row 3 */
				d[12] = q[6] + q[12] + 4 * (q[18] + q[24]);
				d[13] = q[7] + q[13] + 4 * (q[19] + q[25]);
				d[14] = q[8] + q[14] + 4 * (q[20] + q[26]);
				d[15] = q[9] + q[15] + 4 * (q[21] + q[27]);
				d[16] = q[10] + q[16] + 4 * (q[22] + q[28]);
				d[17] = q[11] + q[17] + 4 * (q[23] + q[29]);
				/* row 4 */
				d[18] = q[6] - q[12] + 8 * (q[18] - q[24]) + q[30];
				d[19] = q[7] - q[13] + 8 * (q[19] - q[25]) + q[31];
				d[20] = q[8] - q[14] + 8 * (q[20] - q[26]) + q[32];
				d[21] = q[9] - q[15] + 8 * (q[21] - q[27]) + q[33];
				d[22] = q[10] - q[16] + 8 * (q[22] - q[28]) + q[34];
				d[23] = q[11] - q[17] + 8 * (q[23] - q[29]) + q[35];
				/*
				 * {{a0 + a1 + a2 + a3 + a4, a1 - a2 + 2 a3 - 2 a4, a1 + a2 + 4 (a3 + a4), a1 - a2 + 8 a3 - 8 a4 + a5},
				 * {b10 + b6 + b7 + b8 + b9, -2 b10 + b7 - b8 + 2 b9, 4 b10 + b7 + b8 + 4 b9, -8 b10 + b11 + b7 - b8 + 8 b9},
				 * {c12 + c13 + c14 + c15 + c16, c13 - c14 + 2 c15 - 2 c16, c13 + c14 + 4 (c15 + c16), c13 - c14 + 8 c15 - 8 c16 + c17},
				 * {d18 + d19 + d20 + d21 + d22, d19 - d20 + 2 d21 - 2 d22, d19 + d20 + 4 (d21 + d22), d19 - d20 + 8 d21 - 8 d22 + d23}}
				 */
				float* bpz = bp + x * binc[0] + k;
				#pragma unroll 4
				for (dy = 0; dy < z[1]; dy++)
				{
					float r[] = {
						d[dy * 6 + 0] + d[dy * 6 + 1] + d[dy * 6 + 2] + d[dy * 6 + 3] + d[dy * 6 + 4] + biasval[k],
						d[dy * 6 + 1] - d[dy * 6 + 2] + 2 * (d[dy * 6 + 3] - d[dy * 6 + 4]) + biasval[k],
						d[dy * 6 + 1] + d[dy * 6 + 2] + 4 * (d[dy * 6 + 3] + d[dy * 6 + 4]) + biasval[k],
						d[dy * 6 + 1] - d[dy * 6 + 2] + 8 * (d[dy * 6 + 3] - d[dy * 6 + 4]) + d[dy * 6 + 5] + biasval[k],
					};
					#pragma unroll 4
					for (dx = 0; dx < z[0]; dx++)
						bpz[dx * binc[0]] = r[dx];
					bpz += binc[1] * binc[0];
				}
			}
		}
	} parallel_endfor
	ccfree(workmem);
	return CCV_NNC_EXEC_SUCCESS;
}

#ifdef HAVE_SSE2
inline static void _ccv_nnc_winograd_4x4_3x3_gwtg_sse2(const float* const w, const int* const dim, float* const gwtg)
{
	const int jump_dim = dim[3] / 4;
	const int dimCx4 = (dim[0] + 3) & -4;
	parallel_for(k, jump_dim) {
		int i, j;
		float* gwtgz = gwtg + k * 4 * 36 * dimCx4;
		const float* wz[] = {
			w + (k * 4) * 9 * dim[0],
			w + (k * 4 + 1) * 9 * dim[0],
			w + (k * 4 + 2) * 9 * dim[0],
			w + (k * 4 + 3) * 9 * dim[0],
		};
		for (i = 0; i < dim[0]; i++)
		{
			float x9w[9 * 4] __attribute__ ((__aligned__(16)));
			#pragma unroll 9
			for (j = 0; j < 9; j++)
			{
				x9w[j * 4] = wz[0][j * dim[0] + i];
				x9w[j * 4 + 1] = wz[1][j * dim[0] + i];
				x9w[j * 4 + 2] = wz[2][j * dim[0] + i];
				x9w[j * 4 + 3] = wz[3][j * dim[0] + i];
			}
			float g[18 * 4] __attribute__ ((__aligned__(16)));
			__m128 x9w0 = _mm_load_ps(x9w);
			__m128 x9w1 = _mm_load_ps(x9w + 4);
			__m128 x9w2 = _mm_load_ps(x9w + 8);
			__m128 x9w3 = _mm_load_ps(x9w + 12);
			__m128 x9w4 = _mm_load_ps(x9w + 16);
			__m128 x9w5 = _mm_load_ps(x9w + 20);
			__m128 x9w6 = _mm_load_ps(x9w + 24);
			__m128 x9w7 = _mm_load_ps(x9w + 28);
			__m128 x9w8 = _mm_load_ps(x9w + 32);
			/* row 1 */
			__m128 c1_4 = _mm_set1_ps(1.0 / 4);
			_mm_store_ps(g, _mm_mul_ps(x9w0, c1_4));
			_mm_store_ps(g + 4, _mm_mul_ps(x9w1, c1_4));
			_mm_store_ps(g + 8, _mm_mul_ps(x9w2, c1_4));
			/* row 2 */
			__m128 cn1_6 = _mm_set1_ps(-1.0 / 6);
			_mm_store_ps(g + 12, _mm_mul_ps(_mm_add_ps(_mm_add_ps(x9w0, x9w6), x9w3), cn1_6));
			_mm_store_ps(g + 16, _mm_mul_ps(_mm_add_ps(_mm_add_ps(x9w1, x9w7), x9w4), cn1_6));
			_mm_store_ps(g + 20, _mm_mul_ps(_mm_add_ps(_mm_add_ps(x9w2, x9w8), x9w5), cn1_6));
			/* row 3 */
			_mm_store_ps(g + 24, _mm_mul_ps(_mm_sub_ps(_mm_add_ps(x9w0, x9w6), x9w3), cn1_6));
			_mm_store_ps(g + 28, _mm_mul_ps(_mm_sub_ps(_mm_add_ps(x9w1, x9w7), x9w4), cn1_6));
			_mm_store_ps(g + 32, _mm_mul_ps(_mm_sub_ps(_mm_add_ps(x9w2, x9w8), x9w5), cn1_6));
			/* row 6 */
			_mm_store_ps(g + 60, x9w6);
			_mm_store_ps(g + 64, x9w7);
			_mm_store_ps(g + 68, x9w8);
			/* w[x] * 2 */
			x9w3 = _mm_add_ps(x9w3, x9w3);
			x9w4 = _mm_add_ps(x9w4, x9w4);
			x9w5 = _mm_add_ps(x9w5, x9w5);
			/* w[x] * 4 */
			x9w6 = _mm_add_ps(x9w6, x9w6);
			x9w6 = _mm_add_ps(x9w6, x9w6);
			x9w7 = _mm_add_ps(x9w7, x9w7);
			x9w7 = _mm_add_ps(x9w7, x9w7);
			x9w8 = _mm_add_ps(x9w8, x9w8);
			x9w8 = _mm_add_ps(x9w8, x9w8);
			/* row 4 */
			__m128 c1_24 = _mm_set1_ps(1.0 / 24);
			_mm_store_ps(g + 36, _mm_mul_ps(_mm_add_ps(_mm_add_ps(x9w0, x9w6), x9w3), c1_24));
			_mm_store_ps(g + 40, _mm_mul_ps(_mm_add_ps(_mm_add_ps(x9w1, x9w7), x9w4), c1_24));
			_mm_store_ps(g + 44, _mm_mul_ps(_mm_add_ps(_mm_add_ps(x9w2, x9w8), x9w5), c1_24));
			/* row 5 */
			_mm_store_ps(g + 48, _mm_mul_ps(_mm_sub_ps(_mm_add_ps(x9w0, x9w6), x9w3), c1_24));
			_mm_store_ps(g + 52, _mm_mul_ps(_mm_sub_ps(_mm_add_ps(x9w1, x9w7), x9w4), c1_24));
			_mm_store_ps(g + 56, _mm_mul_ps(_mm_sub_ps(_mm_add_ps(x9w2, x9w8), x9w5), c1_24));
			#pragma unroll 6
			for (j = 0; j < 6; j++)
			{
				const float* const gz = g + j * 12;
				float* const gwtgzu = gwtgz + j * 24 * dimCx4;
				__m128 g0 = _mm_load_ps(gz);
				__m128 g1 = _mm_load_ps(gz + 4);
				__m128 g2 = _mm_load_ps(gz + 8);
				_mm_store_ps(gwtgzu, _mm_mul_ps(g0, c1_4));
				_mm_store_ps(gwtgzu + 4 * dimCx4, _mm_mul_ps(_mm_add_ps(_mm_add_ps(g0, g2), g1), cn1_6));
				_mm_store_ps(gwtgzu + 8 * dimCx4, _mm_mul_ps(_mm_sub_ps(_mm_add_ps(g0, g2), g1), cn1_6));
				_mm_store_ps(gwtgzu + 20 * dimCx4, g2);
				/* g[1] * 2 */
				g1 = _mm_add_ps(g1, g1);
				/* g[2] * 4 */
				g2 = _mm_add_ps(g2, g2);
				g2 = _mm_add_ps(g2, g2);
				_mm_store_ps(gwtgzu + 12 * dimCx4, _mm_mul_ps(_mm_add_ps(_mm_add_ps(g0, g2), g1), c1_24));
				_mm_store_ps(gwtgzu + 16 * dimCx4, _mm_mul_ps(_mm_sub_ps(_mm_add_ps(g0, g2), g1), c1_24));
			}
			gwtgz += 4;
		}
	} parallel_endfor
}

static int _ccv_nnc_conv_forw_4x4_3x3_winograd_sse2(const ccv_nnc_tensor_view_t* a, const ccv_nnc_tensor_t* w, const ccv_nnc_tensor_t* bias, const ccv_nnc_hint_t hint, ccv_nnc_tensor_view_t* b)
{
	const int* ainc = CCV_IS_TENSOR_VIEW(a) ? a->inc : a->info.dim;
	const int* binc = CCV_IS_TENSOR_VIEW(b) ? b->inc : b->info.dim;
	assert(hint.border.begin[1] <= 1);
	assert(hint.border.begin[2] <= 1);
	assert(w->info.dim[3] % 4 == 0);
	assert(w->info.dim[1] == 3);
	assert(w->info.dim[2] == 3);
	const int jump_dim[CCV_NNC_MAX_DIM] = {
		(b->info.dim[1] + 3) / 4, (b->info.dim[2] + 3) / 4
	};
	const int dimCx4 = (a->info.dim[0] + 3) & -4;
	// allocating workspace memory for kernel reshaping and input reshaping.
	float* workmem = 0;
#if FOR_IS_PARALLEL
	// If we do parallel for, we need to allocate input reshaping for each block.
	ccmemalign((void **)&workmem, 16, sizeof(float) * (36 * dimCx4 * jump_dim[1] + 36 * dimCx4 * w->info.dim[3]));
#else
	// Otherwise, just one block.
	ccmemalign((void **)&workmem, 16, sizeof(float) * (36 * dimCx4 + 36 * dimCx4 * w->info.dim[3]));
#endif
	if (!workmem)
		return CCV_NNC_EXEC_OOM;
	// Convert w to a 6x6 matrix, by computing G.w.T(G) // T for transpose.
	float* const gwtg = workmem;
	float* const btdb = workmem + 36 * dimCx4 * w->info.dim[3];
	memset(gwtg, 0, sizeof(float) * 36 * dimCx4 * w->info.dim[3]);
	_ccv_nnc_winograd_4x4_3x3_gwtg_sse2(w->data.f32, w->info.dim, gwtg);
	// kernel weight for one dim.
	const float* const biasval = bias->data.f32;
	// Workaround issues of dispatch_apply (cannot reference to on-stack array)
	const int tile_dim_s[CCV_NNC_MAX_DIM_ALLOC] = {
		w->info.dim[0], 6, 6, w->info.dim[3]
	};
	const int* const tile_dim = tile_dim_s;
	// This block will be cause in each for-loop, therefore, you can use it to generate some temporary variables.
	parallel_for(i, jump_dim[1]) {
		const int y = i * 4; // i is unsigned.
		int j, x, k, c;
		int n[CCV_NNC_MAX_DIM];
		int m[CCV_NNC_MAX_DIM];
		int z[CCV_NNC_MAX_DIM];
		set_n_m_dim(y, 1, tile_dim, a->info.dim);
		z[1] = ccv_min(y + 4, b->info.dim[2]) - y;
		const float* ap = a->data.f32 + ccv_max(y - hint.border.begin[2], 0) * ainc[1] * ainc[0];
		float* bp = b->data.f32 + y * binc[1] * binc[0];
		for (x = 0; x < b->info.dim[1]; x += 4)
		{
			set_n_m_dim(x, 0, tile_dim, a->info.dim);
			z[0] = ccv_min(x + 4, b->info.dim[1]) - x;
#if FOR_IS_PARALLEL
			float* g = btdb + i * 36 * dimCx4;
#else
			float* g = btdb;
#endif
			// zero g such that we can have zero-padding.
			memset(g, 0, sizeof(float) * 36 * dimCx4);
			int dx, dy;
			const float* apz = ap + ccv_max(x - hint.border.begin[1], 0) * ainc[0];
			float* gz = g + (n[1] * 6 + n[0]) * dimCx4;
			#pragma unroll 6
			for (dy = 0; dy < m[1]; dy++)
			{
				#pragma unroll 6
				for (dx = 0; dx < m[0]; dx++)
				{
					float* const gzu = gz + (dy * 6 + dx) * dimCx4;
					for (c = 0; c < a->info.dim[0]; c++)
						gzu[c] = apz[dx * ainc[0] + c];
				}
				apz += ainc[1] * ainc[0];
			}
			for (c = 0; c < a->info.dim[0]; c += 4)
			{
				float d[36 * 4]  __attribute__ ((__aligned__(16)));
				/* BT.d */
				#pragma unroll 6
				for (j = 0; j < 6; j++)
				{
					/* row 1 */
					const float* const gz = g + j * dimCx4;
					float* dz = d + j * 4;
					__m128 g0 = _mm_load_ps(gz);
					__m128 g12 = _mm_load_ps(gz + 12 * dimCx4);
					__m128 g18 = _mm_load_ps(gz + 18 * dimCx4);
					__m128 g24 = _mm_load_ps(gz + 24 * dimCx4);
					g0 = _mm_add_ps(g0, g0);
					g0 = _mm_add_ps(g0, g0);
					__m128 g12x2 = _mm_add_ps(g12, g12);
					g12x2 = _mm_add_ps(g12x2, g12x2);
					g12x2 = _mm_add_ps(g12x2, g12);
					_mm_store_ps(dz, _mm_sub_ps(_mm_add_ps(g0, g24), g12x2));
					/* row 2 */
					__m128 g6 = _mm_load_ps(gz + 6 * dimCx4);
					__m128 g6x12 = _mm_add_ps(g6, g12);
					g6x12 = _mm_add_ps(g6x12, g6x12);
					g6x12 = _mm_add_ps(g6x12, g6x12);
					_mm_store_ps(dz + 24, _mm_sub_ps(_mm_add_ps(g18, g24), g6x12));
					/* row 3 */
					g6x12 = _mm_sub_ps(g6, g12);
					g6x12 = _mm_add_ps(g6x12, g6x12);
					g6x12 = _mm_add_ps(g6x12, g6x12);
					_mm_store_ps(dz + 48, _mm_add_ps(_mm_sub_ps(g24, g18), g6x12));
					/* row 4 */
					__m128 g18x6 = _mm_sub_ps(g18, g6);
					g18x6 = _mm_add_ps(g18x6, g18x6);
					_mm_store_ps(dz + 72, _mm_add_ps(_mm_sub_ps(g24, g12), g18x6));
					/* row 5 */
					_mm_store_ps(dz + 96, _mm_sub_ps(_mm_sub_ps(g24, g12), g18x6));
					/* row 6 */
					__m128 g30 = _mm_load_ps(gz + 30 * dimCx4);
					__m128 g18x2 = _mm_add_ps(g18, g18);
					g18x2 = _mm_add_ps(g18x2, g18x2);
					g18x2 = _mm_add_ps(g18, g18x2);
					g6 = _mm_add_ps(g6, g6);
					g6 = _mm_add_ps(g6, g6);
					_mm_store_ps(dz + 120, _mm_sub_ps(_mm_add_ps(g6, g30), g18x2));
				}
				/* BT.d.B */
				#pragma unroll 6
				for (j = 0; j < 6; j++)
				{
					float* gz = g + j * 6 * dimCx4;
					const float* const dz = d + j * 24;
					__m128 d0 = _mm_load_ps(dz);
					__m128 d1 = _mm_load_ps(dz + 4);
					__m128 d2 = _mm_load_ps(dz + 8);
					__m128 d3 = _mm_load_ps(dz + 12);
					__m128 d4 = _mm_load_ps(dz + 16);
					__m128 d5 = _mm_load_ps(dz + 20);
					d0 = _mm_add_ps(d0, d0);
					d0 = _mm_add_ps(d0, d0);
					__m128 d2x5 = _mm_add_ps(d2, d2);
					d2x5 = _mm_add_ps(d2x5, d2x5);
					d2x5 = _mm_add_ps(d2, d2x5);
					_mm_store_ps(gz, _mm_sub_ps(_mm_add_ps(d0, d4), d2x5));
					__m128 d1x2 = _mm_add_ps(d1, d2);
					d1x2 = _mm_add_ps(d1x2, d1x2);
					d1x2 = _mm_add_ps(d1x2, d1x2);
					_mm_store_ps(gz + dimCx4, _mm_sub_ps(_mm_add_ps(d3, d4), d1x2));
					d1x2 = _mm_sub_ps(d1, d2);
					d1x2 = _mm_add_ps(d1x2, d1x2);
					d1x2 = _mm_add_ps(d1x2, d1x2);
					_mm_store_ps(gz + 2 * dimCx4, _mm_add_ps(_mm_sub_ps(d4, d3), d1x2));
					__m128 d3x1 = _mm_sub_ps(d3, d1);
					d3x1 = _mm_add_ps(d3x1, d3x1);
					_mm_store_ps(gz + 3 * dimCx4, _mm_add_ps(_mm_sub_ps(d4, d2), d3x1));
					_mm_store_ps(gz + 4 * dimCx4, _mm_sub_ps(_mm_sub_ps(d4, d2), d3x1));
					d1 = _mm_add_ps(d1, d1);
					d1 = _mm_add_ps(d1, d1);
					__m128 d3x5 = _mm_add_ps(d3, d3);
					d3x5 = _mm_add_ps(d3x5, d3x5);
					d3x5 = _mm_add_ps(d3, d3x5);
					_mm_store_ps(gz + 5 * dimCx4, _mm_sub_ps(_mm_add_ps(d1, d5), d3x5));
				}
				// move to the next channel
				g += 4;
			}
			const float* wpz = gwtg;
			for (k = 0; k < w->info.dim[3]; k += 4)
			{
				float q[36 * 4] __attribute__ ((__aligned__(16)));
#if FOR_IS_PARALLEL
				g = btdb + i * 36 * dimCx4;
#else
				g = btdb;
#endif
				for (j = 0; j < 36; j++)
				{
					__m128 v40 = _mm_setzero_ps();
					__m128 v41 = _mm_setzero_ps();
					__m128 v42 = _mm_setzero_ps();
					__m128 v43 = _mm_setzero_ps();
					for (c = 0; c < a->info.dim[0]; c += 4)
					{
						__m128 g4 = _mm_load_ps(g);
						__m128 w40 = _mm_load_ps(wpz);
						__m128 w41 = _mm_load_ps(wpz + 4);
						__m128 w42 = _mm_load_ps(wpz + 8);
						__m128 w43 = _mm_load_ps(wpz + 12);
						__m128 g40 = _mm_shuffle_ps(g4, g4, 0x00);
						__m128 g41 = _mm_shuffle_ps(g4, g4, 0x55);
						__m128 g42 = _mm_shuffle_ps(g4, g4, 0xAA);
						__m128 g43 = _mm_shuffle_ps(g4, g4, 0xFF);
						v40 = _mm_add_ps(_mm_mul_ps(w40, g40), v40);
						v41 = _mm_add_ps(_mm_mul_ps(w41, g41), v41);
						v42 = _mm_add_ps(_mm_mul_ps(w42, g42), v42);
						v43 = _mm_add_ps(_mm_mul_ps(w43, g43), v43);
						g += 4;
						wpz += 16;
					}
					v40 = _mm_add_ps(v40, v41);
					v42 = _mm_add_ps(v42, v43);
					_mm_store_ps(q + j * 4, _mm_add_ps(v40, v42));
				}
				float d[24 * 4] __attribute__ ((__aligned__(16)));
				#pragma unroll 6
				for (j = 0; j < 6; j++)
				{
					const float* const qz = q + j * 4;
					float* const dz = d + j * 4;
					__m128 q0 = _mm_load_ps(qz);
					__m128 q6 = _mm_load_ps(qz + 24);
					__m128 q12 = _mm_load_ps(qz + 48);
					__m128 q18 = _mm_load_ps(qz + 72);
					__m128 q24 = _mm_load_ps(qz + 96);
					__m128 qs6x12 = _mm_add_ps(q6, q12);
					__m128 qs18x24 = _mm_add_ps(q18, q24);
					__m128 qss = _mm_add_ps(qs6x12, q0);
					/* row 1 */
					_mm_store_ps(dz, _mm_add_ps(qss, qs18x24));
					__m128 qn6x12 = _mm_sub_ps(q6, q12);
					__m128 qn18x24 = _mm_sub_ps(q18, q24);
					qn18x24 = _mm_add_ps(qn18x24, qn18x24);
					/* row 2 */
					_mm_store_ps(dz + 24, _mm_add_ps(qn6x12, qn18x24));
					qs18x24 = _mm_add_ps(qs18x24, qs18x24);
					qs18x24 = _mm_add_ps(qs18x24, qs18x24);
					/* row 3 */
					_mm_store_ps(dz + 48, _mm_add_ps(qs6x12, qs18x24));
					qn18x24 = _mm_add_ps(qn18x24, qn18x24);
					qn18x24 = _mm_add_ps(qn18x24, qn18x24);
					__m128 q30 = _mm_load_ps(qz + 120);
					/* row 4 */
					_mm_store_ps(dz + 72, _mm_add_ps(_mm_add_ps(qn6x12, q30), qn18x24));
				}
				float* bpz = bp + x * binc[0] + k;
				__m128 bias4 = _mm_loadu_ps(biasval + k);
				switch (z[0]) {
					case 1:
						#pragma unroll 4
						for (dy = 0; dy < z[1]; dy++)
						{
							const float* const dz = d + dy * 24;
							__m128 d0 = _mm_load_ps(dz);
							__m128 d1 = _mm_load_ps(dz + 4);
							__m128 d2 = _mm_load_ps(dz + 8);
							__m128 d3 = _mm_load_ps(dz + 12);
							__m128 d4 = _mm_load_ps(dz + 16);
							__m128 ds1x2 = _mm_add_ps(d1, d2);
							__m128 ds3x4 = _mm_add_ps(d3, d4);
							ds1x2 = _mm_add_ps(ds1x2, bias4);
							_mm_stream_ps(bpz, _mm_add_ps(ds1x2, _mm_add_ps(d0, ds3x4)));
							bpz += binc[1] * binc[0];
						}
						break;
					case 2:
						#pragma unroll 4
						for (dy = 0; dy < z[1]; dy++)
						{
							const float* const dz = d + dy * 24;
							__m128 d0 = _mm_load_ps(dz);
							__m128 d1 = _mm_load_ps(dz + 4);
							__m128 d2 = _mm_load_ps(dz + 8);
							__m128 d3 = _mm_load_ps(dz + 12);
							__m128 d4 = _mm_load_ps(dz + 16);
							__m128 ds1x2 = _mm_add_ps(d1, d2);
							__m128 ds3x4 = _mm_add_ps(d3, d4);
							ds1x2 = _mm_add_ps(ds1x2, bias4);
							_mm_stream_ps(bpz, _mm_add_ps(ds1x2, _mm_add_ps(d0, ds3x4)));
							__m128 dn1x2 = _mm_sub_ps(d1, d2);
							__m128 dn3x4 = _mm_sub_ps(d3, d4);
							dn3x4 = _mm_add_ps(dn3x4, dn3x4);
							dn1x2 = _mm_add_ps(dn1x2, bias4);
							_mm_stream_ps(bpz + binc[0], _mm_add_ps(dn1x2, dn3x4));
							bpz += binc[1] * binc[0];
						}
						break;
					case 3:
						#pragma unroll 4
						for (dy = 0; dy < z[1]; dy++)
						{
							const float* const dz = d + dy * 24;
							__m128 d0 = _mm_load_ps(dz);
							__m128 d1 = _mm_load_ps(dz + 4);
							__m128 d2 = _mm_load_ps(dz + 8);
							__m128 d3 = _mm_load_ps(dz + 12);
							__m128 d4 = _mm_load_ps(dz + 16);
							__m128 ds1x2 = _mm_add_ps(d1, d2);
							__m128 ds3x4 = _mm_add_ps(d3, d4);
							ds1x2 = _mm_add_ps(ds1x2, bias4);
							_mm_stream_ps(bpz, _mm_add_ps(ds1x2, _mm_add_ps(d0, ds3x4)));
							__m128 dn1x2 = _mm_sub_ps(d1, d2);
							__m128 dn3x4 = _mm_sub_ps(d3, d4);
							dn3x4 = _mm_add_ps(dn3x4, dn3x4);
							dn1x2 = _mm_add_ps(dn1x2, bias4);
							_mm_stream_ps(bpz + binc[0], _mm_add_ps(dn1x2, dn3x4));
							ds3x4 = _mm_add_ps(ds3x4, ds3x4);
							ds3x4 = _mm_add_ps(ds3x4, ds3x4);
							_mm_stream_ps(bpz + 2 * binc[0], _mm_add_ps(ds1x2, ds3x4));
							bpz += binc[1] * binc[0];
						}
						break;
					case 4:
						#pragma unroll 4
						for (dy = 0; dy < z[1]; dy++)
						{
							const float* const dz = d + dy * 24;
							__m128 d0 = _mm_load_ps(dz);
							__m128 d1 = _mm_load_ps(dz + 4);
							__m128 d2 = _mm_load_ps(dz + 8);
							__m128 d3 = _mm_load_ps(dz + 12);
							__m128 d4 = _mm_load_ps(dz + 16);
							__m128 ds1x2 = _mm_add_ps(d1, d2);
							__m128 ds3x4 = _mm_add_ps(d3, d4);
							ds1x2 = _mm_add_ps(ds1x2, bias4);
							_mm_stream_ps(bpz, _mm_add_ps(ds1x2, _mm_add_ps(d0, ds3x4)));
							__m128 dn1x2 = _mm_sub_ps(d1, d2);
							__m128 dn3x4 = _mm_sub_ps(d3, d4);
							dn3x4 = _mm_add_ps(dn3x4, dn3x4);
							dn1x2 = _mm_add_ps(dn1x2, bias4);
							_mm_stream_ps(bpz + binc[0], _mm_add_ps(dn1x2, dn3x4));
							ds3x4 = _mm_add_ps(ds3x4, ds3x4);
							ds3x4 = _mm_add_ps(ds3x4, ds3x4);
							_mm_stream_ps(bpz + 2 * binc[0], _mm_add_ps(ds1x2, ds3x4));
							__m128 d5 = _mm_load_ps(dz + 20);
							dn3x4 = _mm_add_ps(dn3x4, dn3x4);
							dn3x4 = _mm_add_ps(dn3x4, dn3x4);
							_mm_stream_ps(bpz + 3 * binc[0], _mm_add_ps(_mm_add_ps(dn1x2, d5), dn3x4));
							bpz += binc[1] * binc[0];
						}
						break;
				};
			}
		}
	} parallel_endfor
	ccfree(workmem);
	return CCV_NNC_EXEC_SUCCESS;
}
#endif

#ifndef HAVE_NEON
#endif

#ifdef HAVE_SSE2
inline static void _ccv_nnc_x4w_sse2(const float* const w, const int* const dim, float* x4w)
{
	int jump_dim = dim[3] / 4;
	parallel_for(k, jump_dim) {
		int i, j;
		float* x4wz = x4w + k * dim[2] * dim[1] * dim[0] * 4;
		const float* wz[] = {
			w + (k * 4) * dim[2] * dim[1] * dim[0],
			w + (k * 4 + 1) * dim[2] * dim[1] * dim[0],
			w + (k * 4 + 2) * dim[2] * dim[1] * dim[0],
			w + (k * 4 + 3) * dim[2] * dim[1] * dim[0],
		};
		for (i = 0; i < dim[2] * dim[1]; i++)
		{
			for (j = 0; j < dim[0]; j++)
			{
				x4wz[j * 4] = wz[0][j];
				x4wz[j * 4 + 1] = wz[1][j];
				x4wz[j * 4 + 2] = wz[2][j];
				x4wz[j * 4 + 3] = wz[3][j];
			}
			x4wz += dim[0] * 4;
			wz[0] += dim[0];
			wz[1] += dim[0];
			wz[2] += dim[0];
			wz[3] += dim[0];
		}
	} parallel_endfor
}

static int _ccv_nnc_conv_forw_sse2(const ccv_nnc_tensor_view_t* a, const ccv_nnc_tensor_t* w, const ccv_nnc_tensor_t* bias, const ccv_nnc_hint_t hint, ccv_nnc_tensor_view_t* b)
{
	const int* ainc = CCV_IS_TENSOR_VIEW(a) ? a->inc : a->info.dim;
	const int* binc = CCV_IS_TENSOR_VIEW(b) ? b->inc : b->info.dim;
	assert(w->info.dim[3] % 4 == 0);
	float* x4w = 0;
	ccmemalign((void **)&x4w, 16, sizeof(float) * w->info.dim[3] * w->info.dim[2] * w->info.dim[1] * w->info.dim[0]);
	if (!x4w)
		return CCV_NNC_EXEC_OOM;
	_ccv_nnc_x4w_sse2(w->data.f32, w->info.dim, x4w);
	int jump_dim = w->info.dim[3] / 4;
	// Do naive tail partition unroll
#define main_for(tail_block) \
	parallel_for(k, jump_dim) { \
		int c; \
		const float* ap = a->data.f32; \
		float* bp = b->data.f32 + k * 4; \
		/* kernel weight for one dim. */ \
		const float* const x4wp = x4w + k * 4 * w->info.dim[0] * w->info.dim[1] * w->info.dim[2]; \
		const float biasval[4] __attribute__ ((__aligned__(16))) = { \
			bias->data.f32[k * 4], \
			bias->data.f32[k * 4 + 1], \
			bias->data.f32[k * 4 + 2], \
			bias->data.f32[k * 4 + 3] \
		}; \
		/* This block will be cause in each for-loop, therefore, you can use it to generate some temporary variables. */ \
		int i[CCV_NNC_MAX_DIM]; \
		int n[CCV_NNC_MAX_DIM]; \
		int m[CCV_NNC_MAX_DIM]; \
		int j[CCV_NNC_MAX_DIM]; \
		for (i[1] = 0; i[1] < b->info.dim[2]; i[1]++) \
		{ \
			set_n_m_dim(i[1], 1, w->info.dim, a->info.dim); \
			const float* wpu = x4wp + n[1] * w->info.dim[1] * w->info.dim[0] * 4; \
			for (i[0] = 0; i[0] < b->info.dim[1]; i[0]++) \
			{ \
				set_n_m_dim(i[0], 0, w->info.dim, a->info.dim); \
				__m128 v40 = _mm_load_ps(biasval); \
				__m128 v41 = _mm_setzero_ps(); \
				__m128 v42 = _mm_setzero_ps(); \
				__m128 v43 = _mm_setzero_ps(); \
				const float* wpz = wpu + n[0] * w->info.dim[0] * 4; \
				const float* apz = ap + ccv_max(i[0] * hint.stride.dim[1] - hint.border.begin[1], 0) * ainc[0]; \
				for (j[1] = 0; j[1] < m[1]; j[1]++) \
				{ \
					for (j[0] = 0; j[0] < m[0]; j[0]++) \
					{ \
						for (c = 0; c < a->info.dim[0] - 3; c += 4) \
						{ \
							__m128 apz4 = _mm_loadu_ps(apz + j[0] * ainc[0] + c); \
							const float* const wpzu = wpz + (j[0] * w->info.dim[0] + c) * 4; \
							__m128 w40 = _mm_loadu_ps(wpzu); \
							__m128 w41 = _mm_loadu_ps(wpzu + 4); \
							__m128 w42 = _mm_loadu_ps(wpzu + 8); \
							__m128 w43 = _mm_loadu_ps(wpzu + 12); \
							__m128 apz40 = _mm_shuffle_ps(apz4, apz4, 0x00); \
							__m128 apz41 = _mm_shuffle_ps(apz4, apz4, 0x55); \
							__m128 apz42 = _mm_shuffle_ps(apz4, apz4, 0xAA); \
							__m128 apz43 = _mm_shuffle_ps(apz4, apz4, 0xFF); \
							v40 =_mm_add_ps(_mm_mul_ps(w40, apz40), v40); \
							v41 =_mm_add_ps(_mm_mul_ps(w41, apz41), v41); \
							v42 =_mm_add_ps(_mm_mul_ps(w42, apz42), v42); \
							v43 =_mm_add_ps(_mm_mul_ps(w43, apz43), v43); \
						} \
						tail_block /* insert executions for tail partition */ \
					} \
					wpz += w->info.dim[1] * w->info.dim[0] * 4; \
					apz += ainc[1] * ainc[0]; \
				} \
				__m128 v4 = _mm_add_ps(_mm_add_ps(v40, v41), _mm_add_ps(v42, v43)); \
				_mm_stream_ps(bp + i[0] * binc[0], v4); \
			} \
			bp += binc[1] * binc[0]; \
			ap += ainc[1] * ainc[0] * (ccv_max((i[1] + 1) * hint.stride.dim[2] - hint.border.begin[2], 0) - ccv_max(i[1] * hint.stride.dim[2] - hint.border.begin[2], 0)); \
		} \
	} parallel_endfor
	if (w->info.dim[0] % 4 == 0)
	{
		main_for();
	} else if (w->info.dim[0] % 4 == 3) { // unroll the last for-loops
#define tail_block \
		__m128 apz40 = _mm_load1_ps(apz + j[0] * ainc[0] + c); \
		__m128 apz41 = _mm_load1_ps(apz + j[0] * ainc[0] + c + 1); \
		__m128 apz42 = _mm_load1_ps(apz + j[0] * ainc[0] + c + 2); \
		const float* const wpzu = wpz + (j[0] * w->info.dim[0] + c) * 4; \
		__m128 w40 = _mm_loadu_ps(wpzu); \
		__m128 w41 = _mm_loadu_ps(wpzu + 4); \
		__m128 w42 = _mm_loadu_ps(wpzu + 8); \
		v40 = _mm_add_ps(_mm_mul_ps(w40, apz40), v40); \
		v41 = _mm_add_ps(_mm_mul_ps(w41, apz41), v41); \
		v42 = _mm_add_ps(_mm_mul_ps(w42, apz42), v42);
		main_for(tail_block);
#undef tail_block
	} else if (w->info.dim[0] % 4 == 2) { // unroll the last for-loops
#define tail_block \
		__m128 apz40 = _mm_load1_ps(apz + j[0] * ainc[0] + c); \
		__m128 apz41 = _mm_load1_ps(apz + j[0] * ainc[0] + c + 1); \
		const float* const wpzu = wpz + (j[0] * w->info.dim[0] + c) * 4; \
		__m128 w40 = _mm_loadu_ps(wpzu); \
		__m128 w41 = _mm_loadu_ps(wpzu + 4); \
		v40 = _mm_add_ps(_mm_mul_ps(w40, apz40), v40); \
		v41 = _mm_add_ps(_mm_mul_ps(w41, apz41), v41);
		main_for(tail_block);
#undef tail_block
	} else {
#define tail_block \
		__m128 apz4 = _mm_load1_ps(apz + j[0] * ainc[0] + c); \
		const float* const wpzu = wpz + (j[0] * w->info.dim[0] + c) * 4; \
		__m128 w4 = _mm_loadu_ps(wpzu); \
		v40 = _mm_add_ps(_mm_mul_ps(w4, apz4), v40);
		main_for(tail_block);
#undef tail_block
	}
#undef main_for
	ccfree(x4w);
	return CCV_NNC_EXEC_SUCCESS;
}
#endif

#ifdef HAVE_NEON
inline static void _ccv_nnc_x4w_neon(const float* const w, const int* const dim, float* x4w)
{
	int jump_dim = dim[3] / 4;
	parallel_for(k, jump_dim) {
		int i, j;
		float* x4wz = x4w + k * dim[2] * dim[1] * dim[0] * 4;
		const float* wz[] = {
			w + (k * 4) * dim[2] * dim[1] * dim[0],
			w + (k * 4 + 1) * dim[2] * dim[1] * dim[0],
			w + (k * 4 + 2) * dim[2] * dim[1] * dim[0],
			w + (k * 4 + 3) * dim[2] * dim[1] * dim[0],
		};
		for (i = 0; i < dim[2] * dim[1]; i++)
		{
			for (j = 0; j < dim[0]; j++)
			{
				x4wz[j * 4] = wz[0][j];
				x4wz[j * 4 + 1] = wz[1][j];
				x4wz[j * 4 + 2] = wz[2][j];
				x4wz[j * 4 + 3] = wz[3][j];
			}
			x4wz += dim[0] * 4;
			wz[0] += dim[0];
			wz[1] += dim[0];
			wz[2] += dim[0];
			wz[3] += dim[0];
		}
	} parallel_endfor
}

static int _ccv_nnc_conv_forw_neon(const ccv_nnc_tensor_view_t* a, const ccv_nnc_tensor_t* w, const ccv_nnc_tensor_t* bias, const ccv_nnc_hint_t hint, ccv_nnc_tensor_view_t* b)
{
	const int* ainc = CCV_IS_TENSOR_VIEW(a) ? a->inc : a->info.dim;
	const int* binc = CCV_IS_TENSOR_VIEW(b) ? b->inc : b->info.dim;
	assert(w->info.dim[3] % 4 == 0);
	float* x4w = 0;
	ccmemalign((void **)&x4w, 16, sizeof(float) * w->info.dim[3] * w->info.dim[2] * w->info.dim[1] * w->info.dim[0]);
	if (!x4w)
		return CCV_NNC_EXEC_OOM;
	_ccv_nnc_x4w_neon(w->data.f32, w->info.dim, x4w);
	int jump_dim = w->info.dim[3] / 4;
#define main_for(tail_block) \
	parallel_for(k, jump_dim) { \
		int c; \
		const float* ap = a->data.f32; \
		float* bp = b->data.f32 + k * 4; \
		/* kernel weight for one dim. */ \
		const float* const x4wp = x4w + k * 4 * w->info.dim[0] * w->info.dim[1] * w->info.dim[2]; \
		const float biasval[4] __attribute__ ((__aligned__(16))) = { \
			bias->data.f32[k * 4], \
			bias->data.f32[k * 4 + 1], \
			bias->data.f32[k * 4 + 2], \
			bias->data.f32[k * 4 + 3] \
		}; \
		/* This block will be cause in each for-loop, therefore, you can use it to generate some temporary variables. */ \
		int i[CCV_NNC_MAX_DIM]; \
		int n[CCV_NNC_MAX_DIM]; \
		int m[CCV_NNC_MAX_DIM]; \
		int j[CCV_NNC_MAX_DIM]; \
		for (i[1] = 0; i[1] < b->info.dim[2]; i[1]++) \
		{ \
			set_n_m_dim(i[1], 1, w->info.dim, a->info.dim); \
			const float* wpu = x4wp + n[1] * w->info.dim[1] * w->info.dim[0] * 4; \
			for (i[0] = 0; i[0] < b->info.dim[1]; i[0]++) \
			{ \
				set_n_m_dim(i[0], 0, w->info.dim, a->info.dim); \
				float32x4_t v40 = vld1q_f32(biasval); \
				float32x4_t v41 = vmovq_n_f32(0); \
				float32x4_t v42 = vmovq_n_f32(0); \
				float32x4_t v43 = vmovq_n_f32(0); \
				const float* wpz = wpu + n[0] * w->info.dim[0] * 4; \
				const float* apz = ap + ccv_max(i[0] * hint.stride.dim[1] - hint.border.begin[1], 0) * ainc[0]; \
				for (j[1] = 0; j[1] < m[1]; j[1]++) \
				{ \
					for (j[0] = 0; j[0] < m[0]; j[0]++) \
					{ \
						for (c = 0; c < a->info.dim[0] - 3; c += 4) \
						{ \
							float32x2x2_t apz4 = vld2_f32(apz + j[0] * ainc[0] + c); \
							const float* const wpzu = wpz + (j[0] * w->info.dim[0] + c) * 4; \
							float32x4_t apz40 = vdupq_lane_f32(apz4.val[0], 0); \
							float32x4_t apz41 = vdupq_lane_f32(apz4.val[0], 1); \
							float32x4_t apz42 = vdupq_lane_f32(apz4.val[1], 0); \
							float32x4_t apz43 = vdupq_lane_f32(apz4.val[1], 1); \
							float32x4_t w40 = vld1q_f32(wpzu); \
							float32x4_t w41 = vld1q_f32(wpzu + 4); \
							float32x4_t w42 = vld1q_f32(wpzu + 8); \
							float32x4_t w43 = vld1q_f32(wpzu + 12); \
							v40 = vmlaq_f32(v40, w40, apz40); \
							v41 = vmlaq_f32(v41, w41, apz41); \
							v42 = vmlaq_f32(v42, w42, apz42); \
							v43 = vmlaq_f32(v43, w43, apz43); \
						} \
						tail_block /* insert executions for tail partition */ \
					} \
					wpz += w->info.dim[1] * w->info.dim[0] * 4; \
					apz += ainc[1] * ainc[0]; \
				} \
				v40 = vaddq_f32(v40, v41); \
				v42 = vaddq_f32(v42, v43); \
				vst1q_f32(bp + i[0] * binc[0], vaddq_f32(v40, v42)); \
			} \
			bp += binc[1] * binc[0]; \
			ap += ainc[1] * ainc[0] * (ccv_max((i[1] + 1) * hint.stride.dim[2] - hint.border.begin[2], 0) - ccv_max(i[1] * hint.stride.dim[2] - hint.border.begin[2], 0)); \
		} \
	} parallel_endfor
	if (w->info.dim[0] % 4 == 0)
	{
		main_for();
	} else if (w->info.dim[0] % 4 == 3) { // unroll the last for-loops
#define tail_block \
		float32x2_t apz4 = vld1_f32(apz + j[0] * ainc[0] + c); \
		const float* const wpzu = wpz + (j[0] * w->info.dim[0] + c) * 4; \
		float32x4_t apz40 = vdupq_lane_f32(apz4, 0); \
		float32x4_t apz41 = vdupq_lane_f32(apz4, 1); \
		float32x4_t apz42 = vld1q_dup_f32(apz + j[0] * ainc[0] + c + 2); \
		float32x4_t w40 = vld1q_f32(wpzu); \
		float32x4_t w41 = vld1q_f32(wpzu + 4); \
		float32x4_t w42 = vld1q_f32(wpzu + 8); \
		v40 = vmlaq_f32(v40, w40, apz40); \
		v41 = vmlaq_f32(v41, w41, apz41); \
		v42 = vmlaq_f32(v42, w42, apz42);
		main_for(tail_block);
#undef tail_block
	} else if (w->info.dim[0] % 4 == 2) { // unroll the last for-loops
#define tail_block \
		float32x2_t apz4 = vld1_f32(apz + j[0] * ainc[0] + c); \
		const float* const wpzu = wpz + (j[0] * w->info.dim[0] + c) * 4; \
		float32x4_t apz40 = vdupq_lane_f32(apz4, 0); \
		float32x4_t apz41 = vdupq_lane_f32(apz4, 1); \
		float32x4_t w40 = vld1q_f32(wpzu); \
		float32x4_t w41 = vld1q_f32(wpzu + 4); \
		v40 = vmlaq_f32(v40, w40, apz40); \
		v41 = vmlaq_f32(v41, w41, apz41);
		main_for(tail_block);
#undef tail_block
	} else { // unroll the last for-loops
#define tail_block \
		float32x4_t apz4 = vld1q_dup_f32(apz + j[0] * ainc[0] + c); \
		const float* const wpzu = wpz + (j[0] * w->info.dim[0] + c) * 4; \
		float32x4_t w4 = vld1q_f32(wpzu); \
		v40 = vmlaq_f32(v40, w4, apz4);
		main_for(tail_block);
#undef tail_block
	}
#undef main_for
	ccfree(x4w);
	return CCV_NNC_EXEC_SUCCESS;
}
#endif

static int _ccv_nnc_conv_forw_4x4_3x3_winograd(const ccv_nnc_tensor_view_t* a, const ccv_nnc_tensor_t* w, const ccv_nnc_tensor_t* bias, const ccv_nnc_hint_t hint, ccv_nnc_tensor_view_t* b)
{
#if defined(HAVE_SSE2)
	if (w->info.dim[3] % 4 == 0)
		return _ccv_nnc_conv_forw_4x4_3x3_winograd_sse2(a, w, bias, hint, b);
#elif defined(HAVE_NEON)
#endif
	return _ccv_nnc_conv_forw_4x4_3x3_winograd_ref(a, w, bias, hint, b);
}

static int _ccv_nnc_conv_forw(const ccv_nnc_cmd_t cmd, const ccv_nnc_hint_t hint, const int flags, ccv_nnc_tensor_t* const* inputs, const int input_size, ccv_nnc_tensor_t** outputs, const int output_size)
{
	assert(input_size == 3);
	const ccv_nnc_tensor_view_t* a = (ccv_nnc_tensor_view_t*)inputs[0];
	const ccv_nnc_tensor_t* w = inputs[1];
	assert(!CCV_IS_TENSOR_VIEW(w));
	const ccv_nnc_tensor_t* bias = inputs[2];
	assert(!CCV_IS_TENSOR_VIEW(bias));
	assert(output_size == 1);
	ccv_nnc_tensor_view_t* b = (ccv_nnc_tensor_view_t*)outputs[0];
	assert(w->info.dim[0] == cmd.info.size.dim[0]);
	assert(w->info.dim[0] == a->info.dim[0]);
	assert(b->info.dim[0] == cmd.info.convolutional.count);
	int i;
	// Make sure the weights dimension matches the network dimension
	for (i = 1; i < CCV_NNC_MAX_DIM_ALLOC; i++)
	{
		if (w->info.dim[i] == 0 || cmd.info.size.dim[i] == 0)
			break;
		assert(w->info.dim[i] == cmd.info.size.dim[i]);
	}
	// Make sure the weights output dimension matches the network convolutional kernels
	for (i = CCV_NNC_MAX_DIM_ALLOC - 1; i > 0; i--)
		if (w->info.dim[i] == 0 && w->info.dim[i])
		{
			assert(w->info.dim[i] == cmd.info.convolutional.count);
			break;
		}
	switch (cmd.algorithm)
	{
		case CCV_NNC_CMD_OPT_CONV_ALGO_DC:
#if defined(HAVE_SSE2)
			if (w->info.dim[3] % 4 == 0)
				return _ccv_nnc_conv_forw_sse2(a, w, bias, hint, b);
#elif defined(HAVE_NEON)
			if (w->info.dim[3] % 4 == 0)
				return _ccv_nnc_conv_forw_neon(a, w, bias, hint, b);
#endif
			return CCV_NNC_EXEC_INVALID;
		case CCV_NNC_CMD_OPT_CONV_ALGO_GEMM:
			if (w->info.dim[1] == 1 && w->info.dim[1] == 1 && hint.stride.dim[1] <= 1 && hint.stride.dim[2] <= 1)
				return CCV_NNC_EXEC_INVALID; // Placeholder, for gemm call.
			return CCV_NNC_EXEC_INVALID;
		case CCV_NNC_CMD_OPT_CONV_ALGO_WINOGRAD:
			if (w->info.dim[1] == 3 && w->info.dim[2] == 3 && hint.stride.dim[1] <= 1 && hint.stride.dim[2] <= 1)
				return _ccv_nnc_conv_forw_4x4_3x3_winograd(a, w, bias, hint, b);
			return CCV_NNC_EXEC_INVALID;
		case CCV_NNC_CMD_OPT_CONV_ALGO_FFT:
			return CCV_NNC_EXEC_INVALID; // Placeholder, for fft.
		case -1:
			// Pass-through
			break;
	}
	if (w->info.dim[1] == 3 && w->info.dim[2] == 3 && hint.stride.dim[1] <= 1 && hint.stride.dim[2] <= 1)
		return _ccv_nnc_conv_forw_4x4_3x3_winograd(a, w, bias, hint, b);
#if defined(HAVE_SSE2)
	if (w->info.dim[3] % 4 == 0)
		return _ccv_nnc_conv_forw_sse2(a, w, bias, hint, b);
#elif defined(HAVE_NEON)
	if (w->info.dim[3] % 4 == 0)
		return _ccv_nnc_conv_forw_neon(a, w, bias, hint, b);
#endif
	return CCV_NNC_EXEC_INVALID;
}

//@ccv_nnc_init CCV_NNC_BACKEND_CPU_OPT
void ccv_nnc_cpu_opt_init(ccv_nnc_cmd_api_t cmd_api[])
{
	/*TODO: I don't think any of these methods handles batch input, and I better to handle CHWN as well. */
	/* Convolutional layer */
	cmd_api[CCV_NNC_COMPUTE_CONVOLUTIONAL_FORWARD].tensor_formats = CCV_TENSOR_FORMAT_NHWC;
	cmd_api[CCV_NNC_COMPUTE_CONVOLUTIONAL_FORWARD].algorithms = CCV_NNC_CMD_OPT_CONV_ALGO_COUNT;
	cmd_api[CCV_NNC_COMPUTE_CONVOLUTIONAL_FORWARD].exec = _ccv_nnc_conv_forw;
	/* Full connect layer */
	/* Max pool layer */
	/* Average pool layer */
	/* Softmax layer */
	/* ReLU activation */
}