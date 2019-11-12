#include "ccv_nnc.h"
#include "ccv_nnc_easy.h"
#include "ccv_nnc_internal.h"
#include "ccv_internal.h"
#include "_ccv_cnnp_model.h"

#pragma mark - Baisc Layers

static const ccv_cnnp_model_vtab_t ccv_cnnp_input_isa;

#define CCV_CNNP_IS_MODEL_INPUT(x) ((x)->isa == &ccv_cnnp_input_isa)

typedef struct {
	ccv_cnnp_model_t super;
	int sequence_size;
	ccv_cnnp_model_t* sequence[1];
} ccv_cnnp_sequential_model_t;

static void _ccv_cnnp_sequential_model_deinit(ccv_cnnp_model_t* const super)
{
	ccv_cnnp_sequential_model_t* const self = (ccv_cnnp_sequential_model_t*)super;
	int i, j;
	for (i = 0; i < self->sequence_size; i++)
	{
		ccv_cnnp_model_t* const model = self->sequence[i];
		if (!model)
			continue;
		ccv_cnnp_model_free(model);
		for (j = i + 1; j < self->sequence_size; j++)
			if (self->sequence[j] == model)
				self->sequence[j] = 0;
	}
}

static void _ccv_cnnp_sequential_model_build(ccv_cnnp_model_t* const super, ccv_nnc_symbolic_graph_t* const graph, const ccv_nnc_tensor_symbol_t* const inputs, const int input_size, ccv_nnc_tensor_symbol_t* const outputs, const int output_size)
{
	ccv_cnnp_sequential_model_t* const self = (ccv_cnnp_sequential_model_t*)super;
	int i;
	ccv_nnc_tensor_symbol_t input = inputs[0];
	assert(input_size == 1);
	for (i = 0; i < self->sequence_size; i++)
	{
		ccv_nnc_tensor_symbol_t output;
		ccv_cnnp_model_t* const sub_model = self->sequence[i];
		// Go through each sub model to build the graph.
		ccv_cnnp_model_build(sub_model, graph, &input, 1, &output, 1);
		input = output;
	}
	outputs[0] = input;
}

static void _ccv_cnnp_sequential_model_init_states(ccv_cnnp_model_t* const super, ccv_nnc_symbolic_graph_t* const graph, const ccv_cnnp_state_initializer_f initializer, void* const context)
{
	ccv_cnnp_sequential_model_t* const self = (ccv_cnnp_sequential_model_t*)super;
	int i;
	for (i = 0; i < self->sequence_size; i++)
		ccv_cnnp_model_init_states(self->sequence[i], graph, initializer, context);
}

static void _ccv_cnnp_sequential_model_add_to_trainable(ccv_cnnp_model_t* const super, const ccv_cnnp_add_to_array_f add_to_array, void* const trainables)
{
	ccv_cnnp_sequential_model_t* const self = (ccv_cnnp_sequential_model_t*)super;
	int i;
	for (i = 0; i < self->sequence_size; i++)
		ccv_cnnp_model_add_to_trainable(self->sequence[i], add_to_array, trainables);
}

static void _ccv_cnnp_sequential_model_add_to_output(ccv_cnnp_model_t* const super, const ccv_cnnp_add_to_array_f add_to_array, void* const outputs)
{
	ccv_cnnp_sequential_model_t* const self = (ccv_cnnp_sequential_model_t*)super;
	int i;
	for (i = 0; i < self->sequence_size; i++)
		ccv_cnnp_model_add_to_output(self->sequence[i], add_to_array, outputs);
}

static void _ccv_cnnp_sequential_model_set_is_test(ccv_cnnp_model_t* const super, const int is_test, const ccv_cnnp_cmd_updater_f updater, void* const context)
{
	ccv_cnnp_sequential_model_t* const self = (ccv_cnnp_sequential_model_t*)super;
	int i;
	for (i = 0; i < self->sequence_size; i++)
		ccv_cnnp_model_set_is_test(self->sequence[i], is_test, updater, context);
}

