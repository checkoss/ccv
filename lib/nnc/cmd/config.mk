CMD_SRCS := ./loss/ccv_nnc_categorical_crossentropy_cpu_ref.c ./reduce/ccv_nnc_reduce_sum_cpu_ref.c ./reduce/ccv_nnc_reduce_max_cpu_ref.c ./util/ccv_nnc_util_cpu_ref.c ./sgd/ccv_nnc_sgd_cpu_ref.c ./relu/ccv_nnc_relu_cpu_ref.c ./softmax_loss/ccv_nnc_softmax_crossentropy_cpu_ref.c ./softmax/ccv_nnc_softmax_cpu_ref.c ./convolution/ccv_nnc_conv_cpu_ref.c ./convolution/ccv_nnc_conv_cpu_opt.c ./compression/ccv_nnc_lssc_cpu_ref.c ./blas/ccv_nnc_gemm_cpu_ref.c ./blas/ccv_nnc_gemm_cpu_opt.c ./blas/ccv_nnc_add_cpu_ref.c ./blas/ccv_nnc_mul_cpu_ref.c ./norm/ccv_nnc_batch_norm_cpu_ref.c ./ew/ccv_nnc_ew_cpu_ref.c ./rand/ccv_nnc_rand_uniform_cpu_ref.c ./index/ccv_nnc_index_select_cpu_ref.c ./pool/ccv_nnc_max_pool_cpu_ref.c ./pool/ccv_nnc_avg_pool_cpu_ref.c ./dropout/ccv_nnc_dropout_cpu_ref.c ./loss/ccv_nnc_categorical_crossentropy.c ./reduce/ccv_nnc_reduce.c ./util/ccv_nnc_util.c ./sgd/ccv_nnc_sgd.c ./relu/ccv_nnc_relu.c ./softmax_loss/ccv_nnc_softmax_crossentropy.c ./softmax/ccv_nnc_softmax.c ./convolution/cpu_opt/_ccv_nnc_conv_cpu_4x4_3x3_winograd.c ./convolution/cpu_opt/_ccv_nnc_conv_cpu_fft.c ./convolution/cpu_opt/_ccv_nnc_conv_cpu_gemm.c ./convolution/cpu_opt/_ccv_nnc_conv_cpu_opt.c ./convolution/ccv_nnc_convolution.c ./compression/ccv_nnc_compression.c ./blas/cpu_opt/_ccv_nnc_gemm_cpu_opt.c ./blas/cpu_sys/_ccv_nnc_gemm_cpu_sys.c ./blas/ccv_nnc_blas.c ./norm/ccv_nnc_batch_norm.c ./ew/ccv_nnc_ew.c ./comm/ccv_nnc_comm.c ./rand/ccv_nnc_rand.c ./index/ccv_nnc_index_select.c ./pool/ccv_nnc_pool.c ./dropout/ccv_nnc_dropout.c
CUDA_CMD_SRCS := ./loss/gpu/ccv_nnc_categorical_crossentropy_gpu_ref.cu ./util/gpu/ccv_nnc_util_gpu_cudnn.cu ./util/gpu/ccv_nnc_util_gpu_ref.cu ./sgd/gpu/ccv_nnc_sgd_gpu_cudnn.cu ./relu/gpu/ccv_nnc_relu_gpu_cudnn.cu ./softmax_loss/gpu/ccv_nnc_softmax_crossentropy_gpu_cudnn.cu ./softmax/gpu/ccv_nnc_softmax_gpu_cudnn.cu ./convolution/gpu/ccv_nnc_conv_gpu_cudnn.cu ./compression/gpu/ccv_nnc_lssc_gpu_ref.cu ./blas/gpu/ccv_nnc_gemm_gpu_cublas.cu ./blas/gpu/ccv_nnc_add_gpu_cudnn.cu ./norm/gpu/ccv_nnc_batch_norm_gpu_cudnn.cu ./ew/gpu/ccv_nnc_ew_gpu_cudnn.cu ./comm/gpu/ccv_nnc_comm_gpu_nccl.cu ./rand/gpu/ccv_nnc_rand_uniform_gpu_ref.cu ./pool/gpu/ccv_nnc_max_pool_gpu_cudnn.cu ./pool/gpu/ccv_nnc_avg_pool_gpu_cudnn.cu ./dropout/gpu/ccv_nnc_dropout_gpu_cudnn.cu
