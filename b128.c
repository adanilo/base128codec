/**
 *  Copyright 2013-2016 Alex Danilo
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *	Base 128/192 encoding experiment.
 *
 *	Ths is just a quick experiment to see what sort of speed we get by encoding
 *	a contiguous 128 value range in bytes that replaces base 64 with it's low level
 *	conditionals and bit-bashing for decode.
 *
 *	Ultimately, the encoding I'd like to use is a 192 value encoding which yields
 *	7.5 bits/byte - i.e. approx. 6.66% loss encoding binary data in a text-safe
 *	transfer form. The 128 value coding is just to test the basis of the
 *	speed and compressibility of the result and is easy to use in JS. The 192
 *  value encoding is more efficient but likely a lot trickier to use from JS.
 *
 *	NB: This is using ISO-9959-1 as it's basis. The source code will break if you
 *	try to view as UTF-8.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define	DECODER		"d128"
#define ENCODER 	"e128"
#define	DECODER7_5	"d192"
#define ENCODER7_5 	"e192"

#define BASE 		'#'		/* 0x22 	 */
#define BASEMAX 	'~'		/* 0x7E 	 */
#define UPPERBASE	'\xA0'	/* NBSP 	 */
#define UPPERMAX	'\xFF'	/* char is ÿ */
#define PADCHAR		'!'
#define INBYTES		7
#define OUTBYTES	8
#define BIGINBYTES	15
#define BIGOUTBYTES	16
/*
 *	Decode table specific stuff
 */
#define ZOF_TAB		256
#define	BAD			0xFF
#define PAD 		0xFE	/* Encoded '!' is padding	*/
 
/*
 *	The decode table reverses the encoding back to bits. Note, that since the most we can encode is 7.5 bits, we can never
 *	generate 0xFF in the decode so use that to mark bad encoding bytes.
 */
static const unsigned char	dtab[ZOF_TAB] = 
{
	BAD, BAD, BAD, BAD, BAD, BAD, BAD, BAD, BAD, 187, BAD, 188, BAD, BAD, BAD, BAD,
	BAD, BAD, BAD, BAD, BAD, BAD, BAD, BAD, BAD, BAD, BAD, 189, 190, BAD, BAD, BAD,
	191, PAD, BAD,   0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,
	 13,  14,  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28, 
	 29,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,
	 45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56, BAD,  57,  58,  59,
	 60,  61,  62,  63,  64,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,
	 76,  77,  78,  79,  80,  81,  82,  83,  84,  85,  86,  87,  88,  89,  90, BAD,
	BAD, BAD, BAD, BAD, BAD, BAD, BAD, BAD, BAD, BAD, BAD, BAD, BAD, BAD, BAD, BAD,
	BAD, BAD, BAD, BAD, BAD, BAD, BAD, BAD, BAD, BAD, BAD, BAD, BAD, BAD, BAD, BAD,
	 91,  92,  93,  94,  95,  96,  97,  98,  99, 100, 101, 102, 103, 104, 105, 106,
	107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 
	123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138,
	139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 
	155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 
	171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186,
};

/*
 *	Reserve " (0x3E) as delimiter for easy manipulation in JS, '!' as stuffing character and generation for JS - the ranges 23->7E then A0->FF create the encoding.
 *	That's enough for the base 128 version but we need 4 more characters to encode 7.5 bits/byte so the last set is carefully chosen from within
 *	the control character range using values that are safe for editors.
 */
static const char	*etab = "#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[]^_`abcdefghijklmnopqrstuvwxyz{|}~"
							"\xA0\xA1\xA2\xA3\xA4\xA5\xA6\xA7\xA8\xA9\xAA\xAB\xAC\xAD\xAE\xAF\xB0\xB1\xB2\xB3\xB4\xB5\xB6\xB7\xB8\xB9\xBA\xBB\xBC\xBD\xBE\xBF"
							"\xC0\xC1\xC2\xC3\xC4\xC5\xC6\xC7\xC8\xC9\xCA\xCB\xCC\xCD\xCE\xCF\xD0\xD1\xD2\xD3\xD4\xD5\xD6\xD7\xD8\xD9\xDA\xDB\xDC\xDD\xDE\xDF"
							"\xE0\xE1\xE2\xE3\xE4\xE5\xE6\xE7\xE8\xE9\xEA\xEB\xEC\xED\xEE\xEF\xF0\xF1\xF2\xF3\xF4\xF5\xF6\xF7\xF8\xF9\xFA\xFB\xFC\xFD\xFE\xFF"
							"\t\x0B\x1B\x1C ";

/**
 *	Union for handling the 120 bit division for 7.5 bit encoding
 */
typedef union
{
	__uint128_t		bn_Number;
	unsigned char 	bn_Bytes[16];
} BigNum;

/*
 *	We're encoding 7 bits each byte so 7 input bytes->8 output bytes
 */
static void
encode7(FILE *infile, FILE *outfile)
{
	unsigned char	in[INBYTES];
	unsigned char	topbits, mask;
	int 			i, len;

	while (!feof(infile))
	{
		len = 0;
		for (i = 0; i < INBYTES; i++)
		{
			in[i] = getc(infile);
			if (feof(infile))
				break;
			len++;
		}
		if (len > 0)
		{
			topbits = 0;
			mask = 0x40;
			for (i = 0; i < len; i++)
			{
				putc(etab[in[i] & 0x7F], outfile);
				if (in[i] > 0x7F)
					topbits |= mask;
				mask >>= 1;
			}
			if (len < INBYTES)	/* Need to pad */
			{
				fputc(PADCHAR, outfile);
			}
			fputc(etab[topbits], outfile);
		}
	}
}