static const ccv_cnnp_model_vtab_t ccv_cnnp_sequential_model_isa = {
	.deinit = _ccv_cnnp_sequential_model_deinit,
	.build = _ccv_cnnp_sequential_model_build,
	.init_states = _ccv_cnnp_sequential_model_init_states,
	.add_to_trainable = _ccv_cnnp_sequential_model_add_to_trainable,
	.add_to_output = _ccv_cnnp_sequential_model_add_to_output,
	.set_is_test = _ccv_cnnp_sequential_model_set_is_test,
};

ccv_cnnp_model_t* ccv_cnnp_sequential_new(ccv_cnnp_model_t* const* const models, const int model_size, const char* const name)
{
	assert(model_size > 0);
	ccv_cnnp_sequential_model_t* const sequential_model = (ccv_cnnp_sequential_model_t*)cccalloc(1, sizeof(ccv_cnnp_sequential_model_t) + sizeof(ccv_cnnp_model_t*) * (model_size - 1) + sizeof(ccv_nnc_tensor_symbol_t));
	sequential_model->super.isa = &ccv_cnnp_sequential_model_isa;
	sequential_model->super.input_size = 1;
	sequential_model->super.outputs = (ccv_nnc_tensor_symbol_t*)(sequential_model->sequence + model_size);
	sequential_model->super.output_size = 1;
	ccv_cnnp_model_copy_name(&sequential_model->super, name);
	sequential_model->sequence_size = model_size;
	memcpy(sequential_model->sequence, models, sizeof(ccv_cnnp_model_t*) * model_size);
	return (ccv_cnnp_model_t*)sequential_model;
}

typedef struct {
	ccv_cnnp_model_t super;
	// The name is similar to sequential model, but it is just topological sorted models.
	int sequence_size;
	ccv_cnnp_model_io_t sequence[1];
} ccv_cnnp_functional_model_t;

static void _ccv_cnnp_functional_model_deinit(ccv_cnnp_model_t* const super)
{
	ccv_cnnp_functional_model_t* const self = (ccv_cnnp_functional_model_t*)super;
	int i, j = 0, k;
	for (i = 0; i < self->sequence_size; i++)
	{
		ccv_cnnp_model_t* const model = self->sequence[i]->model;
		if (!model)
			continue;
		self->sequence[j++] = (ccv_cnnp_model_io_t)model;
		// Go through all their IO to remove itself as model.
		assert(model->io);
		for (k = 0; k < model->io->rnum; k++)
		{
			ccv_cnnp_model_io_t model_io = *(ccv_cnnp_model_io_t*)ccv_array_get(model->io, k);
			model_io->model = 0;
		}
	}
	for (i = 0; i < j; i++)
		ccv_cnnp_model_free((ccv_cnnp_model_t*)self->sequence[i]);
}

static void _ccv_cnnp_functional_model_build(ccv_cnnp_model_t* const super, ccv_nnc_symbolic_graph_t* const graph, const ccv_nnc_tensor_symbol_t* const inputs, const int input_size, ccv_nnc_tensor_symbol_t* const outputs, const int output_size)
{
	ccv_cnnp_functional_model_t* const self = (ccv_cnnp_functional_model_t*)super;
	assert(self->super.input_size == input_size);
	assert(self->super.output_size == output_size);
	int i, j, k;
	for (i = 0; i < self->super.input_size; i++)
		self->sequence[i]->outputs[0] = self->sequence[i]->model->outputs[0] = inputs[i]; // Assigning the output symbol of input layer to be the input symbol.
	ccv_array_t* input_symbols = ccv_array_new(sizeof(ccv_nnc_tensor_symbol_t), 1, 0);
	for (i = self->super.input_size; i < self->sequence_size; i++)
	{
		ccv_cnnp_model_t* const sub_model = self->sequence[i]->model;
		ccv_array_clear(input_symbols);
		const ccv_array_t* const incomings = self->sequence[i]->incomings;
		for (j = 0; j < incomings->rnum; j++)
		{
			const ccv_cnnp_model_io_t input = *(ccv_cnnp_model_io_t*)ccv_array_get(incomings, j);
			for (k = 0; k < input->model->output_size; k++)
				ccv_array_push(input_symbols, &input->outputs[k]);
		}
		// Go through each sub model to build the graph.
		ccv_cnnp_model_build(sub_model, graph, (ccv_nnc_tensor_symbol_t*)ccv_array_get(input_symbols, 0), input_symbols->rnum, self->sequence[i]->outputs, sub_model->output_size);
	}
	ccv_array_free(input_symbols);
	for (i = output_size, k = self->sequence_size - 1; k >= 0; k--)
	{
		ccv_cnnp_model_t* const sub_model = self->sequence[k]->model;
		i -= sub_model->output_size;
		if (i < 0)
			break;
		for (j = 0; j < sub_model->output_size; j++)
			outputs[i + j] = self->sequence[k]->outputs[j];
	}
	assert(i <= 0);
}

