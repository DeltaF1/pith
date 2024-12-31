// https://www.3dbrew.org/wiki/NFCS:SendTagCommand
mod NFCS {
	// TODO: Deal with namespacing

	#[repr(C, pack(4))]
	struct SendTagCommandRequest {
		unknown: u32,
		inputsize: u32,
		outputsize: u32,
		timing_value: AlignedU8,
	}

	struct SendTagCommandResponse {
		actual_output_size: u32,
	}

	struct SendTagCommand;

	impl Command for SendTagCommand {
		const HEADER: u32 = 0x00130102;
		// TODO: RESPONSE_HEADER field
		type Request = SendTagCommandRequest;
		type RequestTranslate = TranslateParams<Static<1>>;
		type Response = SendTagCommandResponse;
		type ResponseTranslate = TranslateParams<Static<0>>;
	}

    fn build_request(unknown: u32, input: &A, output: &mut B, timing_value: u8) -> SendTagCommandRequest {
        ...
    }

	fn SendTagCommand<A, B>(handle: Handle, unknown: u32, input: &A, output: &mut B, timing_value: u8) -> Result<SendTagCommandResponse> {
        ...    	
	}
}