static void
decode7(FILE *infile, FILE *outfile)
{
	unsigned char 	in[OUTBYTES];
	unsigned char 	topbits, mask;
	int 			i, len;

	while (!feof(infile))
	{
		len = 0;
		for (i = 0; i < OUTBYTES; i++)
		{
			in[i] = getc(infile);
			if (feof(infile))
				break;
			len++;
		}
		if (len > 0)
		{
			topbits = dtab[in[len - 1]];
			mask = 0x40;
			for (len = i = 0; i < INBYTES; i++)
			{
				if (in[i] == PADCHAR)
					break;
				len++;
				in[i] = dtab[in[i]];
				if (in[i] == BAD)		/* Illegal encoding character, bail out */
					return;
				if (topbits & mask)
					in[i] |= 0x80;
				mask >>= 1;
			}
			for (i = 0; i < len; i++)
			{
				putc(in[i], outfile);
			}
		}
	}
}

/*
 *	Perform 120 bit binary number conversion into base 192 packed as bytes.
 */
static void
base192_encode(unsigned char *out, BigNum *in)
{
	int 		i;
	__uint128_t	val;

	for (i = 0; i < BIGOUTBYTES; i++)
	{
		val = in->bn_Number % 192;
		out[i] = (unsigned char)val;
		in->bn_Number /= 192;
	}
}

/*
 *	Perform base 192 into binary conversion.
 */
static void
base192_decode(BigNum *out, unsigned char *in)
{
	int 		i;

	out->bn_Number = 0;
	for (i = BIGOUTBYTES - 1; i >= 0; i--)
	{
		out->bn_Number *= 192;
		out->bn_Number += in[i];
	}
}

/**
 *	Encode 7.5 bits per output byte.
 *
 *	To do this means doing a base conversion into base 192 then encode the
 *	digits one per byte using the encoding table. We convert 15 bytes to
 *	16 this way. In order to achieve that we need to do division on
 *	a 120 bit number (the 15 bytes of input) to generate the base 192 digits.
 *
 *	NB: This quick test program is assuming little-endian byte order so we
 *	can use the inbuilt 128 bit types for the division. For portability
 *	we should really use something like Knuth's arbitrary precision
 *	division algorithms.
 */
static void
encode7_5(FILE *infile, FILE *outfile)
{
	int 			i, len, pad;
	BigNum			value;
	unsigned char 	out[BIGOUTBYTES];

	while (!feof(infile))
	{
		len = 0;
		for (i = 0; i < BIGINBYTES; i++)
		{
			value.bn_Bytes[i] = getc(infile);
			if (feof(infile))
				break;
			len++;
		}
		if (len > 0)
		{
			pad = BIGINBYTES - i;
			while (i <= BIGINBYTES)	/* Fill MSBs with 0 - NB up to 128 bits, top 8 never used */
			{
				value.bn_Bytes[i++] = 0;
			}
			base192_encode(out, &value);
			len++;					/* Always outputs 1 byte more than the input			*/
			for (i = 0; i < len; i++)
			{
				putc(etab[out[i]], outfile);
			}
			if (pad > 0)	/* Need to mark early end with the pad character */
			{
				fputc(PADCHAR, outfile);
			}
		}
	}
}

static void
decode7_5(FILE *infile, FILE *outfile)
{
	unsigned char 	in[BIGOUTBYTES];
	BigNum			value;
	int 			i, len, pad;

	while (!feof(infile))
	{
		len = pad = 0;
		for (i = 0; i < BIGOUTBYTES; i++)
		{
			in[i] = dtab[getc(infile)];
			if (feof(infile))		/* Shouldn't happen for valid padded content	*/
				break;
			if (in[i] == PAD)		/* End of stream marker 						*/
			{
				in[i] = 0;
				pad++;
				break;
			}
			else if (in[i] == BAD)	/* Bad character in the encoding set 			*/
			{
				return;
			}
			len++;
		}
		if (len > 0)
		{
			while (i < BIGOUTBYTES)	/* Fill MSBs with 0 - NB up to 128 bits, top 8 never used */
			{
				in[i++] = 0;
			}
			base192_decode(&value, in);
			len--;					/* Output always shrinks by one byte 	*/
			for (i = 0; i < len; i++)
			{
				putc(value.bn_Bytes[i], outfile);
			}
		}
	}
}

int
main(int argc, char const *argv[])
{
	const char	*arg0;
	FILE		*infile, *outfile;

	arg0 = strrchr(argv[0], '/');

	if (arg0 == NULL)
		arg0 = argv[0];
	else
		arg0++;

	infile = freopen(NULL, "rb", stdin);
	outfile = freopen(NULL, "wb", stdout);
	if (strcmp(arg0, DECODER) == 0)
		decode7(infile, outfile);
	else if (strcmp(arg0, ENCODER) == 0)
		encode7(infile, outfile);
	else if (strcmp(arg0, DECODER7_5) == 0)
		decode7_5(infile, outfile);
	else if (strcmp(arg0, ENCODER7_5) == 0)
		encode7_5(infile, outfile);
	else
		exit(1);

	return 0;
}