static void _ccv_cnnp_functional_model_init_states(ccv_cnnp_model_t* const super, ccv_nnc_symbolic_graph_t* const graph, const ccv_cnnp_state_initializer_f initializer, void* const context)
{
	ccv_cnnp_functional_model_t* const self = (ccv_cnnp_functional_model_t*)super;
	int i;
	for (i = self->super.input_size; i < self->sequence_size; i++)
		ccv_cnnp_model_init_states(self->sequence[i]->model, graph, initializer, context);
}

static void _ccv_cnnp_functional_model_add_to_trainable(ccv_cnnp_model_t* const super, const ccv_cnnp_add_to_array_f add_to_array, void* const trainables)
{
	ccv_cnnp_functional_model_t* const self = (ccv_cnnp_functional_model_t*)super;
	int i;
	for (i = self->super.input_size; i < self->sequence_size; i++)
		ccv_cnnp_model_add_to_trainable(self->sequence[i]->model, add_to_array, trainables);
}

static void _ccv_cnnp_functional_model_add_to_output(ccv_cnnp_model_t* const super, const ccv_cnnp_add_to_array_f add_to_array, void* const outputs)
{
	ccv_cnnp_functional_model_t* const self = (ccv_cnnp_functional_model_t*)super;
	int i;
	for (i = self->super.input_size; i < self->sequence_size; i++)
		ccv_cnnp_model_add_to_output(self->sequence[i]->model, add_to_array, outputs);
}

static void _ccv_cnnp_functional_model_set_is_test(ccv_cnnp_model_t* const super, const int is_test, const ccv_cnnp_cmd_updater_f updater, void* const context)
{
	ccv_cnnp_functional_model_t* const self = (ccv_cnnp_functional_model_t*)super;
	int i;
	for (i = self->super.input_size; i < self->sequence_size; i++)
		ccv_cnnp_model_set_is_test(self->sequence[i]->model, is_test, updater, context);
}

static const ccv_cnnp_model_vtab_t ccv_cnnp_functional_model_isa = {
	.deinit = _ccv_cnnp_functional_model_deinit,
	.build = _ccv_cnnp_functional_model_build,
	.init_states = _ccv_cnnp_functional_model_init_states,
	.add_to_trainable = _ccv_cnnp_functional_model_add_to_trainable,
	.add_to_output = _ccv_cnnp_functional_model_add_to_output,
	.set_is_test = _ccv_cnnp_functional_model_set_is_test,
};

