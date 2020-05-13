/**
 * @addtogroup available_command_ids Available Command Identifiers
 * @{
 */
enum {
	CCV_NNC_NOOP = 0,
	CCV_NNC_CUSTOM_FORWARD = 2,
	CCV_NNC_CUSTOM_BACKWARD,
	CCV_NNC_GRAPH_FORWARD,
	CCV_NNC_GRAPH_BACKWARD,
	CCV_NNC_RANDOM_UNIFORM_FORWARD = 0xa0cd1d5e,
	CCV_NNC_RANDOM_UNIFORM_BACKWARD = 0xa0cd1d5f,
	CCV_NNC_CONVOLUTION_FORWARD = 0x254d05f4,
	CCV_NNC_CONVOLUTION_BACKWARD = 0x254d05f5,
	CCV_NNC_SWISH_FORWARD = 0x583d90c2,
	CCV_NNC_SWISH_BACKWARD = 0x583d90c3,
	CCV_NNC_DROPOUT_FORWARD = 0x7f2dc3e4,
	CCV_NNC_DROPOUT_BACKWARD = 0x7f2dc3e5,
	CCV_NNC_SOFTMAX_CROSSENTROPY_FORWARD = 0xc26b7b5e,
	CCV_NNC_SOFTMAX_CROSSENTROPY_BACKWARD = 0xc26b7b5f,
	CCV_NNC_SGD_FORWARD = 0xe650ad26,
	CCV_NNC_SGD_BACKWARD = 0xe650ad27,
	CCV_NNC_MAX_POOL_FORWARD = 0x7bec9360,
	CCV_NNC_MAX_POOL_BACKWARD = 0x7bec9361,
	CCV_NNC_AVERAGE_POOL_FORWARD = 0x51267ab8,
	CCV_NNC_AVERAGE_POOL_BACKWARD = 0x51267ab9,
	CCV_NNC_SIGMOID_BINARY_CROSSENTROPY_FORWARD = 0xd9e0e4a,
	CCV_NNC_SIGMOID_BINARY_CROSSENTROPY_BACKWARD = 0xd9e0e4b,
	CCV_NNC_COMPRESSION_LSSC_FORWARD = 0x17ea8f72,
	CCV_NNC_COMPRESSION_LSSC_BACKWARD = 0x17ea8f73,
	CCV_NNC_SOFTMAX_FORWARD = 0xc969a252,
	CCV_NNC_SOFTMAX_BACKWARD = 0xc969a253,
	CCV_NNC_BINARY_CROSSENTROPY_FORWARD = 0xcd2107ec,
	CCV_NNC_BINARY_CROSSENTROPY_BACKWARD = 0xcd2107ed,
	CCV_NNC_CATEGORICAL_CROSSENTROPY_FORWARD = 0x1eb327a2,
	CCV_NNC_CATEGORICAL_CROSSENTROPY_BACKWARD = 0x1eb327a3,
	CCV_NNC_RELU_FORWARD = 0xc51eaa80,
	CCV_NNC_RELU_BACKWARD = 0xc51eaa81,
	CCV_NNC_ADAM_FORWARD = 0xe30099dc,
	CCV_NNC_ADAM_BACKWARD = 0xe30099dd,
	CCV_NNC_GEMM_FORWARD = 0x7e87d00c,
	CCV_NNC_GEMM_BACKWARD = 0x7e87d00d,
	CCV_NNC_ADD_FORWARD = 0x58fb3664,
	CCV_NNC_ADD_BACKWARD = 0x58fb3665,
	CCV_NNC_MUL_FORWARD = 0x24721a46,
	CCV_NNC_MUL_BACKWARD = 0x24721a47,
	CCV_NNC_SCALAR_MUL_FORWARD = 0x8b4d86aa,
	CCV_NNC_SCALAR_MUL_BACKWARD = 0x8b4d86ab,
	CCV_NNC_UPSAMPLE_BILINEAR_FORWARD = 0x48252aac,
	CCV_NNC_UPSAMPLE_BILINEAR_BACKWARD = 0x48252aad,
	CCV_NNC_COMM_ALLREDUCE_FORWARD = 0x75c8d340,
	CCV_NNC_COMM_ALLREDUCE_BACKWARD = 0x75c8d341,
	CCV_NNC_COMM_BROADCAST_FORWARD = 0x830eee,
	CCV_NNC_COMM_BROADCAST_BACKWARD = 0x830eef,
	CCV_NNC_COMM_REDUCE_FORWARD = 0x3434ead8,
	CCV_NNC_COMM_REDUCE_BACKWARD = 0x3434ead9,
	CCV_NNC_SET_FORWARD = 0x2b070804,
	CCV_NNC_SET_BACKWARD = 0x2b070805,
	CCV_NNC_MASKED_FILL_FORWARD = 0x7f992d84,
	CCV_NNC_MASKED_FILL_BACKWARD = 0x7f992d85,
	CCV_NNC_DATA_TRANSFER_FORWARD = 0x12d21e1a,
	CCV_NNC_DATA_TRANSFER_BACKWARD = 0x12d21e1b,
	CCV_NNC_FORMAT_TRANSFORM_FORWARD = 0xe4a2b192,
	CCV_NNC_FORMAT_TRANSFORM_BACKWARD = 0xe4a2b193,
	CCV_NNC_TRANSPOSE_FORWARD = 0xb4d506e0,
	CCV_NNC_TRANSPOSE_BACKWARD = 0xb4d506e1,
	CCV_NNC_DATATYPE_CONVERSION_FORWARD = 0xd873e38c,
	CCV_NNC_DATATYPE_CONVERSION_BACKWARD = 0xd873e38d,
	CCV_NNC_SIGMOID_FORWARD = 0xf2f69650,
	CCV_NNC_SIGMOID_BACKWARD = 0xf2f69651,
	CCV_NNC_INDEX_SELECT_FORWARD = 0x7ee7771e,
	CCV_NNC_INDEX_SELECT_BACKWARD = 0x7ee7771f,
	CCV_NNC_RMSPROP_FORWARD = 0x9c886b1c,
	CCV_NNC_RMSPROP_BACKWARD = 0x9c886b1d,
	CCV_NNC_EWSUM_FORWARD = 0xe21a2c4c,
	CCV_NNC_EWSUM_BACKWARD = 0xe21a2c4d,
	CCV_NNC_EWPROD_FORWARD = 0xee07e8fe,
	CCV_NNC_EWPROD_BACKWARD = 0xee07e8ff,
	CCV_NNC_EWDIV_FORWARD = 0x1cd2fa18,
	CCV_NNC_EWDIV_BACKWARD = 0x1cd2fa19,
	CCV_NNC_EWEXP_FORWARD = 0xd784b170,
	CCV_NNC_EWEXP_BACKWARD = 0xd784b171,
	CCV_NNC_EWLOG_FORWARD = 0xf4191bf2,
	CCV_NNC_EWLOG_BACKWARD = 0xf4191bf3,
	CCV_NNC_EWSQRT_FORWARD = 0x8870a61e,
	CCV_NNC_EWSQRT_BACKWARD = 0x8870a61f,
	CCV_NNC_REDUCE_SUM_FORWARD = 0x52970f06,
	CCV_NNC_REDUCE_SUM_BACKWARD = 0x52970f07,
	CCV_NNC_REDUCE_MAX_FORWARD = 0x80f1a506,
	CCV_NNC_REDUCE_MAX_BACKWARD = 0x80f1a507,
	CCV_NNC_BATCH_NORM_FORWARD = 0x5419819c,
	CCV_NNC_BATCH_NORM_BACKWARD = 0x5419819d,
	CCV_NNC_LAYER_NORM_FORWARD = 0xbed3c264,
	CCV_NNC_LAYER_NORM_BACKWARD = 0xbed3c265,
	CCV_NNC_COUNT = 87,
};
/** @} */
