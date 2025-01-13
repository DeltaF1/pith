// https://www.3dbrew.org/wiki/NFCS:SendTagCommand

struct NormalParams {
	u32 unknown;
	u32 inputsize;
	u32 outputsize;
	u32 timing_value;
};

struct TranslateParams {
	struct StaticTranslation translate_1;
};

struct Request {
	u32 header;
	struct NormalParams normal;
	struct TranslateParams translate;
};

struct Request build_request(uint32_t unknown,
							 void *input, size_t inputsize,
							 void *output, size_t outputsize,
							 u8 timing_value) {

	Request r;
	r.header = 0x00130102;
	r.normal = {
		.unknown = unknown,
		.inputsize = inputsize,
		.outputsize = outputsize,
		.timing_value = u8_to_u32(timing_value)
	};
	r.translate = {
		.translate_1 = translate_static(input, inputsize, 1)
	}
}

void prepare_receive_buffers(void *output, size_t outputsize) {
	uint32_t *buffers = ...;
	buffers[0] = ...;
}

struct Response {
	IPCHeader header;
	Result result;
	uint32_t actual_output_size;
	StaticTranslation translate_1;
};

struct Response* get_response() {
	uint32_t *buf = ...;
	Result result = buf[1];
	if (result < 0) {
		return null;
	}
	return (struct Response *)buf;
}

struct Reponse* NFCS:SendTagCommand(Handle handle, uint32_t unknown,
							 void *input, size_t inputsize,
							 void *output, size_t outputsize,
							 u8 timing_value) {
	build_request(unknown, input, inputsize, output, outputsize);
	prepare_receive_buffers(output, outputsize);
	
	svcSendSyncRequest(handle);
	
	return get_response();
}