ccv_cnnp_model_t* ccv_cnnp_model_new(const ccv_cnnp_model_io_t* const inputs, const int input_size, const ccv_cnnp_model_io_t* const outputs, const int output_size, const char* const name)
{
	assert(output_size > 0);
	// Do topological sort.
	ccv_array_t* const reverse_top = ccv_array_new(sizeof(ccv_cnnp_model_io_t), output_size, 0);
	ccv_array_resize(reverse_top, output_size);
	memcpy(ccv_array_get(reverse_top, 0), outputs, sizeof(ccv_cnnp_model_io_t) * output_size);
	// Go from the output, until we meet inputs.
	int i, j, k;
	uint64_t input_bitmask[((input_size - 1) >> 6) + 1];
	memset(input_bitmask, 0, sizeof(uint64_t) * (((input_size - 1) >> 6) + 1));
	int tensor_output_size = 0; // io can be mapped to multiple tensor outputs, therefore, need to compute the exact tensor output size.
	for (i = 0; i < output_size; i++)
		tensor_output_size += outputs[i]->model->output_size;
	for (i = 0; i < reverse_top->rnum; i++)
	{
		const ccv_cnnp_model_io_t output = *(ccv_cnnp_model_io_t*)ccv_array_get(reverse_top, i);
		assert(!CCV_CNNP_IS_MODEL_INPUT(output->model));
		// If it is input, push it here.
		if (output->incomings)
			for (j = 0; j < output->incomings->rnum; j++)
			{
				const ccv_cnnp_model_io_t input = *(ccv_cnnp_model_io_t*)ccv_array_get(output->incomings, j);
				++input->visit; // Mark it as visited.
				if (input->visit != input->outgoings->rnum) // Not all dependencies visited.
					continue;
				if (!CCV_CNNP_IS_MODEL_INPUT(input->model))
					ccv_array_push(reverse_top, &input);
				else {
					for (k = 0; k < input_size; k++)
						if (input == inputs[k])
							break;
					assert(k < input_size);
					input_bitmask[k >> 6] |= ((uint64_t)1 << (k & 63));
				}
			}
	}
	for (i = 0; i < reverse_top->rnum; i++)
	{
		const ccv_cnnp_model_io_t output = *(ccv_cnnp_model_io_t*)ccv_array_get(reverse_top, i);
		output->visit = 0; // Clean the visit back.
	}
	for (i = 0; i < input_size; i++)
		inputs[i]->visit = 0; // Clean the visit back.
	for (i = 0; i < input_size; i++)
		{ assert((input_bitmask[i >> 6] & ((uint64_t)1 << (i & 63)))); } // Assuming they all match.
	const int sequence_size = reverse_top->rnum + input_size;
	ccv_cnnp_functional_model_t* const functional_model = (ccv_cnnp_functional_model_t*)cccalloc(1, sizeof(ccv_cnnp_functional_model_t) + sizeof(ccv_cnnp_model_t*) * (sequence_size - 1) + sizeof(ccv_nnc_tensor_symbol_t) * tensor_output_size);
	functional_model->super.isa = &ccv_cnnp_functional_model_isa;
	functional_model->super.outputs = (ccv_nnc_tensor_symbol_t*)(functional_model->sequence + sequence_size);
	functional_model->super.output_size = tensor_output_size;
	functional_model->super.input_size = input_size;
	ccv_cnnp_model_copy_name(&functional_model->super, name);
	functional_model->sequence_size = sequence_size;
	memcpy(functional_model->sequence, inputs, sizeof(ccv_cnnp_model_io_t) * input_size);
	for (i = 0; i < reverse_top->rnum; i++)
		functional_model->sequence[input_size + i] = *(ccv_cnnp_model_io_t*)ccv_array_get(reverse_top, reverse_top->rnum - 1 - i);
	ccv_array_free(reverse_top);
	return (ccv_cnnp_model_t*)functional_model;
}

static const ccv_cnnp_model_vtab_t ccv_cnnp_input_isa = {};

ccv_cnnp_model_io_t ccv_cnnp_input(void)
{
	ccv_cnnp_model_t* const input = (ccv_cnnp_model_t*)cccalloc(1, sizeof(ccv_cnnp_model_t) + sizeof(ccv_nnc_tensor_symbol_t));
	input->isa = &ccv_cnnp_input_isa;
	input->io = ccv_array_new(sizeof(ccv_cnnp_model_io_t), 1, 0);
	ccv_cnnp_model_io_t input_io = ccmalloc(sizeof(struct ccv_cnnp_model_io_s) + sizeof(ccv_nnc_tensor_symbol_t));
	input_io->visit = 0;
	input_io->incomings = 0;
	input_io->outgoings = 0;
	input_io->model = input;
	input_io->outputs = (ccv_nnc_tensor_symbol_t*)(input_io + 1);
	ccv_array_push(input->io, &input_io);
	input->outputs = (ccv_nnc_tensor_symbol_t*)(input + 1);
	input->output_size = 1;
	return input_io;
}

