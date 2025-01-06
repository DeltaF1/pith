_The stuff in between all of the CiTRus segments_

***

Pith is an [interface description language (IDL)](https://en.wikipedia.org/wiki/Interface_description_language) to express the APIs of the 3DS' Horizon OS operating system in a standardized and computer-readable format. The language can be used to auto-generate types and methods to use these APIs from languages such as C and Rust.

# Rationale

Familiarity is assumed with the 3DS' [Inter-Process Communication](https://www.3dbrew.org/wiki/IPC) system. This tool will probably be most useful for people looking to:
1. Port new languages and standard libraries to the 3DS
2. Implement libraries or functionality not already covered by [libctru](https://libctru.devkitpro.org/)
3. Re-implement system modules from scratch (see https://gist.github.com/kynex7510/83cec53fd9aaa5dfa1492a2b5fb53813)
4. Add entirely new APIs to use internally in their homebrew applications, or add new functionality to existing system modules

The [3dbrew](https://www.3dbrew.org/) wiki is an incredible resource with detailed descriptions of (nearly?) every service included in the 3DS operating system. Each wiki page for a specific IPC call has tables detailing the data required to call an API and the data that API returns. Unfortunately, this information has been written by multiple authors across a long period of time, and its content and formatting varies accordingly. This information is useful for a human looking to learn about or implement these services, but it is not machine-readable. Expressing this information in a standard way allows automated tools to generate types and wrapper code and present documentation in any desired format.

Converting all of the human-written human-readable wiki tables to a new format is a daunting task. There are certain patterns that have emerged for how to write these tables which I believe make the work semi-automatable. For example many wiki pages explicitly spell out the code necessary to create translate headers like `0x0000000c | (size << 4)`. This could be detected and automatically turned into a WriteBuffer field. The work I've seen recently to convert the tables to use uniform wiki templates will also help tremendously. An ideal system would involve some sort of browser extension which can spider through the wiki, attempt to parse as it goes, then prompt the human user for help when it does not know how to interpret something. As a fall-back, entries in a table can be represented as simple u32s/words with the full wiki description added as an [attribute](#Attributes) like `u32 word_xxx [wikitext="..."];`

After this ingestion process, the language can be turned around and used to generate documentation like the wiki pages programatically. Ideally the tooling can reproduce the tables on the wiki to a degree that does not lose any information. Preserving comments and explanatory text is useful if those semantics cannot be easily represented in the IDL. See [Attributes](#Attributes)

# Syntax

```
// SERVICE_MethodName.pith

struct SERVICE:SomeStructType 
	// Syntax for metadata TBD
	wikiurl = "https://3dbrew.org/wiki/SERVICE#SomeStructType"

	f32 x;
	f32 y;
	f32 z;
}

enum SERVICE:SomeEnumType /* Syntax for metadata TBD */ [wikiurl = "https://3dbrew.org/wiki/SERVICE#SomeEnumType"] {
	FOO = 0,
	BAR,
	BAZ
}

command SERVICE:MethodName {
	// Syntax for metadata TBD
	wikiurl = "https://www.3dbrew.org/wiki/SERVICE:MethodName"
	command_id = 0x1234

	Request {
		Header header;
		u32 some_normal_param [name = "Some normal parameter"];
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

		u32 normal_param [comment = "..."];
		
		StaticBuffer@1 translated_param;
		MoveHandles[1] translated_param_2;
	}

	// Syntax TBD
	extra_wiki_information = "..."
}
```

A pith file contains one or more definitions. Each definition can be a [`command`](#commands), [`struct`](#structs), or [`enum`](#enums). Definition names are namespaced by service and use single colons (`:`) as namespace separators.

## Commands
The most common type of definition are commands, which define the normal and translated parameters for the request and response associated with an IPC command. Each Request and Response definition inside of a command contain a series of fields. The layout of these fields is discussed in [struct layout](#struct-layout).

Fields are declared with a syntax similar to C struct declarations.
```
field_type field_name;
```
Note that the type and name are separated by a space, even for array and pointer types. For example the following C struct definition

```c
struct Foo {
	char name[8];
	int *ptr;
}
```
Would be written as
```
struct SRV:Foo {
	u8[8] name;
	int* ptr;
}
```

TODO: If this is a big issue for the C developers then it could be changed to be more like C but I think this is simpler. Alternatively the syntax could be made more like Rust to avoid confusion switching between pith and C.

Fields can optionally be declared with metadata called [attributes](#attributes).

## Structs

The syntax for struct fields is the same as Request/Response fields:

```
struct SERVICE:StructType {
	field_type field_name [attributes];
}
```

Structs don't have to be associated with a specific service, since some could be used across services.

## Enums

TODO

## Attributes

Attributes are key-value pairs with metadata pertaining to a specific field. Their presence is optional. The syntax is 

```
field_type field_name [key1 = value1, key2 = value2, ...];
```

Keys are `identifier`s with no whitespace (TODO: formal syntax grammar). Values are strings, delimited by `"`. Internal `"` characters can be typed as `\"`. Whitespace around the `,` and `=` are optional.

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


TODO: If the parser notices `;` followed by `[` then suggest to move the attribute block before the `;`. 

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
| TODO: PXI buffer translations                                                                                              |                             |                   |              |                                                                                                                                                     |

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
Requests and Responses are laid out according to the same algorithm as structs. The algorithm is the same as in C, but all fields are word-aligned which is to say: Fields are laid out in the order they are written. Each field is word-aligned and starts at the closest word-aligned address after the previous field. For types smaller than a word this requires wrapper types in C/Rust/etc. that look like so

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

In this example the request consists of 3 words. `param_2`, `param_3`, and `param_4` are all packed into the second word, with an additional inaccessible padding byte.

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

// Some made-up context where they need to be laid out in a particular way
struct foo {
	PSPXI:Algorithm<u16>[4] algorithm_list;
}
```

Also open to just defining the representation type as part of the enum definition.

# Future work

An important feature of this IDL would be the ability to relate multiple parameters in a command to one another and to input variables. For example many APIs send translated buffers using the proper descriptor pair, but send the size of the buffer again in a separate normal parameter. This pattern could be expressed by the IDL so that generated wrapper methods only ask for the size of the buffer once and use it in all appropriate locations. A previous version of this document contained an incomplete proposal for a small expression language to describe the value as well as type of command parameters. A more coherent version will be proposed in future.

[^u64_align]: Armv6 supports 4-byte aligned accesses for 64-bit words[^1][^2][^3]. Unfortunately the ABI for ARM (as well as modern compilers like devkitpro's gcc and rustc) define the alignment of u64 as 8-bytes on ARMv6k. For this reason wrappers must take care to only use `memcpy` to read these values in C. A smart compiler will eliminate/inline the memcpy call anyway so it shouldn't matter for performance.

[^1]:https://developer.apple.com/documentation/xcode/writing-armv6-code-for-ios
[^2]:https://developer.arm.com/documentation/ddi0419/c/Application-Level-Architecture/Application-Level-Programmers--Model/ARM-processor-data-types-and-arithmetic
[^3]: https://www.scs.stanford.edu/~zyedidia/docs/arm/armv6.pdf

