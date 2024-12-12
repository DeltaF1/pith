_The stuff in between all of the CiTRus segments_

***

Familiarity is assumed with the 3DS' [Inter-Process Communication](https://www.3dbrew.org/wiki/IPC) system. This tool will probably be most useful for people looking to:
1. Port new languages and standard libraries to the 3DS
2. Implement libraries or functionality not already covered by [libctru](https://libctru.devkitpro.org/)
3. Re-implement system modules from scratch (see https://gist.github.com/kynex7510/83cec53fd9aaa5dfa1492a2b5fb53813)
4. Add entirely new APIs to use internally in their homebrew applications, or add new functionality to existing system modules

# Rationale
The [3dbrew](https://www.3dbrew.org/) wiki is an incredible resource with detailed descriptions of (nearly?) every service API included in the 3DS operating system. Each wiki page for a specific IPC call has tables detailing the data required to call an API and the data that API returns. Unfortunately, this information is written by multiple authors across a long period of time, and its contents and formatting varies accordingly. This information is useful for a human looking to learn about or implement these services, but it is not machine-readable. I propose an Interface Description Language to express the information already present in the wiki pages in a familiar and standardized format which can be used to auto-generate types and methods to use these APIs from languages such as C and Rust. This IDL can then in turn be used to generate wiki pages and other documentation in a more standardized fashion.

Converting all of the human-written human-readable wiki tables to a new format is a daunting task. There are certain patterns that have emerged for how to write these tables which I believe make the work semi-automatable. For example many wiki pages explicitly spell out the code necessary to create translate headers like `0x0000000c | (size << 4)`. This could be detected and automatically turned into a `write_buffer` value hint. The work I've seen recently to convert the tables to use uniform wiki templates will also help tremendously.

An ideal system would involve some sort of browser extension which can spider through the wiki, attempt to parse as it goes, then prompt the human user for help when it does not know how to interpret something. As a fall-back, entries in a table can be represented as simply words with the full wiki description added as an [attribute](#Attributes) like `u32 word_xxx [wikitext=""];`
# Use cases
The descriptions can be used to generate code or function signatures with various levels of safety checking.
1. Verify the size of the request data is correct. At this stage the header value (e.g. `0x00130102`) is enough
2. Verify that translated parameters are of the correct type/in the correct order. This requires the header + the types of translated params (including the number of headers sent with each header translation) + the required static buffer descriptors set up to receive data
3. Typed normal parameters. This level requires knowing that a particular word or set of words represents a single object such as a u8 buffer or an f32. At this level the types can link to struct definitions e.g. the shared Animation header struct in https://www.3dbrew.org/wiki/MCURTC:SetInfoLEDPatternHeader and https://www.3dbrew.org/wiki/MCURTC:SetInfoLEDPattern
4. Hints for semantics. At this level different values are tagged with semantic meaning such as "this parameter is the size of a specific buffer". This allows e.g. generating code to automatically ensure that the size in a normal parameter is the same as the size passed to a translate parameter (See: the certificate sizes in https://www.3dbrew.org/wiki/AMNet:ImportCertificates)
Another goal is to produce human-readable documentation i.e. the wiki tables. An ideal system would be able to reproduce the tables on the wiki to a degree that does not lose any information. Preserving comments and explanatory text is useful if those comments cannot be easily represented in the semantic language. See [Attributes](#Attributes)
# Syntax

TODO: Find a real complicated API and use that as the syntax demo (leaving out full wikitext since that will get messy)

```
// SERVICE_MethodName.pith

struct SERVICE:SomeStructType {
	wikiurl = "https://3dbrew.org/wiki/SERVICE#SomeStructType"

	f32 x;
	f32 y;
	f32 z;
}

enum SERVICE:SomeEnumType {
	FOO = 0,
	BAR,
	BAZ
}

command SERVICE:MethodName {
	wikiurl = "https://www.3dbrew.org/wiki/SERVICE:MethodName"

	command_id = 0x1234

	Request {
		Header header;
		u32 some_normal_param;
		u8[8] array;
		#<bikeshedpacked> {
			u8 flag1;
			u8 flag2;
		}
		SERVICE:SomeStructType some_normal_param_2;
		SERVICE:SomeEnumType<u16> flag2;
		u64 unaligned_u64s_work;

		ReadBuffer<u8[100]> translated_param;
		MoveHandles[4] translated_param_2;
		StaticBuffer@0<SomeOtherStructType> translated_param_3;
	}
	
	Response {
		Header header;
		Result result;

		u32 normal_param = bufsize;
		
		StaticBuffer@1 translated_param = static_buffer(buf, bufsize);
		MoveHandles[1] translated_param_2;
	}

	extra_wiki_information = "..."
}
```

In order to drive adoption, should be able to generate C code and keep concepts/syntax C-like.

Fields are declared with a syntax similar to C struct declaration.
```
field_type field_name;
```
Fields can be declared with value hints which are used to express common relationships like "pointer to" or "size of"
```
field_type field_name = value_hint;
```

Fields which are of struct type can use C-like syntax to assign value hints to sub-fields.

```
struct_type field_name = { .sub_field = value_hint };
```

Use a C-like syntax again for struct and enum definitions.

Struct definitions can have value hints but must be constant (as in magic numbers). TODO Possible future extension to allow self-referential value hints.

```
[meta]
wikiurl = ""

struct StructType {
	field_type field_name = value_hint [attributes];
}
```

When structs/enums are referenced by requests, the generated wiki text should create a hyperlink to their wikiurl.

TODO: struct and enum definitions should be namespaced

TODO: ~~enum members must be referenced like a rust enum e.g. `PSPXI::Algorithm::CCM_Encrypt` rather than polluting the global namespace like a C enum.~~ Does this matter? Enum members will probably never be referenced in the IDL, just their types
## Attributes

Attributes are key-value pairs with metadata pertaining to a specific field. syntax is 

```
<attr> is key1 = value1, key2 = value2, ...

field_type field_name [<attr>];
field_type field_name = value_hint [<attr>];
```

whitespace is not significant. Values can be strings or TODO

Suggested attributes are

| Attribute  | Description                                                                                                                                                              |
| ---------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `name`     | The human-readable name of a field                                                                                                                                       |
| `comment`  | Additional information about a field, to display alongside its type and name                                                                                             |
| `wikitext` | Completely overrides the textual representation of this field, ignoring all other attributes and type information. Useful for a first-pass of parsing the existing wiki. |

Example
```
u8[32] red_pattern [name="Red pattern"];

OR

u8[32] red_pattern [wikitext="u8[32] Red pattern"];
```

Could both produce the wiki table

| Index word | Description        |
| ---------- | ------------------ |
| 0          | u8[32] Red pattern |

Other than in the case of `wikitext`, the textual representation of each field is left deliberately unspecified so as not to create an interface for parsing, and to allow for innovation in future output methods (such as the templates being added to the wiki by ElementW and others).


TODO: If notice `;` followed by `[` then suggest to move inside the `;`. Don't want whitespace to be significant.

# Semantics
TODO: Make sure that translation types never end up in the normal area and vice versa

## Data types

| IDL Name                                                                                                          | C name                      | Rust name         | size (words) | notes                                                                                                                                               |
| ----------------------------------------------------------------------------------------------------------------- | --------------------------- | ----------------- | ------------ | --------------------------------------------------------------------------------------------------------------------------------------------------- |
| u8 / s8                                                                                                           | (u)int8_t / (unsigned) char | u8 / i8           | 0.25         | May be packed in with other chars through [bikeshedpacked](#bikeshedpacked). Defaults to 4-bye alignment                                                          |
| u16 / s16                                                                                                         | (u)int16_t                  | u16 / i16         | 0.5          | May be packed in with another short through [bikeshedpacked](#bikeshedpacked). Defaults to 4-byte alignment                                                       |
| u32 / s32                                                                                                         | (u)int32_t                  | u32 / i32         | 1            |                                                                                                                                                     |
| TODO: have a word type?                                                                                           | typedef uint32_t word;      | type word = u32;  | 1            |                                                                                                                                                     |
| u64 / s64                                                                                                         | (u)int64_t                  | u64 / i64         | 2            | Is only 4-byte aligned (i.e. not properly aligned for C or Rust's purposes)[^u64_align] so it might be unaligned while stored in the command buffer |
| f32                                                                                                               | ???                         | f32               | 1            |                                                                                                                                                     |
| f64                                                                                                               | ???                         | f64               | 2            |                                                                                                                                                     |
| ???                                                                                                               | T \*                        | \*const/mut T     | 1            |                                                                                                                                                     |
| T[N]                                                                                                              | T[N]                        | [T; N]            | variable     | Elements are packed internally according to [Array Layout](#Array-Layout) e.g. `u8[8]` is 2 words                                                               |
| Handle                                                                                                            | typedef u32 Handle          | type Handle = u32 | 1            |                                                                                                                                                     |
| Header                                                                                                            | typedef u32 Header          | type Header = u32 | 1            |                                                                                                                                                     |
| Result                                                                                                            | typedef s32 Result          | type Result = s32 | 1            |                                                                                                                                                     |
| `CopyHandles[N]`                                                                                                  |                             |                   | variable     |                                                                                                                                                     |
| `MoveHandles[N]`                                                                                                  |                             |                   | variable     |                                                                                                                                                     |
| `SendProcessID[N]`                                                                                                |                             |                   | variable     |                                                                                                                                                     |
| `StaticBuffer<T>@N` This syntax is ugly but I want to have a way to express the static buffer index in the types. |                             |                   | 2            |                                                                                                                                                     |
| `ReadBuffer<T>`                                                                                                   |                             |                   | 2            |                                                                                                                                                     |
| `WriteBuffer<T>`                                                                                                  |                             |                   | 2            |                                                                                                                                                     |
| `ReadWriteBuffer<T>`                                                                                              |                             |                   | 2            |                                                                                                                                                     |

## Array Layout
Elements in an array or a `#<bikeshedpacked>` section are packed as in C according to their packed alignments. Most types are still word-aligned but this allows for types smaller than a word to be put next to one another:

| Type            | Packed Alignment (bytes)    |
| --------------- | --------------------------- |
| u8/s8           | 1                           |
| u16/s16         | 2                           |
| u32/s32         | 4                           |
| u64/s64         | 4                           |
| f32             | 4                           |
| f64             | 4                           |
| struct          | Maximum alignment of fields |
| array of T      | Alignment of type T         |
| all other types | 4                           |

## Struct Layout
Requests and Responses are laid out according to the same algorithm as structs. The algorithm is the same as in C but all fields are word-aligned, which is to say: Fields are laid out in the order they are written. Each field is word-aligned and starts at the closest word-aligned address after the previous field. For types smaller than a word this requires wrapper types in C/Rust/etc. that look like so

```c
struct WordU8 {
	u8 value;
} __attribute__((aligned(4)));

struct WordU16 {
	u16 value;
} __attribute__((aligned(4)));
```

### bikeshedpacked
Use the `#<bikeshedpacked> {}` syntax to lower the alignment of fields smaller than a whole word (see [Array Layout](#Array-layout) for details). The start of the packed fields and any fields after the packed fields end are still word-aligned.

e.g.
```
Request {
	u32 param_1;
	#<bikeshedpacked> {
		u8 param_2;
		u8 param_3;
		u8 param_4;
		// Implied 1 byte padding
	} // No trailing semicolon
	// Next param stays word-aligned
	u32 param_5;
}
```

## Enum representation
The integer representation of C enums are notoriously squishy and implementation-defined. I propose to separate the representation of an enum from its definition, and let the places that use an enum define what size integer to use. For example the following enum is represented as a u8 in some contexts, but theoretically it could be necessary to represent it by a larger integer in an array or packed context. Enums should only be allowed to be represented by integer types.

```
wikiurl = "https://www.3dbrew.org/wiki/PSPXI:EncryptDecryptAes#Algorithm_Types"
enum PSPXI:Algorithm {
	CBC_Encrypt = 0
	CBC_Decrypt = 1
	CTR_Encrypt = 2
	CTR_Decrypt = 3
	CCM_Encrypt = 4
	CCM_Decrypt = 5
}
```

Then this enum can be used like so 

```
Request {
	PSPXI:Algorithm<u8> algorithm_type;
}

...

// Some context where they need to be laid out in a particular way
struct foo {
	PSPXI:Algorithm<u16>[4] algorithm_list;
}
```

Also open to just defining the representation type as part of the enum definition.


[^u64_align]: Armv6 supports 4-byte aligned accesses for 64-bit words[^1][^2][^3]. Unfortunately the ABI for ARM (as well as modern compilers like devkitpro's gcc and rustc) define the alignment of u64 as 8-bytes on ARMv6k. For this reason wrappers must take care to only use `memcpy` to read these values in C. A smart compiler will eliminate/inline the memcpy call anyway so it shouldn't matter for performance.

[^1]:https://developer.apple.com/documentation/xcode/writing-armv6-code-for-ios
[^2]:https://developer.arm.com/documentation/ddi0419/c/Application-Level-Architecture/Application-Level-Programmers--Model/ARM-processor-data-types-and-arithmetic
[^3]: https://www.scs.stanford.edu/~zyedidia/docs/arm/armv6.pdf

## Value hints

TODO: Allow arithmetic expressions? This is probably future work.

There are common patterns across APIs such as sending pointers to buffers in the following manner (pseudocode):
```
Request {
	u32 buffer_size;
	u32 read_buffer_header = 0x0000000A | (buffer_size<<4);
	u32 read_buffer_pointer = buffer;
}
```

It would be convenient to express it like so

```
bikeshedargsyntax<char* buffer>;

Request {
	u32 buffer_size = size(buffer);
	ReadBuffer buffer = read_buffer(buffer, size(buffer));
}
```

In Rust the size can be determined from fat pointers, but C requires a separate argument. Need C-legible syntax to describe the concept of a pointer to a variable array. Maybe something like this?

```
Request (char[] buffer) {
	u32 buffer_size = size(buffer);
	ReadBuffer<T> buffer = read_buffer(buffer, size(buffer));
}
```

which translates to

```c
void request(char* buffer, size_t bufsize) { ... }
```
```rust
fn request(buffer: &[u8]) { ... }
```

| Expression                               | Description                                                                |
| ---------------------------------------- | -------------------------------------------------------------------------- |
| `sizeof(x)` where `x` is of type `T`     | size of `x` in bytes. equivalent to rust's `std::mem::size_of_val::<T>(x)` |
| `countof(x)` where `x` is of type `T[N]` | Number of elements in the array, i.e. `N`                                  |
| `{x1, x2, x3, ...}`                      | Array constructor                                                          |
| `{.field1 = ..., .field2 = ..., ...}`    | Struct value hints                                                         |
| `read_buffer(pointer, size)`             | Read-only buffer mapping                                                   |
| `write_buffer(pointer, size)`            | Write-only buffer mapping                                                  |
| `read_write_buffer(pointer, size)`       | Read/Write buffer mapping                                                  |
| `static_buffer(pointer, size)`           | Static buffer translation                                                  |
## Output examples

### C

Level 1
```c
#include <assert.h>

// TODO: How to ensure struct packing in C?

// Is this valid across compilers? Doesn't matter as long as it works on devkitpro GCC
#pragma pack(4)
struct Request /* TODO: Determine name-generating rules */ {
	u32 header;
	u32 normal_1;
	u32 normal_2;
	...
	u32 translate_raw_1; // Distinguish between the names of fully typed 
					 // translate params and lists of words using _raw
	u32 translate_raw_2;
	...
}

struct Response {
	u32 header;
	u32 result;
	u32 normal_1;
	u32 normal_2;
	...
	u32 translate_raw_1;
	u32 translate_raw_2;
	...
}

// IPC const_.h
const size_t IPC_CMDBUF_WORDS = 64;
#define BYTES_TO_WORDS(b) ((b) + 4 - 1) / 4)
#define WORD_SIZE(t) (BYTES_TO_WORDS(sizeof(t)))

// Static assertions
static_assert(WORD_SIZE(Request) <= IPC_CMDBUF_WORDS);
static_assert(WORD_SIZE(Response) <= IPC_CMDBUF_WORDS);

```
Level 2
```c

// prelude.h
// https://stackoverflow.com/a/2124433
#define NUMARGS(...)  (sizeof((int[]){__VA_ARGS__})/sizeof(int))

// https://stackoverflow.com/a/35801454
#define HandleTranslation(n) struct HandleTranslation_##n
#define DefineHandleTranslation(n) (HandleTranslation(n) { u32 descriptor; Handle handles[n] })
DefineHandleTranslation(1);
DefineHandleTranslation(2);
...
DefineHandleTranslation(IPC_CMDBUF_WORDS - 2 /* 1 word for header, 1 word for handle descriptor */);

#define translate_handle(type,n,arr) (static_assert(countof(arr) == n, "Expected ##n handles"), { .descriptor = libctru::handle_descriptor(type, n),  .handles = (Handle[])arr } )

struct ReadTranslation {
	u32 descriptor;
	void *pointer;
}

ReadTranslation translate_read(void *pointer, u32 size) {
	ReadTranslation r;
	r.descriptor = libctru::read_descriptor(size);
	r.pointer = pointer;
	return r;
}

struct WriteTranslation {
	u32 descriptor;
	void *point;
}


struct Request {
	u32 header;
	u32 normal_1;

	ReadTranslation translate_1;
	HandleTranslation(4) translate_2;
	WriteTranslation translate_3;
}

struct Request build_request(u32 normal_1, ReadTranslation translate_1, HandleTranslation(4) handles, WriteTranslation translate_3) {
	Request r;
	r.header = 0x1234abcd;
	r.normal_1 = normal;
	return r;
}

char buf1[];
char buf2[];

enum HandleMoveType {
	Copy,
	Move,
	ProcessID
}

// User code
build_request(
	0x00001111,
	translate_read(&buf1, sizeof(buf1)),
	translate_handles(Copy, 4, {0x1, 0x2, 0x3, 0x4}),
	translate_write(&buf2, sizeof(buf2))
);
```
Output of Level 3
```c
// TODO: Unaligned field accesses. Must not generate code that causes UB
//       Maybe can generate field getters/setters that use memcpy to avoid UB?

// https://www.3dbrew.org/wiki/MCURTC:SetInfoLEDPattern

struct Animation {
	uint8_t delay;
	uint8_t smoothing;
	uint8_t loop_delay;
	uint8_t blink_speed;
};

struct NormalParams {
	struct Animation animation;
	char red_pattern[32];
	char green_pattern[32];
	char blue_pattern[32];
}

struct Request build_request(struct Animation animation, ...) {
	...
}
```
Output of Level 4
```c
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
```

### Rust
[NFCS_SendTagCommand]()
Using my traits from https://github.com/DeltaF1/3ds-ipc-tools

```rust
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

	fn make_request<A, B>(handle: Handle, unknown: u32, input: &A, output: &mut B, timing_value: u8) -> u32 {
		
	}
}
```