#pragma mark - Command Layer

typedef struct {
	ccv_cnnp_model_t super;
	ccv_nnc_cmd_t cmd;
	ccv_nnc_hint_t hint;
	ccv_nnc_tensor_symbol_t* input_symbols; // This is only valid for INIT_SHARED_TENSOR / INIT_SHARED_TENSOR_AS_TRAINABLE
	ccv_nnc_tensor_symbol_t* output_symbols; // This is just for the output symbol (in case we need to have no tensor symbol).
	ccv_cnnp_cmd_exec_io_t* inputs;
	int input_size;
	int* outputs;
	int output_size;
} ccv_cnnp_model_cmd_exec_t;

static void _ccv_cnnp_cmd_exec_build(ccv_cnnp_model_t* const super, ccv_nnc_symbolic_graph_t* const graph, const ccv_nnc_tensor_symbol_t* const inputs, const int input_size, ccv_nnc_tensor_symbol_t* const outputs, const int output_size)
{
	ccv_cnnp_model_cmd_exec_t* const self = (ccv_cnnp_model_cmd_exec_t*)super;
	ccv_nnc_tensor_param_t input_params[ccv_max(1, self->input_size)];
	int i, j;
	for (i = 0, j = 0; i < self->input_size; i++)
		if (self->inputs[i].type == CCV_CNNP_IO)
		{
			self->input_symbols[i] = inputs[j++];
			input_params[i] = ccv_nnc_tensor_symbol_params(graph, self->input_symbols[i]);
		} else if (self->inputs[i].type == CCV_CNNP_NO_TENSOR) {
			self->input_symbols[i] = NO_TENSOR_SYMBOL;
		} else if (!self->input_symbols[i].graph) {
			// Otherwise, we only create this symbol if it doesn't exist.
			const ccv_nnc_tensor_param_t params = self->inputs[i].init_state.info;
			input_params[i] = params;
			self->input_symbols[i] = ccv_nnc_tensor_symbol_new(graph, params, 0);
		}
	// We cannot simply mark the outputs as auto, because the subsequent build call may require this output to have params setup.
	// Infer the parameters here.
	ccv_nnc_tensor_param_t output_params[ccv_max(1, self->output_size)];
	ccv_nnc_hint_tensor_auto(self->cmd, input_params, self->input_size, self->hint, output_params, self->output_size);
	for (i = 0, j = 0; i < self->output_size; i++)
		if (self->outputs[i] == CCV_CNNP_IO)
			self->output_symbols[i] = outputs[j++] = ccv_nnc_tensor_symbol_new(graph, output_params[i], 0);
		else if (self->outputs[i] == CCV_CNNP_TENSOR_NOT_OUTPUT)
			self->output_symbols[i] = ccv_nnc_tensor_symbol_new(graph, output_params[i], 0);
		else
			self->output_symbols[i] = NO_TENSOR_SYMBOL;
	ccv_nnc_graph_exec_symbol_new(graph, self->cmd, self->input_symbols, self->input_size, self->output_symbols, self->output_size, 0);
}

static void _ccv_cnnp_cmd_exec_init_states(ccv_cnnp_model_t* const super, ccv_nnc_symbolic_graph_t* const graph, const ccv_cnnp_state_initializer_f initializer, void* const context)
{
	ccv_cnnp_model_cmd_exec_t* const self = (ccv_cnnp_model_cmd_exec_t*)super;
	int i;
	for (i = 0; i < self->input_size; i++)
		if (self->inputs[i].type == CCV_CNNP_INIT_SHARED_TENSOR || self->inputs[i].type == CCV_CNNP_INIT_SHARED_TENSOR_AS_TRAINABLE)
			self->inputs[i].init_state.init(self->input_symbols[i], initializer, context, self->inputs[i].init_state.context);
}

static void _ccv_cnnp_cmd_exec_add_to_output(ccv_cnnp_model_t* const super, const ccv_cnnp_add_to_array_f add_to_array, void* const outputs)
{
	ccv_cnnp_model_cmd_exec_t* const self = (ccv_cnnp_model_cmd_exec_t*)super;
	int i;
	for (i = 0; i < self->input_size; i++)
		if (self->inputs[i].type == CCV_CNNP_INIT_SHARED_TENSOR)
			add_to_array(outputs, self->input_symbols[i]); // Push this as retainable because it need to be init.
}

static void _ccv_cnnp_cmd_exec_add_to_trainable(ccv_cnnp_model_t* const super, const ccv_cnnp_add_to_array_f add_to_array, void* const trainables)
{
	ccv_cnnp_model_cmd_exec_t* const self = (ccv_cnnp_model_cmd_exec_t*)super;
	int i;
	for (i = 0; i < self->input_size; i++)
		if (self->inputs[i].type == CCV_CNNP_INIT_SHARED_TENSOR_AS_TRAINABLE)
			add_to_array(trainables, self->input_symbols[i]); // Push this as trainable.
}

static void _ccv_cnnp_cmd_exec_deinit(ccv_cnnp_model_t* const super)
{
	ccv_cnnp_model_cmd_exec_t* const self = (ccv_cnnp_model_cmd_exec_t*)super;
	int i, j;
	for (i = 0; i < self->input_size; i++)
		if ((self->inputs[i].type == CCV_CNNP_INIT_SHARED_TENSOR || self->inputs[i].type == CCV_CNNP_INIT_SHARED_TENSOR_AS_TRAINABLE) &&
			self->inputs[i].init_state.context)
		{
			void* const context = self->inputs[i].init_state.context;
			if (self->inputs[i].init_state.deinit)
				self->inputs[i].init_state.deinit(context);
			self->inputs[i].init_state.init = 0;
			self->inputs[i].init_state.deinit = 0;
			self->inputs[i].init_state.context = 0;
			for (j = i + 1; j < self->input_size; j++)
				if (self->inputs[j].init_state.context == context)
				{
					self->inputs[j].init_state.init = 0;
					self->inputs[j].init_state.deinit = 0;
					self->inputs[j].init_state.context = 0;
				}
		}
}

static const ccv_cnnp_model_vtab_t ccv_cnnp_cmd_exec_isa = {
	.build = _ccv_cnnp_cmd_exec_build,
	.init_states = _ccv_cnnp_cmd_exec_init_states,
	.add_to_trainable = _ccv_cnnp_cmd_exec_add_to_trainable,
	.add_to_output = _ccv_cnnp_cmd_exec_add_to_output,
	.deinit = _ccv_cnnp_cmd_exec_deinit,
};

ccv_cnnp_model_t* ccv_cnnp_cmd_exec(const ccv_nnc_cmd_t cmd, const ccv_nnc_hint_t hint, const int flags, const ccv_cnnp_cmd_exec_io_t* const inputs, const int input_size, const int* const outputs, const int output_size, const char* const name)
{
	assert(input_size >= 0);
	assert(output_size > 0);
	int i;
	int io_input_size = 0;
	for (i = 0; i < input_size; i++)
		if (inputs[i].type == CCV_CNNP_IO)
			++io_input_size;
		else {
			assert(inputs[i].type == CCV_CNNP_INIT_SHARED_TENSOR || inputs[i].type == CCV_CNNP_INIT_SHARED_TENSOR_AS_TRAINABLE);
			assert(inputs[i].init_state.init);
		}
	int io_output_size = 0;
	for (i = 0; i < output_size; i++)
		if (outputs[i] == CCV_CNNP_IO)
			++io_output_size;
		else {
			assert(outputs[i] == CCV_CNNP_TENSOR_NOT_OUTPUT || outputs[i] == CCV_CNNP_NO_TENSOR);
		}
	assert(io_output_size > 0);
	ccv_cnnp_model_cmd_exec_t* const model_cmd_exec = (ccv_cnnp_model_cmd_exec_t*)cccalloc(1, sizeof(ccv_cnnp_model_cmd_exec_t) + sizeof(ccv_nnc_tensor_symbol_t) * (io_output_size + input_size + output_size) + sizeof(ccv_cnnp_cmd_exec_io_t) * input_size + sizeof(int) * output_size);
	model_cmd_exec->super.isa = &ccv_cnnp_cmd_exec_isa;
	model_cmd_exec->super.input_size = io_input_size;
	model_cmd_exec->super.outputs = (ccv_nnc_tensor_symbol_t*)(model_cmd_exec + 1);
	model_cmd_exec->super.output_size = io_output_size;
	ccv_cnnp_model_copy_name(&model_cmd_exec->super, name);
	model_cmd_exec->cmd = cmd;
	model_cmd_exec->hint = hint;
	model_cmd_exec->input_size = input_size;
	model_cmd_exec->input_symbols = model_cmd_exec->super.outputs + io_output_size;
	model_cmd_exec->output_symbols = model_cmd_exec->input_symbols + input_size;
	model_cmd_exec->inputs = (ccv_cnnp_cmd_exec_io_t*)(model_cmd_exec->output_symbols + output_size);
	if (input_size > 0)
		memcpy(model_cmd_exec->inputs, inputs, sizeof(ccv_cnnp_cmd_exec_io_t) * input_size);
	model_cmd_exec->output_size = output_size;
	model_cmd_exec->outputs = (int*)(model_cmd_exec->inputs + input_size);
	if (output_size > 0)
		memcpy(model_cmd_exec->outputs, outputs, sizeof(int) * output_size);
	return (ccv_cnnp_model_t*)model_cmd_exec;
}

static void _ccv_cnnp_cmd_exec_io_copy(const ccv_nnc_tensor_symbol_t tensor_symbol, const ccv_cnnp_state_initializer_f initializer, void* const initializer_context, void* const context)
{
	initializer(initializer_context, CMD_DATA_TRANSFER_FORWARD(), ccv_nnc_no_hint, 0, (ccv_nnc_tensor_t*)context, tensor_symbol);
}

ccv_cnnp_cmd_exec_io_init_state_t ccv_cnnp_cmd_exec_io_copy(const ccv_nnc_tensor_t* const tensor)
{
	return (ccv_cnnp_cmd_exec_io_init_state_t){
		.info = tensor->info,
		.context = (void *)tensor,
		.init = _ccv_cnnp_cmd_exec_io_copy,
	};
}

typedef struct {
	ccv_nnc_cmd_t cmd;
	ccv_nnc_hint_t hint;
	int flags;
} ccv_cnnp_cmd_exec_io_set_by_t;

static void _ccv_cnnp_cmd_exec_io_set_by(const ccv_nnc_tensor_symbol_t tensor_symbol, const ccv_cnnp_state_initializer_f initializer, void* const initializer_context, void* const context)
{
	const ccv_cnnp_cmd_exec_io_set_by_t* const set_by = (ccv_cnnp_cmd_exec_io_set_by_t*)context;
	initializer(initializer_context, set_by->cmd, set_by->hint, set_by->flags, 0, tensor_symbol);
}

ccv_cnnp_cmd_exec_io_init_state_t ccv_cnnp_cmd_exec_io_set_by(const ccv_nnc_cmd_t cmd, const ccv_nnc_hint_t hint, const int flags, const ccv_nnc_tensor_param_t params)
{
	ccv_cnnp_cmd_exec_io_set_by_t* const set_by = (ccv_cnnp_cmd_exec_io_set_by_t*)ccmalloc(sizeof(ccv_cnnp_cmd_exec_io_set_by_t));
	set_by->cmd = cmd;
	set_by->hint = hint;
	set_by->flags = flags;
	return (ccv_cnnp_cmd_exec_io_init_state_t){
		.info = params,
		.context = set_by,
		.init = _ccv_cnnp_cmd_exec_io_set_by,
		.deinit = ccfree,
	};
}
